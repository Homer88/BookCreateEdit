#pragma once

#include <QString>

namespace Hlp {

// Декодирует файл SHG/MRB (кнопки WinHelp, "mapped runtime bitmap") в PNG.
// Файлы .mrb/.shg, извлечённые декомпилятором, содержат заголовок в формате
// помощи (сжатые поля) и растровые данные без сжатия (либо RLE).
// Возвращает true, если удалось создать pngPath.
bool mrbToPng(const QString &mrbPath, const QString &pngPath);

} // namespace Hlp
