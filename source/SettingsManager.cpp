#include "SettingsManager.h"

SettingsManager &SettingsManager::instance()
{
    static SettingsManager mgr;
    return mgr;
}

SettingsManager::SettingsManager()
    : m_settings("BookCreateEdit.ini", QSettings::IniFormat)
{
}

void SettingsManager::setFilePath(const QString &path)
{
    m_settings.setPath(QSettings::IniFormat, QSettings::UserScope, path);
}

QString SettingsManager::filePath() const
{
    return m_settings.fileName();
}

void SettingsManager::sync()
{
    m_settings.sync();
}

// ===================== Font =====================
QString SettingsManager::editorFontFamily() const
{
    return m_settings.value("Font/EditorFamily", "Consolas").toString();
}

void SettingsManager::setEditorFontFamily(const QString &family)
{
    m_settings.setValue("Font/EditorFamily", family);
}

int SettingsManager::editorFontSize() const
{
    return m_settings.value("Font/EditorSize", 12).toInt();
}

void SettingsManager::setEditorFontSize(int size)
{
    m_settings.setValue("Font/EditorSize", size);
}

bool SettingsManager::editorFontAntialias() const
{
    return m_settings.value("Font/EditorAntialias", true).toBool();
}

void SettingsManager::setEditorFontAntialias(bool on)
{
    m_settings.setValue("Font/EditorAntialias", on);
}

// ===================== Scale =====================
bool SettingsManager::autoScale() const
{
    return m_settings.value("Scale/AutoScale", true).toBool();
}

void SettingsManager::setAutoScale(bool on)
{
    m_settings.setValue("Scale/AutoScale", on);
}

int SettingsManager::scaleFactor() const
{
    return m_settings.value("Scale/Factor", 100).toInt();
}

void SettingsManager::setScaleFactor(int percent)
{
    m_settings.setValue("Scale/Factor", percent);
}

bool SettingsManager::useScaleFactors() const
{
    return m_settings.value("Scale/UseScaleFactors", true).toBool();
}

void SettingsManager::setUseScaleFactors(bool on)
{
    m_settings.setValue("Scale/UseScaleFactors", on);
}

bool SettingsManager::integerScaleOnly() const
{
    return m_settings.value("Scale/IntegerOnly", false).toBool();
}

void SettingsManager::setIntegerScaleOnly(bool on)
{
    m_settings.setValue("Scale/IntegerOnly", on);
}

// ===================== Plugins =====================
QStringList SettingsManager::enabledPlugins() const
{
    return m_settings.value("Plugins/Enabled",
        QStringList() << "Переводчик Google" << "Проверка орфографии" << "Экспорт в Kindle").toStringList();
}

void SettingsManager::setEnabledPlugins(const QStringList &list)
{
    m_settings.setValue("Plugins/Enabled", list);
}

// ===================== Formats =====================
QStringList SettingsManager::enabledFormats() const
{
    return m_settings.value("Formats/Enabled",
        QStringList() << "EPUB" << "MOBI" << "PDF" << "FB2" << "DOC" << "RTF" << "HTML" << "Markdown").toStringList();
}

void SettingsManager::setEnabledFormats(const QStringList &list)
{
    m_settings.setValue("Formats/Enabled", list);
}

// ===================== Appearance =====================
int SettingsManager::theme() const
{
    return m_settings.value("Appearance/Theme", 0).toInt();
}

void SettingsManager::setTheme(int index)
{
    m_settings.setValue("Appearance/Theme", index);
}

QString SettingsManager::accentColor() const
{
    return m_settings.value("Appearance/AccentColor", "#4a90d9").toString();
}

void SettingsManager::setAccentColor(const QString &color)
{
    m_settings.setValue("Appearance/AccentColor", color);
}

QString SettingsManager::backgroundColor() const
{
    return m_settings.value("Appearance/BackgroundColor", "#f0f0f0").toString();
}

void SettingsManager::setBackgroundColor(const QString &color)
{
    m_settings.setValue("Appearance/BackgroundColor", color);
}

bool SettingsManager::showRibbonIcons() const
{
    return m_settings.value("Appearance/ShowRibbonIcons", true).toBool();
}

void SettingsManager::setShowRibbonIcons(bool on)
{
    m_settings.setValue("Appearance/ShowRibbonIcons", on);
}

bool SettingsManager::largeRibbonIcons() const
{
    return m_settings.value("Appearance/LargeRibbonIcons", true).toBool();
}

void SettingsManager::setLargeRibbonIcons(bool on)
{
    m_settings.setValue("Appearance/LargeRibbonIcons", on);
}

int SettingsManager::ribbonStyle() const
{
    return m_settings.value("Appearance/RibbonStyle", 0).toInt();
}

void SettingsManager::setRibbonStyle(int index)
{
    m_settings.setValue("Appearance/RibbonStyle", index);
}

int SettingsManager::sidebarWidth() const
{
    return m_settings.value("Appearance/SidebarWidth", 280).toInt();
}

void SettingsManager::setSidebarWidth(int width)
{
    m_settings.setValue("Appearance/SidebarWidth", width);
}

bool SettingsManager::autoHideSidebar() const
{
    return m_settings.value("Appearance/AutoHideSidebar", false).toBool();
}

void SettingsManager::setAutoHideSidebar(bool on)
{
    m_settings.setValue("Appearance/AutoHideSidebar", on);
}

bool SettingsManager::showTabIcons() const
{
    return m_settings.value("Appearance/ShowTabIcons", true).toBool();
}

void SettingsManager::setShowTabIcons(bool on)
{
    m_settings.setValue("Appearance/ShowTabIcons", on);
}

// ===================== Dictionaries =====================
QStringList SettingsManager::enabledDictionaries() const
{
    return m_settings.value("Dictionaries/Enabled",
        QStringList() << "Русский" << "English (US)").toStringList();
}

void SettingsManager::setEnabledDictionaries(const QStringList &list)
{
    m_settings.setValue("Dictionaries/Enabled", list);
}

QString SettingsManager::dictionaryPath() const
{
    return m_settings.value("Dictionaries/Path", "./source/des").toString();
}

void SettingsManager::setDictionaryPath(const QString &path)
{
    m_settings.setValue("Dictionaries/Path", path);
}

// ===================== Autosave =====================
bool SettingsManager::autoSaveEnabled() const
{
    return m_settings.value("Autosave/Enabled", true).toBool();
}

void SettingsManager::setAutoSaveEnabled(bool on)
{
    m_settings.setValue("Autosave/Enabled", on);
}

int SettingsManager::autoSaveInterval() const
{
    return m_settings.value("Autosave/Interval", 5).toInt();
}

void SettingsManager::setAutoSaveInterval(int minutes)
{
    m_settings.setValue("Autosave/Interval", minutes);
}

bool SettingsManager::saveOnExit() const
{
    return m_settings.value("Autosave/SaveOnExit", true).toBool();
}

void SettingsManager::setSaveOnExit(bool on)
{
    m_settings.setValue("Autosave/SaveOnExit", on);
}

bool SettingsManager::saveBeforeExport() const
{
    return m_settings.value("Autosave/SaveBeforeExport", true).toBool();
}

void SettingsManager::setSaveBeforeExport(bool on)
{
    m_settings.setValue("Autosave/SaveBeforeExport", on);
}

bool SettingsManager::keepHistory() const
{
    return m_settings.value("Autosave/KeepHistory", true).toBool();
}

void SettingsManager::setKeepHistory(bool on)
{
    m_settings.setValue("Autosave/KeepHistory", on);
}

int SettingsManager::maxVersions() const
{
    return m_settings.value("Autosave/MaxVersions", 20).toInt();
}

void SettingsManager::setMaxVersions(int count)
{
    m_settings.setValue("Autosave/MaxVersions", count);
}

bool SettingsManager::autoCleanHistory() const
{
    return m_settings.value("Autosave/AutoClean", true).toBool();
}

void SettingsManager::setAutoCleanHistory(bool on)
{
    m_settings.setValue("Autosave/AutoClean", on);
}

QString SettingsManager::backupPath() const
{
    return m_settings.value("Autosave/BackupPath", "./history").toString();
}

void SettingsManager::setBackupPath(const QString &path)
{
    m_settings.setValue("Autosave/BackupPath", path);
}

bool SettingsManager::compressBackups() const
{
    return m_settings.value("Autosave/CompressBackups", true).toBool();
}

void SettingsManager::setCompressBackups(bool on)
{
    m_settings.setValue("Autosave/CompressBackups", on);
}

// ===================== Image Editor =====================
QString SettingsManager::imageDefaultFormat() const
{
    return m_settings.value("ImageEditor/DefaultFormat", "PNG").toString();
}

void SettingsManager::setImageDefaultFormat(const QString &format)
{
    m_settings.setValue("ImageEditor/DefaultFormat", format);
}

int SettingsManager::imageQuality() const
{
    return m_settings.value("ImageEditor/Quality", 90).toInt();
}

void SettingsManager::setImageQuality(int quality)
{
    m_settings.setValue("ImageEditor/Quality", quality);
}

bool SettingsManager::imagePreserveExif() const
{
    return m_settings.value("ImageEditor/PreserveExif", true).toBool();
}

void SettingsManager::setImagePreserveExif(bool on)
{
    m_settings.setValue("ImageEditor/PreserveExif", on);
}

bool SettingsManager::imageAutoOrient() const
{
    return m_settings.value("ImageEditor/AutoOrient", true).toBool();
}

void SettingsManager::setImageAutoOrient(bool on)
{
    m_settings.setValue("ImageEditor/AutoOrient", on);
}

int SettingsManager::imageHistorySize() const
{
    return m_settings.value("ImageEditor/HistorySize", 50).toInt();
}

void SettingsManager::setImageHistorySize(int size)
{
    m_settings.setValue("ImageEditor/HistorySize", size);
}

bool SettingsManager::imageSmoothZoom() const
{
    return m_settings.value("ImageEditor/SmoothZoom", true).toBool();
}

void SettingsManager::setImageSmoothZoom(bool on)
{
    m_settings.setValue("ImageEditor/SmoothZoom", on);
}

bool SettingsManager::imageShowGrid() const
{
    return m_settings.value("ImageEditor/ShowGrid", false).toBool();
}

void SettingsManager::setImageShowGrid(bool on)
{
    m_settings.setValue("ImageEditor/ShowGrid", on);
}

double SettingsManager::imageGridSize() const
{
    return m_settings.value("ImageEditor/GridSize", 8.0).toDouble();
}

void SettingsManager::setImageGridSize(double size)
{
    m_settings.setValue("ImageEditor/GridSize", size);
}

QString SettingsManager::imageResizeAlgorithm() const
{
    return m_settings.value("ImageEditor/ResizeAlgorithm", "Bilinear").toString();
}

void SettingsManager::setImageResizeAlgorithm(const QString &algo)
{
    m_settings.setValue("ImageEditor/ResizeAlgorithm", algo);
}

bool SettingsManager::imageKeepAspect() const
{
    return m_settings.value("ImageEditor/KeepAspect", true).toBool();
}

void SettingsManager::setImageKeepAspect(bool on)
{
    m_settings.setValue("ImageEditor/KeepAspect", on);
}

bool SettingsManager::imageAllowUpscale() const
{
    return m_settings.value("ImageEditor/AllowUpscale", false).toBool();
}

void SettingsManager::setImageAllowUpscale(bool on)
{
    m_settings.setValue("ImageEditor/AllowUpscale", on);
}
