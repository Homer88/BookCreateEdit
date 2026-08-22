#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QFont>

class SettingsManager
{
public:
    static SettingsManager &instance();

    void setFilePath(const QString &path);
    QString filePath() const;

    void sync();

    // Font
    QString editorFontFamily() const;
    void setEditorFontFamily(const QString &family);
    int editorFontSize() const;
    void setEditorFontSize(int size);
    bool editorFontAntialias() const;
    void setEditorFontAntialias(bool on);

    // Scale
    bool autoScale() const;
    void setAutoScale(bool on);
    int scaleFactor() const;
    void setScaleFactor(int percent);
    bool useScaleFactors() const;
    void setUseScaleFactors(bool on);
    bool integerScaleOnly() const;
    void setIntegerScaleOnly(bool on);

    // Plugins
    QStringList enabledPlugins() const;
    void setEnabledPlugins(const QStringList &list);

    // Formats
    QStringList enabledFormats() const;
    void setEnabledFormats(const QStringList &list);

    // Appearance
    int theme() const;
    void setTheme(int index);
    QString accentColor() const;
    void setAccentColor(const QString &color);
    QString backgroundColor() const;
    void setBackgroundColor(const QString &color);
    bool showRibbonIcons() const;
    void setShowRibbonIcons(bool on);
    bool largeRibbonIcons() const;
    void setLargeRibbonIcons(bool on);
    int ribbonStyle() const;
    void setRibbonStyle(int index);
    int sidebarWidth() const;
    void setSidebarWidth(int width);
    bool autoHideSidebar() const;
    void setAutoHideSidebar(bool on);
    bool showTabIcons() const;
    void setShowTabIcons(bool on);

    // Dictionaries
    QStringList enabledDictionaries() const;
    void setEnabledDictionaries(const QStringList &list);
    QString dictionaryPath() const;
    void setDictionaryPath(const QString &path);

    // Autosave
    bool autoSaveEnabled() const;
    void setAutoSaveEnabled(bool on);
    int autoSaveInterval() const;
    void setAutoSaveInterval(int minutes);
    bool saveOnExit() const;
    void setSaveOnExit(bool on);
    bool saveBeforeExport() const;
    void setSaveBeforeExport(bool on);
    bool keepHistory() const;
    void setKeepHistory(bool on);
    int maxVersions() const;
    void setMaxVersions(int count);
    bool autoCleanHistory() const;
    void setAutoCleanHistory(bool on);
    QString backupPath() const;
    void setBackupPath(const QString &path);
    bool compressBackups() const;
    void setCompressBackups(bool on);

    // Image Editor
    QString imageDefaultFormat() const;
    void setImageDefaultFormat(const QString &format);
    int imageQuality() const;
    void setImageQuality(int quality);
    bool imagePreserveExif() const;
    void setImagePreserveExif(bool on);
    bool imageAutoOrient() const;
    void setImageAutoOrient(bool on);
    int imageHistorySize() const;
    void setImageHistorySize(int size);
    bool imageSmoothZoom() const;
    void setImageSmoothZoom(bool on);
    bool imageShowGrid() const;
    void setImageShowGrid(bool on);
    double imageGridSize() const;
    void setImageGridSize(double size);
    QString imageResizeAlgorithm() const;
    void setImageResizeAlgorithm(const QString &algo);
    bool imageKeepAspect() const;
    void setImageKeepAspect(bool on);
    bool imageAllowUpscale() const;
    void setImageAllowUpscale(bool on);

private:
    SettingsManager();
    ~SettingsManager() = default;
    SettingsManager(const SettingsManager &) = delete;
    SettingsManager &operator=(const SettingsManager &) = delete;

    QSettings m_settings;
};

#endif // SETTINGSMANAGER_H
