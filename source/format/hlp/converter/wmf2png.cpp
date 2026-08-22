#include "wmf2png.h"

#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <olectl.h>
#include <gdiplus.h>

namespace Hlp {

using namespace Gdiplus;

namespace {

ULONG_PTR g_gdiToken = 0;
bool g_gdiInited = false;

bool ensureGdi()
{
    if (!g_gdiInited)
    {
        GdiplusStartupInput in;
        in.GdiplusVersion = 1;
        if (GdiplusStartup(&g_gdiToken, &in, NULL) != Ok)
            return false;
        g_gdiInited = true;
    }
    return true;
}

int encoderClsid(const WCHAR *format, CLSID *pClsid)
{
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;
    std::vector<BYTE> buf(size);
    ImageCodecInfo *info = reinterpret_cast<ImageCodecInfo *>(buf.data());
    GetImageEncoders(num, size, info);
    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp(info[i].MimeType, format) == 0)
        {
            *pClsid = info[i].Clsid;
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace

bool wmfToPng(const QString &wmfPath, const QString &pngPath)
{
    if (!ensureGdi())
        return false;

    const std::wstring src = wmfPath.toStdWString();
    HANDLE h = CreateFileW(src.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    const DWORD sz = GetFileSize(h, NULL);
    if (sz == 0 || sz == INVALID_FILE_SIZE)
    {
        CloseHandle(h);
        return false;
    }
    std::vector<BYTE> bits(sz);
    DWORD rd = 0;
    const BOOL ok = ReadFile(h, bits.data(), sz, &rd, NULL);
    CloseHandle(h);
    if (!ok || rd != sz)
        return false;

    // WMF на диске имеет 22-байтный placeable-заголовок (сигнатура 0x9AC6CDD7),
    // который SetWinMetaFileBits не принимает - отбрасываем его.
    DWORD wmfSize = sz;
    BYTE *wmfBits = bits.data();
    if (wmfSize >= 22 && bits[0] == 0xD7 && bits[1] == 0xCD
        && bits[2] == 0xC6 && bits[3] == 0x9A)
    {
        wmfBits += 22;
        wmfSize -= 22;
    }

    // Преобразование WMF -> EMF через опорный DC, затем рисуем метафайл в растр.
    HDC ref = CreateCompatibleDC(NULL);
    HENHMETAFILE hemf = SetWinMetaFileBits(wmfSize, wmfBits, ref, NULL);
    DeleteDC(ref);
    if (!hemf)
        return false;

    ENHMETAHEADER hdr;
    if (!GetEnhMetaFileHeader(hemf, sizeof(hdr), &hdr))
    {
        DeleteEnhMetaFile(hemf);
        return false;
    }
    const int w = hdr.rclBounds.right - hdr.rclBounds.left;
    const int height = hdr.rclBounds.bottom - hdr.rclBounds.top;
    if (w <= 0 || height <= 0)
    {
        DeleteEnhMetaFile(hemf);
        return false;
    }

    Bitmap bmp(w, height, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(255, 255, 255, 255));
    Metafile meta(hemf);
    g.DrawImage(&meta, 0, 0, w, height);

    CLSID clsid;
    if (encoderClsid(L"image/png", &clsid) < 0)
    {
        DeleteEnhMetaFile(hemf);
        return false;
    }
    const std::wstring dst = pngPath.toStdWString();
    const Status st = bmp.Save(dst.c_str(), &clsid, NULL);
    DeleteEnhMetaFile(hemf);
    return st == Ok;
}

bool unsupportedImageFile(const QString &filename)
{
    const QString lower = filename.toLower();
    return lower.endsWith(QLatin1String(".mrb"))
        || lower.endsWith(QLatin1String(".shg"));
}

} // namespace Hlp
