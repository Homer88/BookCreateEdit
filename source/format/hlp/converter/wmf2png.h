#pragma once

#include <QString>

namespace Hlp {

// Растеризует Windows Metafile (.wmf) в PNG через GDI+.
// Возвращает true, если удалось создать pngPath.
bool wmfToPng(const QString &wmfPath, const QString &pngPath);

// Возвращает true, если формат файла не поддерживается ни Qt, ни GDI+
// (например .mrb/.shg) и изображение не удастся показать.
bool unsupportedImageFile(const QString &filename);

} // namespace Hlp
