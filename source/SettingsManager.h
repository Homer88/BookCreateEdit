#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QFont>

/**
 * @brief Менеджер настроек приложения.
 *
 * Реализован как синглтон: предоставляет централизованный доступ
 * к параметрам приложения (шрифт, масштабирование, внешний вид,
 * автосохранение, редактор изображений и т.д.), хранящимся в INI-файле.
 */
class SettingsManager
{
public:
    /**
     * @brief Возвращает единственный экземпляр менеджера настроек (синглтон).
     * @return Ссылка на глобальный объект SettingsManager.
     */
    static SettingsManager &instance();

    /**
     * @brief Задаёт путь к файлу хранения настроек.
     * @param path Абсолютный или относительный путь к INI-файлу.
     */
    void setFilePath(const QString &path);

    /**
     * @brief Возвращает путь к текущему файлу настроек.
     * @return Путь к INI-файлу настроек.
     */
    QString filePath() const;

    /**
     * @brief Немедленно записывает отложенные изменения на диск.
     */
    void sync();

    /**
     * @brief Возвращает значение произвольного ключа настроек.
     * @param key — ключ (например, "git/login").
     * @param defaultValue — значение по умолчанию.
     */
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;

    /**
     * @brief Устанавливает значение произвольного ключа настроек.
     * @param key — ключ.
     * @param value — значение.
     */
    void setValue(const QString &key, const QVariant &value);

    // Font
    /**
     * @brief Возвращает имя семейства шрифта редактора.
     * @return Название семейства шрифта (по умолчанию "Consolas").
     */
    QString editorFontFamily() const;

    /**
     * @brief Устанавливает семейство шрифта редактора.
     * @param family Название семейства шрифта.
     */
    void setEditorFontFamily(const QString &family);

    /**
     * @brief Возвращает размер шрифта редактора.
     * @return Размер в пунктах (по умолчанию 12).
     */
    int editorFontSize() const;

    /**
     * @brief Устанавливает размер шрифта редактора.
     * @param size Размер шрифта в пунктах.
     */
    void setEditorFontSize(int size);

    /**
     * @brief Включено ли сглаживание шрифта редактора.
     * @return true, если сглаживание включено (по умолчанию).
     */
    bool editorFontAntialias() const;

    /**
     * @brief Включает или выключает сглаживание шрифта редактора.
     * @param on true — включить сглаживание.
     */
    void setEditorFontAntialias(bool on);

    // Scale
    /**
     * @brief Включено ли автоматическое масштабирование интерфейса.
     * @return true, если автомасштабирование активно (по умолчанию).
     */
    bool autoScale() const;

    /**
     * @brief Включает или выключает автоматическое масштабирование.
     * @param on true — включить автомасштабирование.
     */
    void setAutoScale(bool on);

    /**
     * @brief Возвращает коэффициент масштабирования интерфейса.
     * @return Масштаб в процентах (по умолчанию 100).
     */
    int scaleFactor() const;

    /**
     * @brief Устанавливает коэффициент масштабирования интерфейса.
     * @param percent Масштаб в процентах.
     */
    void setScaleFactor(int percent);

    /**
     * @brief Использовать ли коэффициенты масштабирования платформы.
     * @return true, если коэффициенты применяются (по умолчанию).
     */
    bool useScaleFactors() const;

    /**
     * @brief Включает или отключает применение коэффициентов масштабирования.
     * @param on true — использовать коэффициенты.
     */
    void setUseScaleFactors(bool on);

    /**
     * @brief Допускаются ли только целочисленные значения масштаба.
     * @return true, если разрешён только целый масштаб.
     */
    bool integerScaleOnly() const;

    /**
     * @brief Ограничивает масштаб целочисленными значениями.
     * @param on true — только целые значения масштаба.
     */
    void setIntegerScaleOnly(bool on);

    // Plugins
    /**
     * @brief Возвращает список включённых плагинов.
     * @return Перечень имён активных плагинов (с набором по умолчанию).
     */
    QStringList enabledPlugins() const;

    /**
     * @brief Сохраняет список включённых плагинов.
     * @param list Имена активных плагинов.
     */
    void setEnabledPlugins(const QStringList &list);

    // Formats
    /**
     * @brief Возвращает список включённых книжных форматов.
     * @return Имена форматов (EPUB, MOBI и т.д.) со значениями по умолчанию.
     */
    QStringList enabledFormats() const;

    /**
     * @brief Сохраняет список включённых книжных форматов.
     * @param list Имена форматов.
     */
    void setEnabledFormats(const QStringList &list);

    // Appearance
    /**
     * @brief Возвращает индекс выбранной темы оформления.
     * @return Индекс темы (по умолчанию 0).
     */
    int theme() const;

    /**
     * @brief Сохраняет индекс выбранной темы оформления.
     * @param index Индекс темы.
     */
    void setTheme(int index);

    /**
     * @brief Возвращает акцентный цвет интерфейса.
     * @return Цвет в формате "#RRGGBB" (по умолчанию "#4a90d9").
     */
    QString accentColor() const;

    /**
     * @brief Устанавливает акцентный цвет интерфейса.
     * @param color Цвет в формате "#RRGGBB".
     */
    void setAccentColor(const QString &color);

    /**
     * @brief Возвращает фоновый цвет интерфейса.
     * @return Цвет в формате "#RRGGBB" (по умолчанию "#f0f0f0").
     */
    QString backgroundColor() const;

    /**
     * @brief Устанавливает фоновый цвет интерфейса.
     * @param color Цвет в формате "#RRGGBB".
     */
    void setBackgroundColor(const QString &color);

    /**
     * @brief Показывать ли значки на ленте (Ribbon).
     * @return true, если значки отображаются (по умолчанию).
     */
    bool showRibbonIcons() const;

    /**
     * @brief Включает или отключает показ значков на ленте.
     * @param on true — показывать значки.
     */
    void setShowRibbonIcons(bool on);

    /**
     * @brief Используются ли крупные значки на ленте.
     * @return true, если значки крупные (по умолчанию).
     */
    bool largeRibbonIcons() const;

    /**
     * @brief Задаёт размер значков ленты.
     * @param on true — крупные значки.
     */
    void setLargeRibbonIcons(bool on);

    /**
     * @brief Возвращает индекс стиля ленты.
     * @return Индекс стиля (по умолчанию 0).
     */
    int ribbonStyle() const;

    /**
     * @brief Устанавливает стиль ленты по индексу.
     * @param index Индекс стиля.
     */
    void setRibbonStyle(int index);

    /**
     * @brief Возвращает ширину боковой панели.
     * @return Ширина в пикселях (по умолчанию 280).
     */
    int sidebarWidth() const;

    /**
     * @brief Устанавливает ширину боковой панели.
     * @param width Ширина в пикселях.
     */
    void setSidebarWidth(int width);

    /**
     * @brief Включено ли автоскрытие боковой панели.
     * @return true, если панель скрывается автоматически.
     */
    bool autoHideSidebar() const;

    /**
     * @brief Включает или отключает автоскрытие боковой панели.
     * @param on true — скрывать панель автоматически.
     */
    void setAutoHideSidebar(bool on);

    /**
     * @brief Показывать ли значки на вкладках.
     * @return true, если значки вкладок отображаются (по умолчанию).
     */
    bool showTabIcons() const;

    /**
     * @brief Включает или отключает значки на вкладках.
     * @param on true — показывать значки вкладок.
     */
    void setShowTabIcons(bool on);

    // Dictionaries
    /**
     * @brief Возвращает список включённых словарей проверки орфографии.
     * @return Названия словарей ("Русский", "English (US)" по умолчанию).
     */
    QStringList enabledDictionaries() const;

    /**
     * @brief Сохраняет список включённых словарей.
     * @param list Названия словарей.
     */
    void setEnabledDictionaries(const QStringList &list);

    /**
     * @brief Возвращает путь к каталогу со словарями.
     * @return Путь к каталогу (по умолчанию "./source/des").
     */
    QString dictionaryPath() const;

    /**
     * @brief Устанавливает путь к каталогу со словарями.
     * @param path Путь к каталогу.
     */
    void setDictionaryPath(const QString &path);

    // Autosave
    /**
     * @brief Включено ли автосохранение документа.
     * @return true, если автосохранение активно (по умолчанию).
     */
    bool autoSaveEnabled() const;

    /**
     * @brief Включает или отключает автосохранение.
     * @param on true — включить автосохранение.
     */
    void setAutoSaveEnabled(bool on);

    /**
     * @brief Возвращает интервал автосохранения.
     * @return Интервал в минутах (по умолчанию 5).
     */
    int autoSaveInterval() const;

    /**
     * @brief Устанавливает интервал автосохранения.
     * @param minutes Интервал в минутах.
     */
    void setAutoSaveInterval(int minutes);

    /**
     * @brief Сохранять ли документ при выходе из программы.
     * @return true, если сохранение при выходе включено (по умолчанию).
     */
    bool saveOnExit() const;

    /**
     * @brief Включает или отключает сохранение при выходе.
     * @param on true — сохранять при выходе.
     */
    void setSaveOnExit(bool on);

    /**
     * @brief Сохранять ли документ перед экспортом.
     * @return true, если сохранение перед экспортом включено (по умолчанию).
     */
    bool saveBeforeExport() const;

    /**
     * @brief Включает или отключает сохранение перед экспортом.
     * @param on true — сохранять перед экспортом.
     */
    void setSaveBeforeExport(bool on);

    /**
     * @brief Хранить ли историю версий документа.
     * @return true, если история ведётся (по умолчанию).
     */
    bool keepHistory() const;

    /**
     * @brief Включает или отключает ведение истории версий.
     * @param on true — вести историю.
     */
    void setKeepHistory(bool on);

    /**
     * @brief Возвращает максимальное число хранимых версий.
     * @return Количество версий (по умолчанию 20).
     */
    int maxVersions() const;

    /**
     * @brief Устанавливает максимальное число хранимых версий.
     * @param count Лимит версий.
     */
    void setMaxVersions(int count);

    /**
     * @brief Включена ли автоматическая очистка устаревших версий истории.
     * @return true, если очистка выполняется автоматически (по умолчанию).
     */
    bool autoCleanHistory() const;

    /**
     * @brief Включает или отключает автоочистку истории.
     * @param on true — очищать историю автоматически.
     */
    void setAutoCleanHistory(bool on);

    /**
     * @brief Возвращает путь к каталогу резервных копий.
     * @return Путь к каталогу (по умолчанию "./history").
     */
    QString backupPath() const;

    /**
     * @brief Устанавливает путь к каталогу резервных копий.
     * @param path Путь к каталогу.
     */
    void setBackupPath(const QString &path);

    /**
     * @brief Сжимать ли резервные копии.
     * @return true, если сжатие включено (по умолчанию).
     */
    bool compressBackups() const;

    /**
     * @brief Включает или отключает сжатие резервных копий.
     * @param on true — сжимать копии.
     */
    void setCompressBackups(bool on);

    // Image Editor
    /**
     * @brief Возвращает формат по умолчанию для сохранения изображений.
     * @return Строка формата (по умолчанию "PNG").
     */
    QString imageDefaultFormat() const;

    /**
     * @brief Устанавливает формат по умолчанию для изображений.
     * @param format Строка формата ("PNG", "JPEG" и т.п.).
     */
    void setImageDefaultFormat(const QString &format);

    /**
     * @brief Возвращает качество сохранения изображений с потерями.
     * @return Качество в процентах (по умолчанию 90).
     */
    int imageQuality() const;

    /**
     * @brief Устанавливает качество сохранения изображений.
     * @param quality Качество в процентах.
     */
    void setImageQuality(int quality);

    /**
     * @brief Сохранять ли EXIF-метаданные изображений.
     * @return true, если EXIF сохраняется (по умолчанию).
     */
    bool imagePreserveExif() const;

    /**
     * @brief Включает или отключает сохранение EXIF-данных.
     * @param on true — сохранять EXIF.
     */
    void setImagePreserveExif(bool on);

    /**
     * @brief Выполнять ли автоматическую ориентацию изображения по EXIF.
     * @return true, если автоориентация включена (по умолчанию).
     */
    bool imageAutoOrient() const;

    /**
     * @brief Включает или отключает автоориентацию изображений.
     * @param on true — поворачивать по данным EXIF.
     */
    void setImageAutoOrient(bool on);

    /**
     * @brief Возвращает размер истории отмены в редакторе изображений.
     * @return Число шагов истории (по умолчанию 50).
     */
    int imageHistorySize() const;

    /**
     * @brief Устанавливает размер истории отмены редактора изображений.
     * @param size Число шагов истории.
     */
    void setImageHistorySize(int size);

    /**
     * @brief Включено ли плавное масштабирование изображений.
     * @return true, если плавный зум активен (по умолчанию).
     */
    bool imageSmoothZoom() const;

    /**
     * @brief Включает или отключает плавное масштабирование.
     * @param on true — использовать плавный зум.
     */
    void setImageSmoothZoom(bool on);

    /**
     * @brief Показывать ли сетку в редакторе изображений.
     * @return true, если сетка отображается (по умолчанию скрыта).
     */
    bool imageShowGrid() const;

    /**
     * @brief Включает или отключает показ сетки.
     * @param on true — показывать сетку.
     */
    void setImageShowGrid(bool on);

    /**
     * @brief Возвращает шаг (размер ячейки) сетки.
     * @return Размер сетки (по умолчанию 8.0).
     */
    double imageGridSize() const;

    /**
     * @brief Устанавливает шаг сетки в редакторе изображений.
     * @param size Размер ячейки сетки.
     */
    void setImageGridSize(double size);

    /**
     * @brief Возвращает алгоритм масштабирования изображений.
     * @return Название алгоритма (по умолчанию "Bilinear").
     */
    QString imageResizeAlgorithm() const;

    /**
     * @brief Устанавливает алгоритм масштабирования изображений.
     * @param algo Название алгоритма ("Bilinear", "Nearest" и т.п.).
     */
    void setImageResizeAlgorithm(const QString &algo);

    /**
     * @brief Сохранять ли пропорции при изменении размера изображения.
     * @return true, если пропорции фиксируются (по умолчанию).
     */
    bool imageKeepAspect() const;

    /**
     * @brief Включает или отключает сохранение пропорций изображения.
     * @param on true — сохранять пропорции.
     */
    void setImageKeepAspect(bool on);

    /**
     * @brief Разрешено ли увеличение изображения сверх исходного размера.
     * @return true, если апскейл разрешён (по умолчанию запрещён).
     */
    bool imageAllowUpscale() const;

    /**
     * @brief Разрешает или запрещает апскейл изображений.
     * @param on true — разрешить увеличение.
     */
    void setImageAllowUpscale(bool on);

private:
    /** @brief Закрытый конструктор: создаёт QSettings с INI-файлом по умолчанию. */
    SettingsManager();

    /** @brief Деструктор по умолчанию. */
    ~SettingsManager() = default;

    /** @brief Копирование запрещено (синглтон). */
    SettingsManager(const SettingsManager &) = delete;

    /** @brief Присваивание запрещено (синглтон). */
    SettingsManager &operator=(const SettingsManager &) = delete;

    /** @brief Объект Qt-настроек, хранящий все параметры в INI-формате. */
    QSettings m_settings;
};

#endif // SETTINGSMANAGER_H
