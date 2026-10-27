#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QTabWidget;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QSlider;
class QListWidget;
class QLineEdit;
class QGroupBox;
class QFontComboBox;
class QPushButton;
class QRadioButton;
class QTreeWidget;
class QLabel;

/**
 * @brief Диалог настроек приложения (7 вкладок).
 *
 * Вкладки:
 *   1. Шрифт — выбор шрифта, размера, сглаживания для редактора
 *   2. Масштаб — автоматический/ручной масштаб UI (100%-300%)
 *   3. Плагины — список загруженных плагинов (имя, версия, статус)
 *   4. Форматы — список поддерживаемых форматов и расширений
 *   5. Внешний вид — тема (светлая/тёмная/системная), цвет акцента,
 *     размер иконок, стиль ленты, ширина боковой панели
 *   6. Словари — управление словарями проверки орфографии
 *   7. Автосохранение — интервал, сохранение при выходе, история версий
 *
 * Все настройки сохраняются через SettingsManager.
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private:
    void setupUI();         ///< Создаёт QTabWidget с 7 вкладками
    void loadSettings();    ///< Загружает текущие значения из SettingsManager
    void saveSettings();    ///< Сохраняет значения в SettingsManager и применяет

    QWidget *createFontPage();      ///< Вкладка «Шрифт»
    QWidget *createScalePage();     ///< Вкладка «Масштаб»
    QWidget *createPluginPage();    ///< Вкладка «Плагины»
    QWidget *createFormatPage();    ///< Вкладка «Форматы»
    QWidget *createAppearancePage();///< Вкладка «Внешний вид»
    QWidget *createDictionaryPage();///< Вкладка «Словари»
    QWidget *createAutosavePage();  ///< Вкладка «Автосохранение»
    QWidget *createGitPage();       ///< Вкладка «Git»

    QTabWidget *m_tabWidget;       ///< Главный tab widget

    // Вкладка «Шрифт»
    QFontComboBox *m_cmbEditorFont; ///< Выбор шрифта
    QSpinBox *m_spnEditorSize;      ///< Размер шрифта
    QCheckBox *m_chkEditorAntialias; ///< Сглаживание шрифта
    QLabel *m_lblFontPreview;       ///< Пример шрифта

    // Вкладка «Масштаб»
    QCheckBox *m_chkAutoScale;      ///< Автоматический масштаб по DPI
    QSlider *m_sldScale;            ///< Ручной ползунок масштаба (100-300%)
    QLabel *m_lblScaleVal;          ///< Текущее значение масштаба
    QCheckBox *m_chkScaleFactors;   ///< Использовать предустановленные коэффициенты
    QCheckBox *m_chkIntegerOnly;    ///< Только целочисленные коэффициенты

    // Вкладка «Плагины»
    QTreeWidget *m_treePlugins;     ///< Дерево загруженных плагинов

    // Вкладка «Форматы»
    QTreeWidget *m_treeFormats;     ///< Дерево поддерживаемых форматов

    // Вкладка «Внешний вид»
    QRadioButton *m_rbLight;        ///< Светлая тема
    QRadioButton *m_rbDark;         ///< Тёмная тема
    QRadioButton *m_rbSystem;       ///< Системная тема
    QPushButton *m_btnAccent;       ///< Выбор цвета акцента
    QPushButton *m_btnBg;           ///< Выбор цвета фона
    QCheckBox *m_chkShowIcons;      ///< Показывать иконки на кнопках
    QCheckBox *m_chkLargeIcons;     ///< Крупные иконки
    QComboBox *m_cmbRibbonStyle;    ///< Стиль ленты (классический/вкладки)
    QSpinBox *m_spnSidebarWidth;    ///< Ширина боковой панели (px)
    QCheckBox *m_chkAutoHide;       ///< Автоскрытие боковой панели
    QCheckBox *m_chkTabIcons;       ///< Иконки на вкладках редактора

    // Вкладка «Словари»
    QTreeWidget *m_treeDicts;       ///< Дерево установленных словарей
    QLineEdit *m_edtDictPath;       ///< Путь к каталогу словарей

    // Вкладка «Автосохранение»
    QCheckBox *m_chkAutoSave;       ///< Включить автосохранение
    QSpinBox *m_spnInterval;        ///< Интервал автосохранения (мин)
    QCheckBox *m_chkSaveOnExit;     ///< Сохранять при выходе
    QCheckBox *m_chkSaveBeforeExport; ///< Сохранять перед экспортом
    QCheckBox *m_chkKeepHistory;    ///< Хранить историю версий
    QSpinBox *m_spnMaxVersions;     ///< Макс. число версий в истории
    QCheckBox *m_chkAutoClean;      ///< Автоочистка старых версий
    QLineEdit *m_edtBackupPath;     ///< Путь для резервных копий
    QCheckBox *m_chkCompress;       ///< Сжимать резервные копии

    // Git
    QLineEdit *m_edtGitLogin;       ///< Логин GitHub
    QLineEdit *m_edtGitPassword;    ///< Токен доступа (PAT)
    QLineEdit *m_edtGitApiKey;      ///< API ключ (опционально)
    QLineEdit *m_edtGitRemoteUrl;   ///< URL удалённого репозитория
};

/**
 * @brief Диалог настроек редактора изображений.
 *
 * Настройки:
 *   - Формат по умолчанию (PNG/JPG/BMP)
 *   - Качество сжатия (1-100)
 *   - Сохранение EXIF-данных
 *   - Автоповорот
 *   - Размер истории отмен
 *   - Плавный зум
 *   - Сетка
 *   - Алгоритм масштабирования (ilinear/bilinear/bicubic)
 *   - Сохранение пропорций
 */
class ImageEditorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImageEditorSettingsDialog(QWidget *parent = nullptr);

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    QComboBox *m_cmbDefaultFormat;  ///< Формат по умолчанию
    QSpinBox *m_spnQuality;        ///< Качество JPEG (1-100)
    QCheckBox *m_chkPreserveExif;  ///< Сохранять EXIF
    QCheckBox *m_chkAutoOrient;    ///< Автоповорот по EXIF
    QSpinBox *m_spnHistorySize;    ///< Размер истории отмен
    QCheckBox *m_chkSmoothZoom;    ///< Плавный зум
    QCheckBox *m_chkShowGrid;      ///< Показывать сетку
    QDoubleSpinBox *m_spnGridSize; ///< Размер ячейки сетки
    QComboBox *m_cmbResizeAlgo;    ///< Алгоритм масштабирования
    QCheckBox *m_chkKeepAspect;    ///< Сохранять пропорции
    QCheckBox *m_chkUpScale;       ///< Разрешать увеличение
};

#endif // SETTINGSDIALOG_H
