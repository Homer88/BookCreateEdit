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

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    QWidget *createFontPage();
    QWidget *createScalePage();
    QWidget *createPluginPage();
    QWidget *createFormatPage();
    QWidget *createAppearancePage();
    QWidget *createDictionaryPage();
    QWidget *createAutosavePage();

    QTabWidget *m_tabWidget;

    // Font
    QFontComboBox *m_cmbEditorFont;
    QSpinBox *m_spnEditorSize;
    QCheckBox *m_chkEditorAntialias;
    QLabel *m_lblFontPreview;

    // Scale
    QCheckBox *m_chkAutoScale;
    QSlider *m_sldScale;
    QLabel *m_lblScaleVal;
    QCheckBox *m_chkScaleFactors;
    QCheckBox *m_chkIntegerOnly;

    // Plugins
    QTreeWidget *m_treePlugins;

    // Formats
    QTreeWidget *m_treeFormats;

    // Appearance
    QRadioButton *m_rbLight;
    QRadioButton *m_rbDark;
    QRadioButton *m_rbSystem;
    QPushButton *m_btnAccent;
    QPushButton *m_btnBg;
    QCheckBox *m_chkShowIcons;
    QCheckBox *m_chkLargeIcons;
    QComboBox *m_cmbRibbonStyle;
    QSpinBox *m_spnSidebarWidth;
    QCheckBox *m_chkAutoHide;
    QCheckBox *m_chkTabIcons;

    // Dictionaries
    QTreeWidget *m_treeDicts;
    QLineEdit *m_edtDictPath;

    // Autosave
    QCheckBox *m_chkAutoSave;
    QSpinBox *m_spnInterval;
    QCheckBox *m_chkSaveOnExit;
    QCheckBox *m_chkSaveBeforeExport;
    QCheckBox *m_chkKeepHistory;
    QSpinBox *m_spnMaxVersions;
    QCheckBox *m_chkAutoClean;
    QLineEdit *m_edtBackupPath;
    QCheckBox *m_chkCompress;
};

class ImageEditorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImageEditorSettingsDialog(QWidget *parent = nullptr);

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    QComboBox *m_cmbDefaultFormat;
    QSpinBox *m_spnQuality;
    QCheckBox *m_chkPreserveExif;
    QCheckBox *m_chkAutoOrient;
    QSpinBox *m_spnHistorySize;
    QCheckBox *m_chkSmoothZoom;
    QCheckBox *m_chkShowGrid;
    QDoubleSpinBox *m_spnGridSize;
    QComboBox *m_cmbResizeAlgo;
    QCheckBox *m_chkKeepAspect;
    QCheckBox *m_chkUpScale;
};

#endif // SETTINGSDIALOG_H
