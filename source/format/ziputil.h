#ifndef ZIPUTIL_H
#define ZIPUTIL_H

#include <QByteArray>
#include <QString>
#include <QList>
#include <QtEndian>

/**
 * @brief Утилиты чтения ZIP-контейнеров для форматных плагинов.
 *
 * Реальные EPUB/DOCX-файлы (Calibre, Word и т.д.) сжаты deflate,
 * а прежние читатели понимали только несжатые записи (stored).
 * Здесь поддержаны оба метода (0 — stored, 8 — deflate). Размеры и
 * смещения берутся из центрального каталога, поэтому корректно читаются
 * даже «потоковые» архивы с нулями в локальных заголовках.
 *
 * Распаковка выполняется собственным компактным инфлятором (RFC 1951),
 * чтобы не зависеть от внешних библиотек zlib.
 */
namespace ZipUtil {

struct ZipEntry {
    QString name;       ///< Имя записи внутри архива
    QByteArray data;    ///< Распакованное содержимое
};

// -----------------------------------------------------------------------
// Минимальный распаковщик raw-deflate (RFC 1951), адаптация алгоритма puff.
// -----------------------------------------------------------------------
namespace InflateImpl {

/** @brief Побитовый читатель потока (младшие биты вперёд). */
struct BitReader {
    const uchar *p;
    int size;
    int pos;
    int buf;
    int cnt;

    BitReader(const uchar *data, int n)
        : p(data), size(n), pos(0), buf(0), cnt(0) {}

    /** @brief Читает need бит (до 16 за раз); -1 — данные кончились. */
    int bits(int need)
    {
        while (cnt < need) {
            if (pos >= size)
                return -1;
            buf |= p[pos++] << cnt;
            cnt += 8;
        }
        const int v = buf & ((1 << need) - 1);
        buf >>= need;
        cnt -= need;
        return v;
    }
};

/** @brief Канонический код Хаффмана (счётчики длин + отсортированные символы). */
struct Huffman {
    short counts[16];
    short symbols[320];

    void clear()
    {
        for (int i = 0; i < 16; ++i)
            counts[i] = 0;
    }

    /** @brief Строит таблицу из массива длин кодов. false при ошибке. */
    bool build(const uchar *lengths, int n)
    {
        clear();
        for (int i = 0; i < n; ++i)
            counts[lengths[i]]++;
        if (counts[0] == n)
            return true;                    // пустой код (допустимо для дистанций)
        counts[0] = 0;

        // Проверка переполнения кода
        int left = 1;
        for (int len = 1; len <= 15; ++len) {
            left <<= 1;
            left -= counts[len];
            if (left < 0)
                return false;
        }

        short offs[16];
        offs[1] = 0;
        for (int len = 1; len < 15; ++len)
            offs[len + 1] = offs[len] + counts[len];

        for (int i = 0; i < n; ++i)
            if (lengths[i])
                symbols[offs[lengths[i]]++] = short(i);
        return true;
    }

    /** @brief Декодирует один символ; -1 — ошибка потока. */
    int decode(BitReader &br) const
    {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len <= 15; ++len) {
            const int b = br.bits(1);
            if (b < 0)
                return -1;
            code |= b;
            const int count = counts[len];
            if (code - count < first)
                return symbols[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }
};

// Базовые значения и дополнительные биты длин/дистанций (RFC 1951)
const ushort kLenBase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19,
    23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
const uchar kLenExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
const ushort kDistBase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65,
    97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577 };
const uchar kDistExtra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

/** @brief Блок фиксированных кодов Хаффмана. */
inline void fixedTables(Huffman &lit, Huffman &dist)
{
    uchar lens[288];
    for (int i = 0; i < 144; ++i) lens[i] = 8;
    for (int i = 144; i < 256; ++i) lens[i] = 9;
    for (int i = 256; i < 280; ++i) lens[i] = 7;
    for (int i = 280; i < 288; ++i) lens[i] = 8;
    lit.build(lens, 288);

    uchar dlens[30];
    for (int i = 0; i < 30; ++i) dlens[i] = 5;
    dist.build(dlens, 30);
}

/** @brief Читает динамические таблицы кодов блока. */
inline bool dynamicTables(BitReader &br, Huffman &lit, Huffman &dist)
{
    const int hlit = br.bits(5) + 257;
    const int hdist = br.bits(5) + 1;
    const int hclen = br.bits(4) + 4;
    if (hlit < 0 || hdist < 0 || hclen < 0 || hlit > 286 || hdist > 30)
        return false;

    static const uchar order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
        11, 4, 12, 3, 13, 2, 14, 1, 15 };
    uchar clens[19];
    for (int i = 0; i < 19; ++i)
        clens[i] = 0;
    for (int i = 0; i < hclen; ++i) {
        const int v = br.bits(3);
        if (v < 0)
            return false;
        clens[order[i]] = uchar(v);
    }

    Huffman clen;
    if (!clen.build(clens, 19))
        return false;

    uchar lens[286 + 30];
    int n = 0;
    while (n < hlit + hdist) {
        int sym = clen.decode(br);
        if (sym < 0)
            return false;
        if (sym < 16) {
            lens[n++] = uchar(sym);
        } else {
            int len = 0, rep = 0;
            if (sym == 16) {
                if (n == 0)
                    return false;
                len = lens[n - 1];
                rep = 3 + br.bits(2);
            } else if (sym == 17) {
                rep = 3 + br.bits(3);
            } else {
                rep = 11 + br.bits(7);
            }
            if (n + rep > hlit + hdist)
                return false;
            while (rep--)
                lens[n++] = uchar(sym == 16 ? len : 0);
        }
    }
    if (lens[256] == 0)
        return false;                       // нет кода конца блока
    return lit.build(lens, hlit) && dist.build(lens + hlit, hdist);
}

/**
 * @brief Распаковывает поток raw-deflate целиком.
 * @return true при успехе (out содержит распакованные данные).
 */
inline bool inflate(const uchar *src, int srcSize, QByteArray &out)
{
    BitReader br(src, srcSize);

    for (;;) {
        const int bfinal = br.bits(1);
        const int btype = br.bits(2);
        if (bfinal < 0 || btype < 0)
            return false;

        if (btype == 0) {
            // Несжатый блок: выровнять на границу байта
            br.buf >>= (br.cnt & 7);
            br.cnt -= (br.cnt & 7);
            if (br.cnt >= 8 && br.pos < br.size) {
                br.pos--;
                br.buf = 0;
                br.cnt = 0;
            } else {
                br.buf = 0;
                br.cnt = 0;
            }
            const int lo = br.bits(8);
            const int hi = br.bits(8);
            const int nlo = br.bits(8);
            const int nhi = br.bits(8);
            if (lo < 0 || hi < 0 || nlo < 0 || nhi < 0)
                return false;
            const int len = lo | (hi << 8);
            const int nlen = nlo | (nhi << 8);
            if ((len ^ 0xFFFF) != nlen)
                return false;
            for (int i = 0; i < len; ++i) {
                const int b = br.bits(8);
                if (b < 0)
                    return false;
                out.append(char(b));
            }
        } else {
            Huffman lit, dist;
            if (btype == 1) {
                fixedTables(lit, dist);
            } else if (btype == 2) {
                if (!dynamicTables(br, lit, dist))
                    return false;
            } else {
                return false;               // недопустимый тип блока
            }

            for (;;) {
                const int sym = lit.decode(br);
                if (sym < 0)
                    return false;
                if (sym < 256) {
                    out.append(char(sym));
                } else if (sym == 256) {
                    break;                  // конец блока
                } else {
                    const int lsym = sym - 257;
                    if (lsym >= 29)
                        return false;
                    const int e1 = br.bits(kLenExtra[lsym]);
                    const int dsym = dist.decode(br);
                    if (e1 < 0 || dsym < 0 || dsym >= 30)
                        return false;
                    const int e2 = br.bits(kDistExtra[dsym]);
                    if (e2 < 0)
                        return false;
                    const int len = kLenBase[lsym] + e1;
                    const int d = kDistBase[dsym] + e2;

                    // Копируем len байт со смещением d назад
                    const int start = out.size() - d;
                    if (start < 0)
                        return false;
                    if (d >= len) {
                        out.append(out.constData() + start, len);
                    } else {
                        // Перекрывающееся копирование (RLE-подобные участки)
                        for (int i = 0; i < len; ++i)
                            out.append(out.at(start + i));
                    }
                }
            }
        }

        if (bfinal)
            break;
    }
    return true;
}

} // namespace InflateImpl

/**
 * @brief Распаковывает raw-deflate данные из zip-записи.
 * @param deflated   Сжатые данные записи.
 * @param uncompSize Ожидаемый размер после распаковки (для резерва памяти).
 */
inline QByteArray inflateRawDeflate(const QByteArray &deflated, quint32 uncompSize)
{
    QByteArray out;
    if (deflated.isEmpty())
        return out;
    out.reserve(int(uncompSize));
    if (!InflateImpl::inflate(
            reinterpret_cast<const uchar *>(deflated.constData()),
            deflated.size(), out))
        return QByteArray();
    return out;
}

/**
 * @brief Читает все записи zip-архива.
 * @param zip   Содержимое файла-архива целиком.
 * @param error Сюда пишется причина неудачи (может быть nullptr).
 */
inline QList<ZipEntry> readZip(const QByteArray &zip, QString *error = nullptr)
{
    QList<ZipEntry> result;

    // Ищем End Of Central Directory с конца файла (комментарий до 64КБ)
    int eocd = -1;
    const int limit = int(zip.size()) - 22 - 65536;
    for (int i = int(zip.size()) - 22; i >= limit && i >= 0; --i) {
        if (zip.mid(i, 4) == QByteArray("PK\x05\x06", 4)) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        if (error) *error = QStringLiteral("central directory not found");
        return result;
    }

    const uchar *eob = reinterpret_cast<const uchar *>(zip.constData()) + eocd;
    const quint16 totalEntries = qFromLittleEndian<quint16>(eob + 10);
    const quint32 cdOffset = qFromLittleEndian<quint32>(eob + 16);

    int pos = int(cdOffset);
    for (int n = 0; n < totalEntries; ++n) {
        if (pos + 46 > zip.size() || zip.mid(pos, 4) != QByteArray("PK\x01\x02", 4))
            break;

        const uchar *base = reinterpret_cast<const uchar *>(zip.constData()) + pos;
        const quint16 method     = qFromLittleEndian<quint16>(base + 10);
        const quint32 compSize   = qFromLittleEndian<quint32>(base + 20);
        const quint32 uncompSize = qFromLittleEndian<quint32>(base + 24);
        const quint16 nameLen    = qFromLittleEndian<quint16>(base + 28);
        const quint16 extraLen   = qFromLittleEndian<quint16>(base + 30);
        const quint16 commLen    = qFromLittleEndian<quint16>(base + 32);
        const quint32 lhOffset   = qFromLittleEndian<quint32>(base + 42);

        const QString name = QString::fromUtf8(zip.mid(pos + 46, nameLen));

        // Каталоги пропускаем
        if (!name.endsWith('/') && int(lhOffset) + 30 <= zip.size()) {
            const uchar *lh =
                reinterpret_cast<const uchar *>(zip.constData()) + int(lhOffset);
            const quint16 lNameLen  = qFromLittleEndian<quint16>(lh + 26);
            const quint16 lExtraLen = qFromLittleEndian<quint16>(lh + 28);
            const int dataPos = int(lhOffset) + 30 + lNameLen + lExtraLen;

            ZipEntry entry;
            entry.name = name;
            if (method == 0) {
                // stored — данные лежат как есть
                entry.data = zip.mid(dataPos,
                                     int(compSize ? compSize : uncompSize));
            } else if (method == 8) {
                // deflate — распаковываем собственным инфлятором
                entry.data = inflateRawDeflate(
                    zip.mid(dataPos, int(compSize)), uncompSize);
                if (entry.data.isEmpty() && uncompSize > 0 && error)
                    *error = QStringLiteral("deflate failed: ") + name;
            } else if (error) {
                *error = QStringLiteral("unsupported compression %1: %2")
                             .arg(method).arg(name);
            }
            result.append(entry);
        }

        pos += 46 + nameLen + extraLen + commLen;
    }

    return result;
}

} // namespace ZipUtil

#endif // ZIPUTIL_H
