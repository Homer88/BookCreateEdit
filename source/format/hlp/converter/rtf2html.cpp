#include "rtf2html.h"

#include <QByteArray>
#include <QChar>
#include <QColor>
#include <QList>
#include <QTextCodec>

namespace {

// Кодовая страница из \ansicpg (default - Windows-1252).
QTextCodec *codecFor(const QString &rtf)
{
    int cpg = 0;
    const int pos = rtf.indexOf(QLatin1String("\\ansicpg"));
    if (pos >= 0)
    {
        int end = pos + 8;
        while (end < rtf.size() && rtf.at(end).isDigit())
            ++end;
        cpg = rtf.mid(pos + 8, end - pos - 8).toInt();
    }
    switch (cpg)
    {
    case 1251: return QTextCodec::codecForName("Windows-1251");
    case 932:  return QTextCodec::codecForName("Shift_JIS");
    case 950:  return QTextCodec::codecForName("Big5");
    case 936:  return QTextCodec::codecForName("GB18030");
    case 949:  return QTextCodec::codecForName("EUC-KR");
    case 1252:
    default:   return QTextCodec::codecForName("Windows-1252");
    }
}

// Скобка, экранированная обратным слэшем (\{ или \}), не является границей
// группы RTF. Число обратных слэшей перед ней определяет экранирование.
bool isEscapedBrace(const QString &s, int idx)
{
    int b = 0;
    for (int k = idx - 1; k >= 0 && s.at(k) == QLatin1Char('\\'); --k)
        ++b;
    return (b % 2) == 1;
}

int findGroupClose(const QString &s, int i)
{
    int depth = 0;
    for (int k = i; k < s.size(); ++k)
    {
        const QChar c = s.at(k);
        if (c == QLatin1Char('{'))
        {
            if (!isEscapedBrace(s, k))
                ++depth;
        }
        else if (c == QLatin1Char('}'))
        {
            if (isEscapedBrace(s, k))
                continue;
            --depth;
            if (depth == 0)
                return k;
        }
    }
    return s.size();
}

// RTF-фрагмент -> обычный текст (для значений сносок).
QString plainText(const QString &frag, QTextCodec *codec)
{
    QString out;
    const int n = frag.size();
    int i = 0;
    while (i < n)
    {
        const QChar c = frag.at(i);
        if (c == QLatin1Char('{'))
        {
            i = findGroupClose(frag, i) + 1;
            continue;
        }
        if (c == QLatin1Char('}'))
        {
            ++i;
            continue;
        }
        if (c == QLatin1Char('\\'))
        {
            ++i;
            if (i >= n)
                break;
            const QChar sym = frag.at(i);
            if (sym.isLetter())
            {
                QString word;
                while (i < n && frag.at(i).isLetter())
                    word += frag.at(i++);
                QString param;
                if (i < n && (frag.at(i) == QLatin1Char('-') || frag.at(i) == QLatin1Char('+')))
                    param += frag.at(i++);
                while (i < n && frag.at(i).isDigit())
                    param += frag.at(i++);
                if (i < n && frag.at(i) == QLatin1Char(' '))
                    ++i;
                if (word == QLatin1String("tab"))
                    out += QLatin1Char(' ');
                else if (word == QLatin1String("u"))
                {
                    bool ok = false;
                    const int code = param.toInt(&ok);
                    if (ok)
                        out += QChar(code);
                }
                continue;
            }
            ++i;
            if (sym == QLatin1Char('\''))
            {
                bool ok = false;
                const int v = frag.mid(i, 2).toInt(&ok, 16);
                if (ok)
                {
                    out += codec->toUnicode(QByteArray(1, static_cast<char>(v))).at(0);
                    i += 2;
                }
            }
            else if (sym == QLatin1Char('~'))
                out += QLatin1Char(' ');
            continue;
        }
        out += c;
        ++i;
    }
    return out.simplified();
}

QStringList parseFonts(const QString &src)
{
    QStringList out;
    const int p = src.indexOf(QLatin1String("\\fonttbl"));
    if (p < 0)
        return out;
    const int open = src.lastIndexOf(QLatin1Char('{'), p);
    if (open < 0)
        return out;
    const int close = findGroupClose(src, open);
    const QString inner = src.mid(open + 1, close - open - 1);

    int i = 0;
    while (i < inner.size())
    {
        const int f = inner.indexOf(QLatin1String("\\f"), i);
        if (f < 0)
            break;
        int k = f + 2;
        QString idx;
        while (k < inner.size() && inner.at(k).isDigit())
            idx += inner.at(k++);
        // после \fN идут атрибуты шрифта (\fswiss, \froman, \fmodern...) -
        // пропускаем их до собственно имени семейства
        while (k < inner.size() && inner.at(k) == QLatin1Char('\\'))
        {
            ++k;
            while (k < inner.size() && inner.at(k).isLetter())
                ++k;
            if (k < inner.size() && (inner.at(k) == QLatin1Char('-') || inner.at(k) == QLatin1Char('+')))
                ++k;
            while (k < inner.size() && inner.at(k).isDigit())
                ++k;
            while (k < inner.size() && inner.at(k) == QLatin1Char(' '))
                ++k;
        }
        const int semi = inner.indexOf(QLatin1Char(';'), k);
        const QString fam = inner.mid(k, semi < 0 ? inner.size() - k : semi - k).simplified();
        const int fi = idx.toInt();
        while (out.size() <= fi)
            out << QString();
        if (!fam.isEmpty())
            out[fi] = fam;
        i = semi < 0 ? inner.size() : semi + 1;
    }
    return out;
}

QList<QRgb> parseColors(const QString &src)
{
    QList<QRgb> out;
    out << 0; // индекс 0 = авто
    const int p = src.indexOf(QLatin1String("\\colortbl"));
    if (p < 0)
        return out;
    const int open = src.lastIndexOf(QLatin1Char('{'), p);
    if (open < 0)
        return out;
    const int close = findGroupClose(src, open);
    const QString inner = src.mid(open + 1, close - open - 1);

    const QStringList entries = inner.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &e : entries)
    {
        int r = 0, g = 0, b = 0;
        const int pr = e.indexOf(QLatin1String("\\red"));
        if (pr >= 0)
            r = e.mid(pr + 4).split(QLatin1Char('\\')).value(0).toInt();
        const int pg = e.indexOf(QLatin1String("\\green"));
        if (pg >= 0)
            g = e.mid(pg + 6).split(QLatin1Char('\\')).value(0).toInt();
        const int pb = e.indexOf(QLatin1String("\\blue"));
        if (pb >= 0)
            b = e.mid(pb + 5).split(QLatin1Char('\\')).value(0).toInt();
        out << qRgb(r, g, b);
    }
    return out;
}

QString esc(const QString &t)
{
    QString o = t;
    o.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    o.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    o.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    o.replace(QLatin1Char('"'), QLatin1String("&quot;"));
    return o;
}

QString px(int twips)
{
    return QString::number(qRound(twips / 15.0));
}

struct Fmt
{
    bool bold = false;
    bool italic = false;
    bool ul = false;
    bool strike = false;
    bool scaps = false;
    int fs = 0; // полупункты
    int f = -1;
    int cf = 0;
};

struct Seg
{
    enum Kind { Text, Br, Image } kind = Text;
    QString text;
    QString img;
    QString imgStyle;
    QString link;
    Fmt fmt;
};

struct ParaStyle
{
    int li = 0, ri = 0, fi = 0, sb = 0, sa = 0, sl = 0;
    QString align;
};

// Windows Help кодирует спецсимволы и греческие буквы шрифтом Symbol
// (кодировка Symbol charset). Базируем их на Unicode, чтобы текст был
// читабелен независимо от наличия шрифта Symbol на системе.
static QChar symbolMap(QChar ch)
{
    const int v = ch.unicode();
    if (v > 0xFF)
        return ch;
    switch (v)
    {
    case 0x22: return QChar(0x2200); // " -> forall
    case 0x24: return QChar(0x2203); // $ -> exists
    case 0x27: return QChar(0x220D); // ' -> such that
    case 0x2D: return QChar(0x2212); // - -> minus
    case 0x40: return QChar(0x2245); // @ -> congruent
    case 0x5C: return QChar(0x2234); // \ -> therefore
    case 0x5E: return QChar(0x22A5); // ^ -> perpendicular
    case 0x60: return QChar(0x2310); // ` -> angle (reversed not)
    case 0x7E: return QChar(0x223C); // ~ -> tilde operator
    case 0xA1: return QChar(0x03D2); // upsilon
    case 0xA2: return QChar(0x2032); // prime
    case 0xA3: return QChar(0x2264); // <=
    case 0xA4: return QChar(0x2044); // fraction slash
    case 0xA5: return QChar(0x221E); // infinity
    case 0xA6: return QChar(0x0192); // florin
    case 0xA7: return QChar(0x2663); // club
    case 0xA8: return QChar(0x2666); // diamond
    case 0xA9: return QChar(0x2665); // heart
    case 0xAA: return QChar(0x2660); // spade
    case 0xAB: return QChar(0x2194); // <-> arrow
    case 0xAC: return QChar(0x2190); // <- arrow
    case 0xAD: return QChar(0x2191); // up arrow
    case 0xAE: return QChar(0x2192); // -> arrow
    case 0xAF: return QChar(0x2193); // down arrow
    case 0xB2: return QChar(0x2033); // double prime
    case 0xB3: return QChar(0x2265); // >=
    case 0xB4: return QChar(0x00D7); // multiply
    case 0xB5: return QChar(0x221D); // proportional
    case 0xB6: return QChar(0x2202); // partial differential
    case 0xB7: return QChar(0x2022); // bullet
    case 0xB8: return QChar(0x00F7); // divide
    case 0xB9: return QChar(0x2260); // !=
    case 0xBA: return QChar(0x2261); // identical
    case 0xBB: return QChar(0x2248); // approx
    case 0xBC: return QChar(0x2026); // ellipsis
    case 0xBD: return QChar(0x23AE); // vertical line extension
    case 0xBE: return QChar(0x2015); // horizontal bar (em-dash-like)
    case 0xBF: return QChar(0x21B5); // down arrow with corner
    case 0xDB: return QChar(0x2234); // therefore
    case 0xDC: return QChar(0x22A5); // perpendicular
    case 0xDD: return QChar(0x005F); // underscore
    case 0xDE: return QChar(0x2310); // angle
    case 0xDF: return QChar(0x2320); // top half integral
    case 0xE1: return QChar(0x222B); // integral
    case 0xE3: return QChar(0x2321); // bottom half integral
    case 0xE4: return QChar(0x2320); // top half integral
    case 0xE5: return QChar(0x23AE); // vertical line extension
    default:
        if (v >= 0x41 && v <= 0x5A)
        {
            // греческие заглавные (A-Z в кодировке Symbol)
            static const ushort greekCap[26] = {
                0x0391,0x0392,0x03A7,0x0394,0x0395,0x03A6,0x0393,0x0397,
                0x0399,0x03D1,0x039A,0x039B,0x039C,0x039D,0x039F,0x03A0,
                0x0398,0x03A1,0x03A3,0x03A4,0x03A5,0x03C2,0x03A9,0x039E,
                0x03A8,0x0396
            };
            return QChar(greekCap[v - 0x41]);
        }
        if (v >= 0x61 && v <= 0x7A)
        {
            // греческие строчные (a-z в кодировке Symbol)
            static const ushort greekLow[26] = {
                0x03B1,0x03B2,0x03C7,0x03B4,0x03B5,0x03C6,0x03B3,0x03B7,
                0x03B9,0x03D5,0x03BA,0x03BB,0x03BC,0x03BD,0x03BF,0x03C0,
                0x03B8,0x03C1,0x03C3,0x03C4,0x03C5,0x03D6,0x03C9,0x03BE,
                0x03C8,0x03B6
            };
            return QChar(greekLow[v - 0x61]);
        }
        if (v >= 0xC0 && v <= 0xD9)
        {
            static const ushort greekCap2[26] = {
                0x0391,0x0392,0x03A7,0x0394,0x0395,0x03A6,0x0393,0x0397,
                0x0399,0x03D1,0x039A,0x039B,0x039C,0x039D,0x039F,0x03A0,
                0x0398,0x03A1,0x03A3,0x03A4,0x03A5,0x03C2,0x03A9,0x039E,
                0x03A8,0x0396
            };
            return QChar(greekCap2[v - 0xC0]);
        }
        return ch;
    }
}

class Converter
{
public:
    Converter(const QString &topic, QTextCodec *c,
              const QStringList &fontList, const QList<QRgb> &colorList,
              Rtf::Meta *m)
        : s(topic), codec(c), fonts(fontList), colors(colorList), meta(m)
    {}

    QString convert()
    {
        const int n = s.size();
        int i = 0;
        while (i < n)
        {
            const QChar c = s.at(i);
            if (c == QLatin1Char('{'))
            {
                handleGroup(i);
                continue;
            }
            if (c == QLatin1Char('}'))
            {
                ++i;
                continue;
            }
            if (c == QLatin1Char('\\'))
            {
                handleControl(i);
                continue;
            }
            addText(c);
            ++i;
        }
        flush();
        renderParagraph();
        return out;
    }

private:
    const QString &s;
    QTextCodec *codec;
    const QStringList &fonts;
    const QList<QRgb> &colors;
    Rtf::Meta *meta;

    QList<Seg> segs;
    QString m_cur;
    Fmt fmt;
    ParaStyle para;
    int linkStart = -1;
    QString pendingMarker;
    QString out;
    bool fmtDirty = true; // формат изменился с момента последнего flush()

    void flush()
    {
        if (m_cur.isEmpty())
            return;
        Seg sg;
        sg.kind = Seg::Text;
        sg.text = m_cur;
        sg.fmt = fmt;
        segs << sg;
        m_cur.clear();
        fmtDirty = false;
    }

    void addText(const QChar &ch)
    {
        // переводы строк в исходнике RTF - это форматирование, а не текст
        if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r'))
            return;
        // формат применяется к символам в момент их появления, а не к моменту
        // flush(): иначе "\f2 \'be\plain" красит весь предыдущий текст
        if (fmtDirty)
        {
            flush();
            fmtDirty = false;
        }
        if (linkStart < 0)
            linkStart = segs.size();
        if (isSymbolFont())
            m_cur += symbolMap(ch);
        else
            m_cur += ch;
    }

    bool isSymbolFont() const
    {
        if (fmt.f < 0 || fmt.f >= fonts.size())
            return false;
        const QString fam = fonts.at(fmt.f);
        return fam.compare(QLatin1String("Symbol"), Qt::CaseInsensitive) == 0
            || fam.endsWith(QLatin1String("Symbol"), Qt::CaseInsensitive);
    }

    void emitImage(const QString &filename, const QString &style)
    {
        flush();
        Seg sg;
        sg.kind = Seg::Image;
        sg.img = filename.simplified();
        sg.imgStyle = style;
        segs << sg;
        linkStart = -1;
    }

    void handleGroup(int &i)
    {
        const int n = s.size();
        const int open = i;
        int j = i + 1;
        if (j < n && s.at(j) == QLatin1Char('\\'))
        {
            int k = j + 1;
            if (k < n && s.at(k) == QLatin1Char('*'))
            {
                i = findGroupClose(s, open) + 1;
                return;
            }
            QString w;
            while (k < n && s.at(k).isLetter())
                w += s.at(k++);

            if (w == QLatin1String("v"))
            {
                int p = k;
                while (p < n && s.at(p) == QLatin1Char(' '))
                    ++p;
                int q = p;
                QString tgt;
                while (q < n && s.at(q) != QLatin1Char('}'))
                    tgt += s.at(q++);
                flush();
                tgt = tgt.simplified();
                // "!macro..." - кнопка с макросом, а не переход по контексту
                const bool isButton = tgt.startsWith(QLatin1Char('!'));
                if (linkStart >= 0 && !tgt.isEmpty() && !isButton)
                {
                    for (int idx = linkStart; idx < segs.size(); ++idx)
                        segs[idx].link = tgt;
                }
                linkStart = -1;
                i = q < n ? q + 1 : q;
                return;
            }
            if (w == QLatin1String("footnote"))
            {
                const int close = findGroupClose(s, open);
                if (!pendingMarker.isEmpty() && meta)
                {
                    const QString inner = s.mid(open + 1, close - open - 1);
                    const QString content = plainText(inner, codec);
                    if (pendingMarker == QLatin1Char('$'))
                        meta->title = content;
                    else if (pendingMarker == QLatin1Char('#'))
                        meta->anchors.append(content);
                    pendingMarker.clear();
                }
                i = close < n ? close + 1 : close;
                return;
            }
            if (w == QLatin1String("up") || w == QLatin1String("dn"))
            {
                // сноска-маркер: {\up X}, X - одиночный символ (+ $ # K ! ...)
                int p = k;
                while (p < n && s.at(p) == QLatin1Char(' '))
                    ++p;
                while (p < n && s.at(p).isDigit())
                    ++p;
                while (p < n && s.at(p) == QLatin1Char(' '))
                    ++p;
                if (p < n && s.at(p) != QLatin1Char('}'))
                {
                    pendingMarker = s.at(p);
                    i = p + 1;
                    if (i < n && s.at(i) == QLatin1Char('}'))
                        ++i;
                    return;
                }
            }
            if (w == QLatin1String("bmc") || w == QLatin1String("bml")
                || w == QLatin1String("bmr") || w == QLatin1String("bmct"))
            {
                const int close = findGroupClose(s, open);
                const QString inner = s.mid(open + 1, close - open - 1);
                const QString style = w == QLatin1String("bmc")
                    ? QStringLiteral("display:block;margin-left:auto;margin-right:auto;")
                    : (w == QLatin1String("bml") ? QStringLiteral("float:left;")
                                                 : QStringLiteral("float:right;"));
                emitImage(inner.mid(3), style);
                i = close < n ? close + 1 : close;
                return;
            }
            static const QStringList skip = {
                "fonttbl", "colortbl", "stylesheet", "info", "header",
                "footer", "field", "pict", "revtbl", "generator"
            };
            if (skip.contains(w))
            {
                i = findGroupClose(s, open) + 1;
                return;
            }
        }
        pendingMarker.clear();
        ++i; // обычная группа: входим и разбираем содержимое
    }

    void handleControl(int &i)
    {
        const int n = s.size();
        ++i; // за '\'
        if (i >= n)
            return;
        const QChar sym = s.at(i);
        if (!sym.isLetter())
        {
            ++i;
            if (sym == QLatin1Char('{'))
            {
                // возможно, экранированное изображение \{bmc bm0.wmf\}
                int k = i;
                QString w;
                while (k < n && s.at(k).isLetter())
                    w += s.at(k++);
                if (w == QLatin1String("bmc") || w == QLatin1String("bml")
                    || w == QLatin1String("bmr") || w == QLatin1String("bmct"))
                {
                    int p = k;
                    while (p < n && s.at(p) == QLatin1Char(' '))
                        ++p;
                    QString fn;
                    while (p + 1 < n && !(s.at(p) == QLatin1Char('\\') && s.at(p + 1) == QLatin1Char('}')))
                        fn += s.at(p++);
                    const QString style = w == QLatin1String("bmc")
                        ? QStringLiteral("display:block;margin-left:auto;margin-right:auto;")
                        : (w == QLatin1String("bml") ? QStringLiteral("float:left;")
                                                     : QStringLiteral("float:right;"));
                    emitImage(fn, style);
                    i = p + 2; // за \}
                    return;
                }
                addText(QLatin1Char('{'));
                return;
            }
            if (sym == QLatin1Char('}'))
            {
                addText(QLatin1Char('}'));
                return;
            }
            if (sym == QLatin1Char('\\'))
            {
                addText(QLatin1Char('\\'));
                return;
            }
            if (sym == QLatin1Char('\''))
            {
                bool ok = false;
                const int v = s.mid(i, 2).toInt(&ok, 16);
                if (ok)
                {
                    addText(codec->toUnicode(QByteArray(1, static_cast<char>(v))).at(0));
                    i += 2;
                }
                return;
            }
            if (sym == QLatin1Char('~'))
            {
                addText(QChar(0xA0));
                return;
            }
            if (sym == QLatin1Char('_'))
            {
                addText(QLatin1Char('-'));
                return;
            }
            // '-', '*', ':' и пр. - игнорируем
            return;
        }

        QString word;
        while (i < n && s.at(i).isLetter())
            word += s.at(i++);
        QString param;
        if (i < n && (s.at(i) == QLatin1Char('-') || s.at(i) == QLatin1Char('+')))
            param += s.at(i++);
        while (i < n && s.at(i).isDigit())
            param += s.at(i++);
        if (i < n && s.at(i) == QLatin1Char(' '))
            ++i;

        if (word == QLatin1String("b") || word == QLatin1String("i")
            || word == QLatin1String("ul") || word == QLatin1String("uldb")
            || word == QLatin1String("ulw") || word == QLatin1String("uld")
            || word == QLatin1String("ulnone") || word == QLatin1String("strike")
            || word == QLatin1String("scaps"))
        {
            const bool on = word == QLatin1String("ulnone")
                ? false
                : (param.isEmpty() ? true : param.toInt() != 0);
            if (word == QLatin1String("b"))
                fmt.bold = on;
            else if (word == QLatin1String("i"))
                fmt.italic = on;
            else if (word == QLatin1String("strike"))
                fmt.strike = on;
            else if (word == QLatin1String("scaps"))
                fmt.scaps = on;
            else
                fmt.ul = on;
            fmtDirty = true;
            return;
        }
        if (word == QLatin1String("fs"))
        {
            fmt.fs = param.toInt();
            fmtDirty = true;
            return;
        }
        if (word == QLatin1String("f"))
        {
            fmt.f = param.toInt();
            fmtDirty = true;
            return;
        }
        if (word == QLatin1String("cf"))
        {
            fmt.cf = param.toInt();
            fmtDirty = true;
            return;
        }
        if (word == QLatin1String("plain"))
        {
            flush();
            fmt = Fmt();
            fmtDirty = true;
            return;
        }
        if (word == QLatin1String("pard"))
        {
            flush();
            renderParagraph();
            para = ParaStyle();
            fmt = Fmt();
            fmtDirty = true;
            linkStart = -1;
            return;
        }
        if (word == QLatin1String("par") || word == QLatin1String("page"))
        {
            flush();
            renderParagraph();
            fmt = Fmt();
            fmtDirty = true;
            linkStart = -1;
            return;
        }
        if (word == QLatin1String("line"))
        {
            flush();
            Seg b;
            b.kind = Seg::Br;
            segs << b;
            linkStart = -1;
            return;
        }
        if (word == QLatin1String("tab"))
        {
            flush();
            m_cur += QChar(0xA0);
            linkStart = -1;
            return;
        }
        if (word == QLatin1String("li"))
        {
            para.li = param.toInt();
            return;
        }
        if (word == QLatin1String("ri"))
        {
            para.ri = param.toInt();
            return;
        }
        if (word == QLatin1String("fi"))
        {
            para.fi = param.toInt();
            return;
        }
        if (word == QLatin1String("sb"))
        {
            para.sb = param.toInt();
            return;
        }
        if (word == QLatin1String("sa"))
        {
            para.sa = param.toInt();
            return;
        }
        if (word == QLatin1String("sl"))
        {
            para.sl = param.toInt();
            return;
        }
        if (word == QLatin1String("qc"))
        {
            para.align = QStringLiteral("center");
            return;
        }
        if (word == QLatin1String("ql"))
        {
            para.align = QStringLiteral("left");
            return;
        }
        if (word == QLatin1String("qr"))
        {
            para.align = QStringLiteral("right");
            return;
        }
        if (word == QLatin1String("qj"))
        {
            para.align = QStringLiteral("justify");
            return;
        }
        if (word == QLatin1String("u"))
        {
            bool ok = false;
            const int code = param.toInt(&ok);
            if (ok)
                addText(QChar(code));
            if (i + 3 < n && s.at(i) == QLatin1Char('\\') && s.at(i + 1) == QLatin1Char('\''))
                i += 4; // пропустить fallback \'xx
            return;
        }
        // прочие слова (keepn, s, deff, ansi, tx, shading...) игнорируем
    }

    static QString styleOf(const Fmt &f, const QStringList &fonts, const QList<QRgb> &colors)
    {
        QString st;
        if (f.bold)
            st += QLatin1String("font-weight:bold;");
        if (f.italic)
            st += QLatin1String("font-style:italic;");
        if (f.ul || f.strike)
        {
            QString td;
            if (f.ul)
                td += QLatin1String("underline ");
            if (f.strike)
                td += QLatin1String("line-through");
            st += QStringLiteral("text-decoration:%1;").arg(td.trimmed());
        }
        if (f.scaps)
            st += QLatin1String("font-variant:small-caps;");
        if (f.fs)
            st += QStringLiteral("font-size:%1em;").arg(QString::number(f.fs / 20.0, 'g', 4));
        if (f.f >= 0 && f.f < fonts.size() && !fonts.at(f.f).isEmpty()
            && fonts.at(f.f).compare(QLatin1String("Symbol"), Qt::CaseInsensitive) != 0)
            st += QStringLiteral("font-family:'%1';").arg(esc(fonts.at(f.f)));
        if (f.cf > 0 && f.cf < colors.size())
            st += QStringLiteral("color:%1;").arg(QColor(colors.at(f.cf)).name());
        return st;
    }

    void renderParagraph()
    {
        if (segs.isEmpty())
            return;
        QString body;
        QString prevLink;
        bool hasContent = false;
        for (const Seg &sg : segs)
        {
            if (sg.kind == Seg::Br)
            {
                body += QLatin1String("<br/>");
                hasContent = true;
                continue;
            }
            if (sg.kind == Seg::Image)
            {
                hasContent = true;
                body += QStringLiteral("<img src=\"%1\" border=\"0\" style=\"max-width:100%;%2\"/>")
                            .arg(esc(sg.img), sg.imgStyle);
                continue;
            }
            if (sg.kind == Seg::Text && !sg.text.trimmed().isEmpty())
                hasContent = true;
            if (sg.link != prevLink)
            {
                if (!prevLink.isEmpty())
                    body += QLatin1String("</a>");
                if (!sg.link.isEmpty())
                    body += QStringLiteral("<a href=\"#%1\">").arg(esc(sg.link));
                prevLink = sg.link;
            }
            const QString st = styleOf(sg.fmt, fonts, colors);
            if (st.isEmpty())
                body += esc(sg.text);
            else
                body += QStringLiteral("<span style=\"%1\">%2</span>").arg(st, esc(sg.text));
        }
        if (!prevLink.isEmpty())
            body += QLatin1String("</a>");
        if (!hasContent)
        {
            segs.clear();
            return;
        }

        QString style;
        if (para.li)
            style += QStringLiteral("margin-left:%1px;").arg(px(para.li));
        if (para.ri)
            style += QStringLiteral("margin-right:%1px;").arg(px(para.ri));
        if (para.fi)
            style += QStringLiteral("text-indent:%1px;").arg(px(para.fi));
        if (para.sb)
            style += QStringLiteral("margin-top:%1px;").arg(px(para.sb));
        if (para.sa)
            style += QStringLiteral("margin-bottom:%1px;").arg(px(para.sa));
        if (para.sl)
            style += QStringLiteral("line-height:%1em;").arg(QString::number(qAbs(para.sl) / 200.0, 'g', 4));
        if (!para.align.isEmpty())
            style += QStringLiteral("text-align:%1;").arg(para.align);

        if (style.isEmpty())
            out += QLatin1String("<p>") + body + QLatin1String("</p>\n");
        else
            out += QStringLiteral("<p style=\"%1\">%2</p>\n").arg(style, body);
        segs.clear();
    }
};

} // namespace

namespace Rtf {

QString toHtml(const QString &rtfTopic, const QString &rtfAll, Meta *meta)
{
    const QString &src = rtfAll.isEmpty() ? rtfTopic : rtfAll;
    QTextCodec *codec = codecFor(src);
    const QStringList fonts = parseFonts(src);
    const QList<QRgb> colors = parseColors(src);
    Converter cv(rtfTopic, codec, fonts, colors, meta);
    return cv.convert();
}

QStringList splitTopics(const QString &rtf)
{
    QStringList topics;
    int start = 0;
    int depth = 0;

    for (int i = 0; i + 4 < rtf.size(); ++i)
    {
        const QChar c = rtf.at(i);
        if (c == QLatin1Char('{'))
        {
            if (!isEscapedBrace(rtf, i))
                ++depth;
        }
        else if (c == QLatin1Char('}'))
        {
            if (!isEscapedBrace(rtf, i))
                --depth;
        }
        else if (depth == 1 && c == QLatin1Char('\\') && rtf.midRef(i, 5) == QLatin1String("\\page"))
        {
            topics.append(rtf.mid(start, i - start));
            start = i + 5;
            i += 4;
        }
    }
    if (start < rtf.size())
        topics.append(rtf.mid(start));

    return topics;
}

QString headingOf(const QString &rtfTopic)
{
    QTextCodec *codec = codecFor(rtfTopic);
    QString out;
    const int n = rtfTopic.size();
    int i = 0;
    while (i < n)
    {
        const QChar c = rtfTopic.at(i);
        if (c == QLatin1Char('{'))
        {
            int j = i + 1;
            QString w;
            if (j < n && rtfTopic.at(j) == QLatin1Char('\\'))
            {
                int k = j + 1;
                while (k < n && rtfTopic.at(k).isLetter())
                    w += rtfTopic.at(k++);
            }
            static const QStringList skip = {
                "fonttbl", "colortbl", "stylesheet", "info", "footnote",
                "header", "footer", "pict"
            };
            if (w == QLatin1String("footnote"))
            {
                i = findGroupClose(rtfTopic, i) + 1;
                continue;
            }
            if (skip.contains(w))
            {
                i = findGroupClose(rtfTopic, i) + 1;
                continue;
            }
            i = findGroupClose(rtfTopic, i) + 1;
            continue;
        }
        if (c == QLatin1Char('}'))
        {
            ++i;
            continue;
        }
        if (c == QLatin1Char('\\'))
        {
            ++i;
            if (i >= n)
                break;
            const QChar sym = rtfTopic.at(i);
            if (sym.isLetter())
            {
                QString word;
                while (i < n && rtfTopic.at(i).isLetter())
                    word += rtfTopic.at(i++);
                QString param;
                if (i < n && (rtfTopic.at(i) == QLatin1Char('-') || rtfTopic.at(i) == QLatin1Char('+')))
                    param += rtfTopic.at(i++);
                while (i < n && rtfTopic.at(i).isDigit())
                    param += rtfTopic.at(i++);
                if (i < n && rtfTopic.at(i) == QLatin1Char(' '))
                    ++i;
                if (word == QLatin1String("par") || word == QLatin1String("line")
                    || word == QLatin1String("page"))
                    break;
                if (word == QLatin1String("tab"))
                    out += QLatin1Char(' ');
                else if (word == QLatin1String("u"))
                {
                    bool ok = false;
                    const int code = param.toInt(&ok);
                    if (ok)
                        out += QChar(code);
                }
                continue;
            }
            ++i;
            if (sym == QLatin1Char('\''))
            {
                bool ok = false;
                const int v = rtfTopic.mid(i, 2).toInt(&ok, 16);
                if (ok)
                {
                    out += codec->toUnicode(QByteArray(1, static_cast<char>(v))).at(0);
                    i += 2;
                }
            }
            else if (sym == QLatin1Char('~'))
                out += QLatin1Char(' ');
            continue;
        }
        out += c;
        ++i;
    }
    return out.simplified();
}

} // namespace Rtf
