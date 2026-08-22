#include "mrb2png.h"

#include <QFile>
#include <QImage>
#include <QVector>

#include <cstring>

namespace Hlp {

namespace {

// Заголовок SHG/MRB: wMagic, n, затем n смещений картинок.
// Картинка: byType (5=DDB, 6=DIB, 8=WMF), byPacked
// (0=без сжатия, 1=RLE, 2=LZ77, 3=оба), затем поля с "сжатым"
// кодированием (значение хранится со сдвигом <<1, младший бит -
// признак двухсловного значения).

struct Picture
{
    quint8 byType;
    quint8 byPacked;
    quint8 bitCount;
    quint16 planes;
    quint32 width;
    quint32 height;
    quint32 clrUsed;
    quint32 clrImportant;
    quint32 dataSize;
    quint32 hotspotSize;
    quint32 pictOff;
    quint32 hsOff;
    // Для layout "перезаписи" декомпилятора заголовок занимает 32 байта
    // и первые четыре DWORD после clrImportant хранят dataSize/hotspotSize
    // (со сдвигом <<1) и смещения. Для исходного формата - 28 байт и поля
    // "сжатые".
    bool rewrite;
    qint32 paletteOff;
    qint32 dataOff;
};

quint16 rd16(const uchar *p)
{
    return quint16(quint16(p[0]) | (quint16(p[1]) << 8));
}

quint32 rd32(const uchar *p)
{
    return quint32(quint32(p[0]) | (quint32(p[1]) << 8)
                   | (quint32(p[2]) << 16) | (quint32(p[3]) << 24));
}

const uchar *rdCWord(const uchar *p, quint32 &v)
{
    const uchar b = *p++;
    if (b & 1)
    {
        v = quint32(quint16(b) | (quint16(*p++) << 8)) >> 1;
        return p;
    }
    v = b >> 1;
    return p;
}

const uchar *rdCDWord(const uchar *p, quint32 &v)
{
    quint16 w = rd16(p);
    p += 2;
    if (w & 1)
    {
        quint16 w2 = rd16(p);
        p += 2;
        v = ((quint32(w2) << 16) | quint32(w)) >> 1;
        return p;
    }
    v = quint32(w) >> 1;
    return p;
}

bool parsePicture(const uchar *p, qint32 picLen, qint32 fileLen, Picture &pic)
{
    if (picLen < 8)
        return false;
    pic.byType = p[0];
    pic.byPacked = p[1];
    pic.rewrite = false;
    pic.paletteOff = -1;
    pic.dataOff = -1;

    // Общие "сжатые" поля: xPels, yPels, planes, bitCount, width,
    // height, clrUsed, clrImportant.
    const uchar *q = p + 2;
    quint32 v;
    q = rdCDWord(q, v); // xPels
    q = rdCDWord(q, v); // yPels
    q = rdCWord(q, v); pic.planes = quint16(v);
    q = rdCWord(q, v); pic.bitCount = quint8(v);
    q = rdCDWord(q, v); pic.width = v;
    q = rdCDWord(q, v); pic.height = v;
    q = rdCDWord(q, v); pic.clrUsed = v;
    q = rdCDWord(q, v); pic.clrImportant = v;
    const qint32 fieldsEnd = qint32(q - p);

    const qint32 regionEnd = (picLen > 0) ? picLen : fileLen;

    // Layout "перезаписи" декомпилятора: после clrImportant идут 4 DWORD:
    // dataSize/hotspotSize (со сдвигом <<1), pictOff, hsOff, затем палитра.
    if (picLen >= 36 && fieldsEnd + 16 <= picLen)
    {
        const quint32 d0 = rd32(p + fieldsEnd);
        const quint32 d1 = rd32(p + fieldsEnd + 4);
        const quint32 d2 = rd32(p + fieldsEnd + 8);
        const quint32 d3 = rd32(p + fieldsEnd + 12);
        const quint32 dataSize = d0 >> 1;
        const quint32 hsSize = d1 >> 1;
        if (dataSize > 0 && dataSize < 0x10000000
            && d2 >= quint32(fieldsEnd + 16)
            && d2 + dataSize <= quint32(regionEnd))
        {
            pic.rewrite = true;
            pic.dataSize = dataSize;
            pic.hotspotSize = hsSize;
            pic.pictOff = d2;
            pic.hsOff = d3;
            pic.paletteOff = fieldsEnd + 16;
            pic.dataOff = qint32(d2);
        }
    }

    if (!pic.rewrite)
    {
        // Исходный формат: "сжатые" dataSize/hotspotSize, затем смещения.
        q = rdCDWord(q, pic.dataSize);
        q = rdCDWord(q, pic.hotspotSize);
        pic.pictOff = rd32(q); q += 4;
        pic.hsOff = rd32(q); q += 4;
        const qint32 hdr = qint32(q - p);
        if (pic.dataSize == 0 || pic.dataSize > 0x10000000
            || pic.pictOff < quint32(hdr)
            || pic.pictOff + pic.dataSize > quint32(regionEnd))
            return false;
        pic.paletteOff = hdr;
        pic.dataOff = qint32(pic.pictOff);
    }

    // Вычисляем количество цветов палитры.
    const quint32 defColors =
        pic.bitCount < 32 ? (quint32(1) << pic.bitCount) : 0;
    if (pic.clrUsed == 0)
        pic.clrUsed = defColors;

    return pic.width > 0 && pic.height > 0
           && (pic.byType == 5 || pic.byType == 6)
           && (pic.bitCount == 1 || pic.bitCount == 4 || pic.bitCount == 8)
           && pic.clrUsed <= 256;
}

// Распаковка данных картинки. Декомпилятор хранит растровые данные либо
// без сжатия (byPacked 0), либо в формате RLE (byPacked 1).
bool expandPixels(const QByteArray &raw, quint8 byPacked,
                  QVector<uchar> &out)
{
    out.clear();
    if (byPacked == 0)
    {
        out.resize(raw.size());
        std::memcpy(out.data(), raw.constData(), size_t(raw.size()));
        return true;
    }
    if (byPacked != 1)
        return false; // LZ77 в этих файлах уже распакован декомпилятором

    // RLE (схема helpdeco::DeRun): байт со старшим битом - повторение,
    // значение count<<1; иначе байт выдаётся как есть.
    signed char count = 0;
    for (int i = 0; i < raw.size(); ++i)
    {
        const uchar c = uchar(raw.at(i));
        if (count & 0x7F)
        {
            if (count & 0x80)
            {
                out.append(c);
                --count;
            }
            else
            {
                const int n = count;
                for (int k = 0; k < n; ++k)
                    out.append(c);
                count = 0;
            }
        }
        else
        {
            count = static_cast<signed char>(c);
        }
    }
    return true;
}

bool buildImage(const Picture &pic, const QByteArray &raw,
                const uchar *pal, QImage &img)
{
    const qint32 width = qint32(pic.width);
    const qint32 height = qint32(pic.height);
    const qint32 stride = ((width * pic.bitCount + 31) / 32) * 4;
    if (raw.size() < stride * height)
        return false;

    QVector<QRgb> table;
    table.reserve(int(pic.clrUsed));
    if (pic.byType == 5)
    {
        table.append(qRgb(0, 0, 0));
        table.append(qRgb(255, 255, 255));
    }
    else
    {
        for (quint32 i = 0; i < pic.clrUsed; ++i)
        {
            const uchar b = pal[i * 4 + 0];
            const uchar g = pal[i * 4 + 1];
            const uchar r = pal[i * 4 + 2];
            table.append(qRgb(r, g, b));
        }
    }

    QImage im(width, height, QImage::Format_Indexed8);
    im.setColorTable(table);
    if (pic.bitCount == 8)
    {
        for (qint32 y = 0; y < height; ++y)
            std::memcpy(im.scanLine(y), raw.constData() + y * stride,
                        size_t(width));
    }
    else if (pic.bitCount == 4)
    {
        for (qint32 y = 0; y < height; ++y)
        {
            uchar *line = im.scanLine(y);
            const uchar *src = reinterpret_cast<const uchar *>(raw.constData())
                               + y * stride;
            for (qint32 x = 0; x < width; ++x)
                line[x] = (src[x / 2] >> (x & 1 ? 0 : 4)) & 0x0F;
        }
    }
    else // 1 bpp
    {
        for (qint32 y = 0; y < height; ++y)
        {
            uchar *line = im.scanLine(y);
            const uchar *src = reinterpret_cast<const uchar *>(raw.constData())
                               + y * stride;
            for (qint32 x = 0; x < width; ++x)
                line[x] = (src[x / 8] >> (7 - (x & 7))) & 1;
        }
    }

    img = im.convertToFormat(QImage::Format_ARGB32);
    return true;
}

} // namespace

bool mrbToPng(const QString &mrbPath, const QString &pngPath)
{
    QFile f(mrbPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = f.readAll();
    f.close();
    if (data.size() < 8)
        return false;

    const uchar *buf = reinterpret_cast<const uchar *>(data.constData());
    const quint16 magic = rd16(buf);
    if (magic != 0x506C /* SHG */ && magic != 0x706C /* MRB */)
        return false;
    const quint16 n = rd16(buf + 2);
    if (n == 0 || n > 64)
        return false;

    int best = -1;
    qint64 bestArea = -1;
    QVector<Picture> pics(n);
    for (int j = 0; j < n; ++j)
    {
        const quint32 offs = rd32(buf + 4 + 4 * j);
        if (offs >= quint32(data.size()))
            continue;
        qint32 next = (j + 1 < n) ? qint32(rd32(buf + 8 + 4 * j)) : data.size();
        Picture pic;
        if (!parsePicture(buf + offs, next - qint32(offs), data.size(), pic))
            continue;
        pics[j] = pic;
        const qint64 area = qint64(pic.width) * qint64(pic.height);
        if (area > bestArea)
        {
            bestArea = area;
            best = j;
        }
    }
    if (best < 0)
        return false;

    const Picture &pic = pics[best];
    if (pic.byType == 8)
        return false;

    const quint32 offs = rd32(buf + 4 + 4 * best);
    // Для "перезаписи" декомпилятора pictOff считается от начала файла,
    // для исходного формата - от начала картинки (за 9-байтовым FILEHEADER).
    const qint64 base = qint64(offs) + (pic.rewrite ? 0 : 9);
    const qint64 dataStart = base + pic.dataOff;
    const qint64 avail = data.size() - dataStart;
    const qint64 len = qMin<qint64>(qint64(pic.dataSize), avail);
    if (len <= 0)
        return false;
    const QByteArray raw = data.mid(int(dataStart), int(len));

    QVector<uchar> pixels;
    if (!expandPixels(raw, pic.byPacked, pixels))
        return false;

    const uchar *pal = buf + base + pic.paletteOff;
    QImage img;
    if (!buildImage(pic,
                    QByteArray(reinterpret_cast<const char *>(pixels.constData()),
                               int(pixels.size())),
                    pal, img))
        return false;

    return img.save(pngPath);
}

} // namespace Hlp
