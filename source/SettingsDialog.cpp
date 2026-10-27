#include "SettingsDialog.h"
#include "SettingsManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QListWidget>
#include <QLineEdit>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QFontComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QHeaderView>
#include <QTreeWidget>
#include <QFormLayout>
#include <QScrollArea>
#include <QColorDialog>
#include <QFont>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Настройки программы");
    resize(700, 550);
    setupUI();
    loadSettings();

    connect(this, &QDialog::accepted, this, &SettingsDialog::saveSettings);
}

void SettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createFontPage(), "Шрифты");
    m_tabWidget->addTab(createScalePage(), "Масштабирование");
    m_tabWidget->addTab(createPluginPage(), "Плагины");
    m_tabWidget->addTab(createFormatPage(), "Форматы");
    m_tabWidget->addTab(createAppearancePage(), "Внешний вид");
    m_tabWidget->addTab(createDictionaryPage(), "Словари");
    m_tabWidget->addTab(createAutosavePage(), "Автосохранение");
    m_tabWidget->addTab(createGitPage(), "Git");
    mainLayout->addWidget(m_tabWidget, 1);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *btnApply = new QPushButton("Применить", this);
    QPushButton *btnOk = new QPushButton("ОК", this);
    QPushButton *btnCancel = new QPushButton("Отмена", this);
    btnLayout->addWidget(btnApply);
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnApply, &QPushButton::clicked, this, [this]() { saveSettings(); });

    setStyleSheet(R"(
        QTabWidget::pane { border: 1px solid #c0c0c0; background: white; padding: 8px; }
        QGroupBox { font-weight: bold; border: 1px solid #c0c0c0; border-radius: 4px;
                     margin-top: 8px; padding-top: 16px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
        QPushButton { padding: 5px 16px; min-width: 70px; }
    )");
}

// ===================== Load =====================
void SettingsDialog::loadSettings()
{
    SettingsManager &s = SettingsManager::instance();

    // Font
    m_cmbEditorFont->setCurrentFont(QFont(s.editorFontFamily()));
    m_spnEditorSize->setValue(s.editorFontSize());
    m_chkEditorAntialias->setChecked(s.editorFontAntialias());

    // Scale
    m_chkAutoScale->setChecked(s.autoScale());
    m_sldScale->setValue(s.scaleFactor());
    m_chkScaleFactors->setChecked(s.useScaleFactors());
    m_chkIntegerOnly->setChecked(s.integerScaleOnly());

    // Appearance
    int theme = s.theme();
    m_rbLight->setChecked(theme == 0);
    m_rbDark->setChecked(theme == 1);
    m_rbSystem->setChecked(theme == 2);
    m_btnAccent->setStyleSheet(QString("background-color: %1; border: 1px solid #c0c0c0;").arg(s.accentColor()));
    m_btnBg->setStyleSheet(QString("background-color: %1; border: 1px solid #c0c0c0;").arg(s.backgroundColor()));
    m_chkShowIcons->setChecked(s.showRibbonIcons());
    m_chkLargeIcons->setChecked(s.largeRibbonIcons());
    m_cmbRibbonStyle->setCurrentIndex(s.ribbonStyle());
    m_spnSidebarWidth->setValue(s.sidebarWidth());
    m_chkAutoHide->setChecked(s.autoHideSidebar());
    m_chkTabIcons->setChecked(s.showTabIcons());

    // Dictionaries
    m_edtDictPath->setText(s.dictionaryPath());
    QStringList enabledDicts = s.enabledDictionaries();
    for (int i = 0; i < m_treeDicts->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treeDicts->topLevelItem(i);
        item->setCheckState(0, enabledDicts.contains(item->text(0)) ? Qt::Checked : Qt::Unchecked);
    }

    // Plugins
    QStringList enabledPlugins = s.enabledPlugins();
    for (int i = 0; i < m_treePlugins->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treePlugins->topLevelItem(i);
        bool enabled = enabledPlugins.contains(item->text(0));
        item->setCheckState(0, enabled ? Qt::Checked : Qt::Unchecked);
        item->setText(3, enabled ? "Включён" : "Отключён");
    }

    // Formats
    QStringList enabledFormats = s.enabledFormats();
    for (int i = 0; i < m_treeFormats->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treeFormats->topLevelItem(i);
        bool enabled = enabledFormats.contains(item->text(0));
        item->setCheckState(0, enabled ? Qt::Checked : Qt::Unchecked);
        item->setText(3, enabled ? "Включён" : "Отключён");
    }

    // Autosave
    m_chkAutoSave->setChecked(s.autoSaveEnabled());
    m_spnInterval->setValue(s.autoSaveInterval());
    m_spnInterval->setEnabled(s.autoSaveEnabled());
    m_chkSaveOnExit->setChecked(s.saveOnExit());
    m_chkSaveBeforeExport->setChecked(s.saveBeforeExport());
    m_chkKeepHistory->setChecked(s.keepHistory());
    m_spnMaxVersions->setValue(s.maxVersions());
    m_chkAutoClean->setChecked(s.autoCleanHistory());
    m_edtBackupPath->setText(s.backupPath());
    m_chkCompress->setChecked(s.compressBackups());

    // Git
    m_edtGitLogin->setText(s.value("git/login", "").toString());
    m_edtGitPassword->setText(s.value("git/password", "").toString());
    m_edtGitApiKey->setText(s.value("git/apiKey", "").toString());
    m_edtGitRemoteUrl->setText(s.value("git/remoteUrl", "").toString());
}

// ===================== Save =====================
void SettingsDialog::saveSettings()
{
    SettingsManager &s = SettingsManager::instance();

    // Font
    s.setEditorFontFamily(m_cmbEditorFont->currentFont().family());
    s.setEditorFontSize(m_spnEditorSize->value());
    s.setEditorFontAntialias(m_chkEditorAntialias->isChecked());

    // Scale
    s.setAutoScale(m_chkAutoScale->isChecked());
    s.setScaleFactor(m_sldScale->value());
    s.setUseScaleFactors(m_chkScaleFactors->isChecked());
    s.setIntegerScaleOnly(m_chkIntegerOnly->isChecked());

    // Appearance
    int theme = 0;
    if (m_rbDark->isChecked()) theme = 1;
    else if (m_rbSystem->isChecked()) theme = 2;
    s.setTheme(theme);
    s.setAccentColor(m_btnAccent->palette().color(QPalette::Button).name());
    s.setBackgroundColor(m_btnBg->palette().color(QPalette::Button).name());
    s.setShowRibbonIcons(m_chkShowIcons->isChecked());
    s.setLargeRibbonIcons(m_chkLargeIcons->isChecked());
    s.setRibbonStyle(m_cmbRibbonStyle->currentIndex());
    s.setSidebarWidth(m_spnSidebarWidth->value());
    s.setAutoHideSidebar(m_chkAutoHide->isChecked());
    s.setShowTabIcons(m_chkTabIcons->isChecked());

    // Dictionaries
    QStringList dictList;
    for (int i = 0; i < m_treeDicts->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treeDicts->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            dictList.append(item->text(0));
    }
    s.setEnabledDictionaries(dictList);
    s.setDictionaryPath(m_edtDictPath->text());

    // Plugins
    QStringList pluginList;
    for (int i = 0; i < m_treePlugins->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treePlugins->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            pluginList.append(item->text(0));
    }
    s.setEnabledPlugins(pluginList);

    // Formats
    QStringList formatList;
    for (int i = 0; i < m_treeFormats->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treeFormats->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            formatList.append(item->text(0));
    }
    s.setEnabledFormats(formatList);

    // Autosave
    s.setAutoSaveEnabled(m_chkAutoSave->isChecked());
    s.setAutoSaveInterval(m_spnInterval->value());
    s.setSaveOnExit(m_chkSaveOnExit->isChecked());
    s.setSaveBeforeExport(m_chkSaveBeforeExport->isChecked());
    s.setKeepHistory(m_chkKeepHistory->isChecked());
    s.setMaxVersions(m_spnMaxVersions->value());
    s.setAutoCleanHistory(m_chkAutoClean->isChecked());
    s.setBackupPath(m_edtBackupPath->text());
    s.setCompressBackups(m_chkCompress->isChecked());

    // Git
    s.setValue("git/login", m_edtGitLogin->text().trimmed());
    s.setValue("git/password", m_edtGitPassword->text().trimmed());
    s.setValue("git/apiKey", m_edtGitApiKey->text().trimmed());
    s.setValue("git/remoteUrl", m_edtGitRemoteUrl->text().trimmed());

    s.sync();
}

// ===================== 2.1 Шрифты =====================
QWidget *SettingsDialog::createFontPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *grpEditor = new QGroupBox("Редактор", page);
    QFormLayout *formEditor = new QFormLayout(grpEditor);

    m_cmbEditorFont = new QFontComboBox(grpEditor);
    m_spnEditorSize = new QSpinBox(grpEditor);
    m_spnEditorSize->setRange(6, 72);
    m_spnEditorSize->setSuffix(" pt");
    m_chkEditorAntialias = new QCheckBox("Сглаживание", grpEditor);

    formEditor->addRow("Шрифт:", m_cmbEditorFont);
    formEditor->addRow("Размер:", m_spnEditorSize);
    formEditor->addRow("", m_chkEditorAntialias);

    QGroupBox *grpPreview = new QGroupBox("Предпросмотр", page);
    QVBoxLayout *prevLayout = new QVBoxLayout(grpPreview);
    m_lblFontPreview = new QLabel("Пример текста ABCabc 0123", grpPreview);
    m_lblFontPreview->setFrameStyle(QFrame::StyledPanel);
    m_lblFontPreview->setMinimumHeight(60);
    m_lblFontPreview->setAlignment(Qt::AlignCenter);
    prevLayout->addWidget(m_lblFontPreview);

    layout->addWidget(grpEditor);
    layout->addWidget(grpPreview);
    layout->addStretch();

    connect(m_cmbEditorFont, &QFontComboBox::currentFontChanged, this, [this](const QFont &f) {
        QFont font = f;
        font.setPointSize(m_spnEditorSize->value());
        m_lblFontPreview->setFont(font);
    });

    connect(m_spnEditorSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        QFont font = m_cmbEditorFont->currentFont();
        font.setPointSize(val);
        m_lblFontPreview->setFont(font);
    });

    return page;
}

// ===================== 2.2 Масштабирование =====================
QWidget *SettingsDialog::createScalePage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *grpResolution = new QGroupBox("Поддержка разрешений", page);
    QVBoxLayout *resLayout = new QVBoxLayout(grpResolution);
    QLabel *lblInfo = new QLabel("Приложение адаптируется к разрешению экрана от 800x600 до 8K (7680x4320).", grpResolution);
    lblInfo->setWordWrap(true);
    resLayout->addWidget(lblInfo);

    QFormLayout *formRes = new QFormLayout();
    QLabel *lblMinVal = new QLabel("800 x 600", grpResolution);
    lblMinVal->setStyleSheet("font-weight: bold;");
    formRes->addRow("Минимальное разрешение:", lblMinVal);
    QLabel *lblMaxVal = new QLabel("7680 x 4320 (8K UHD)", grpResolution);
    lblMaxVal->setStyleSheet("font-weight: bold;");
    formRes->addRow("Максимальное разрешение:", lblMaxVal);
    resLayout->addLayout(formRes);
    layout->addWidget(grpResolution);

    QGroupBox *grpScale = new QGroupBox("Масштабирование интерфейса", page);
    QFormLayout *formScale = new QFormLayout(grpScale);

    m_chkAutoScale = new QCheckBox("Автоматическое масштабирование", grpScale);
    formScale->addRow("", m_chkAutoScale);

    m_sldScale = new QSlider(Qt::Horizontal, grpScale);
    m_sldScale->setRange(50, 300);
    m_sldScale->setTickPosition(QSlider::TicksBelow);
    m_sldScale->setTickInterval(25);
    m_lblScaleVal = new QLabel("100%", grpScale);
    m_lblScaleVal->setMinimumWidth(50);

    QHBoxLayout *scaleRow = new QHBoxLayout();
    scaleRow->addWidget(new QLabel("50%", grpScale));
    scaleRow->addWidget(m_sldScale, 1);
    scaleRow->addWidget(new QLabel("300%", grpScale));
    scaleRow->addWidget(m_lblScaleVal);
    formScale->addRow("Масштаб:", scaleRow);

    QFormLayout *hidpiForm = new QFormLayout();
    m_chkScaleFactors = new QCheckBox("Учитывать коэффициенты масштабирования Qt", grpScale);
    m_chkIntegerOnly = new QCheckBox("Только целые коэффициенты (200%, 300%...)", grpScale);
    hidpiForm->addRow("", m_chkScaleFactors);
    hidpiForm->addRow("", m_chkIntegerOnly);
    QGroupBox *grpHiDPI = new QGroupBox("HiDPI", grpScale);
    grpHiDPI->setLayout(hidpiForm);
    formScale->addRow(grpHiDPI);

    layout->addWidget(grpScale);
    layout->addStretch();

    connect(m_sldScale, &QSlider::valueChanged, this, [this](int val) {
        m_lblScaleVal->setText(QString("%1%").arg(val));
    });

    return page;
}

// ===================== 2.3 Плагины =====================
QWidget *SettingsDialog::createPluginPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *grpList = new QGroupBox("Установленные плагины", page);
    QVBoxLayout *listLayout = new QVBoxLayout(grpList);

    m_treePlugins = new QTreeWidget(grpList);
    m_treePlugins->setHeaderLabels({"Имя", "Версия", "Описание", "Статус"});
    m_treePlugins->setRootIsDecorated(false);
    m_treePlugins->setColumnCount(4);
    m_treePlugins->header()->setStretchLastSection(true);
    m_treePlugins->setAlternatingRowColors(true);

    auto addPlugin = [this](const QString &name, const QString &ver, const QString &desc) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_treePlugins);
        item->setText(0, name);
        item->setText(1, ver);
        item->setText(2, desc);
    };

    addPlugin("Переводчик Google", "1.2", "Автоматический перевод текста");
    addPlugin("Переводчик Яндекс", "1.0", "Перевод через Яндекс.Переводчик");
    addPlugin("Проверка орфографии", "2.0", "Hunspell проверка орфографии");
    addPlugin("Экспорт в Kindle", "1.1", "Генерация MOBI для Kindle");

    m_treePlugins->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_treePlugins->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    listLayout->addWidget(m_treePlugins);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnLoad = new QPushButton("Загрузить плагин...", grpList);
    QPushButton *btnAdd = new QPushButton("Добавить из каталога...", grpList);
    QPushButton *btnRemove = new QPushButton("Удалить", grpList);
    btnLayout->addWidget(btnLoad);
    btnLayout->addWidget(btnAdd);
    btnLayout->addWidget(btnRemove);
    btnLayout->addStretch();
    listLayout->addLayout(btnLayout);
    layout->addWidget(grpList);

    connect(btnLoad, &QPushButton::clicked, [page]() {
        QFileDialog::getOpenFileName(page, "Загрузить плагин", QString(),
            "Файлы плагинов (*.dll *.so);;Все файлы (*)");
    });
    connect(btnAdd, &QPushButton::clicked, [page]() {
        QFileDialog::getExistingDirectory(page, "Выбрать каталог плагинов");
    });

    return page;
}

// ===================== 2.4 Форматы =====================
QWidget *SettingsDialog::createFormatPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *grpList = new QGroupBox("Поддерживаемые форматы", page);
    QVBoxLayout *listLayout = new QVBoxLayout(grpList);

    m_treeFormats = new QTreeWidget(grpList);
    m_treeFormats->setHeaderLabels({"Формат", "Расширение", "Тип", "Статус"});
    m_treeFormats->setRootIsDecorated(false);
    m_treeFormats->setColumnCount(4);
    m_treeFormats->setAlternatingRowColors(true);

    auto addFormat = [this](const QString &name, const QString &ext, const QString &type) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeFormats);
        item->setText(0, name);
        item->setText(1, ext);
        item->setText(2, type);
    };

    addFormat("EPUB", ".epub", "Вход/Выход");
    addFormat("MOBI", ".mobi", "Выход");
    addFormat("PDF", ".pdf", "Выход");
    addFormat("FB2", ".fb2", "Вход/Выход");
    addFormat("FB3", ".fb3", "Вход/Выход");
    addFormat("DOC", ".doc", "Вход");
    addFormat("RTF", ".rtf", "Вход/Выход");
    addFormat("HTML", ".html", "Вход/Выход");
    addFormat("Markdown", ".md", "Вход/Выход");
    addFormat("CHM", ".chm", "Вход");
    addFormat("HLP", ".hlp", "Вход");
    addFormat("EXE", ".exe", "Выход");

    m_treeFormats->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_treeFormats->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    listLayout->addWidget(m_treeFormats);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnLoad = new QPushButton("Загрузить формат...", grpList);
    QPushButton *btnAdd = new QPushButton("Добавить формат...", grpList);
    QPushButton *btnRemove = new QPushButton("Удалить", grpList);
    btnLayout->addWidget(btnLoad);
    btnLayout->addWidget(btnAdd);
    btnLayout->addWidget(btnRemove);
    btnLayout->addStretch();
    listLayout->addLayout(btnLayout);
    layout->addWidget(grpList);

    connect(btnLoad, &QPushButton::clicked, [page]() {
        QFileDialog::getOpenFileName(page, "Загрузить модуль формата", QString(),
            "Библиотеки (*.dll *.so);;Все файлы (*)");
    });
    connect(btnAdd, &QPushButton::clicked, [page]() {
        QFileDialog::getExistingDirectory(page, "Выбрать каталог форматов");
    });

    return page;
}

// ===================== 2.5 Внешний вид =====================
QWidget *SettingsDialog::createAppearancePage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *grpTheme = new QGroupBox("Тема оформления", page);
    QVBoxLayout *themeLayout = new QVBoxLayout(grpTheme);
    QButtonGroup *themeGroup = new QButtonGroup(grpTheme);
    m_rbLight = new QRadioButton("Светлая", grpTheme);
    m_rbDark = new QRadioButton("Тёмная", grpTheme);
    m_rbSystem = new QRadioButton("Системная", grpTheme);
    themeGroup->addButton(m_rbLight);
    themeGroup->addButton(m_rbDark);
    themeGroup->addButton(m_rbSystem);
    themeLayout->addWidget(m_rbLight);
    themeLayout->addWidget(m_rbDark);
    themeLayout->addWidget(m_rbSystem);
    layout->addWidget(grpTheme);

    QGroupBox *grpColors = new QGroupBox("Цвета интерфейса", page);
    QFormLayout *colorForm = new QFormLayout(grpColors);
    m_btnAccent = new QPushButton("   ", grpColors);
    m_btnAccent->setFixedSize(60, 24);
    colorForm->addRow("Акцентный цвет:", m_btnAccent);
    m_btnBg = new QPushButton("   ", grpColors);
    m_btnBg->setFixedSize(60, 24);
    colorForm->addRow("Фон:", m_btnBg);
    layout->addWidget(grpColors);

    QGroupBox *grpRibbon = new QGroupBox("Ribbon-панель", page);
    QFormLayout *ribbonForm = new QFormLayout(grpRibbon);
    m_chkShowIcons = new QCheckBox("Показывать иконки на кнопках", grpRibbon);
    m_chkLargeIcons = new QCheckBox("Крупные иконки (32x32)", grpRibbon);
    m_cmbRibbonStyle = new QComboBox(grpRibbon);
    m_cmbRibbonStyle->addItems({"Office 2007", "Office 2010", "Office 2013", "Плоский"});
    ribbonForm->addRow("", m_chkShowIcons);
    ribbonForm->addRow("", m_chkLargeIcons);
    ribbonForm->addRow("Стиль:", m_cmbRibbonStyle);
    layout->addWidget(grpRibbon);

    QGroupBox *grpSidebar = new QGroupBox("Боковая панель", page);
    QFormLayout *sidebarForm = new QFormLayout(grpSidebar);
    m_spnSidebarWidth = new QSpinBox(grpSidebar);
    m_spnSidebarWidth->setRange(180, 600);
    m_spnSidebarWidth->setSuffix(" px");
    m_chkAutoHide = new QCheckBox("Автоскрытие боковой панели", grpSidebar);
    m_chkTabIcons = new QCheckBox("Показывать иконки на вкладках", grpSidebar);
    sidebarForm->addRow("Ширина:", m_spnSidebarWidth);
    sidebarForm->addRow("", m_chkAutoHide);
    sidebarForm->addRow("", m_chkTabIcons);
    layout->addWidget(grpSidebar);
    layout->addStretch();

    connect(m_btnAccent, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(Qt::blue, this, "Выберите акцентный цвет");
        if (color.isValid())
            m_btnAccent->setStyleSheet(QString("background-color: %1; border: 1px solid #c0c0c0;").arg(color.name()));
    });
    connect(m_btnBg, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(Qt::white, this, "Выберите цвет фона");
        if (color.isValid())
            m_btnBg->setStyleSheet(QString("background-color: %1; border: 1px solid #c0c0c0;").arg(color.name()));
    });

    return page;
}

// ===================== 2.6 Словари =====================
QWidget *SettingsDialog::createDictionaryPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *grpDicts = new QGroupBox("Установленные словари", page);
    QVBoxLayout *dictLayout = new QVBoxLayout(grpDicts);

    m_treeDicts = new QTreeWidget(grpDicts);
    m_treeDicts->setHeaderLabels({"Словарь", "Язык", "Тип", "Размер"});
    m_treeDicts->setRootIsDecorated(false);
    m_treeDicts->setColumnCount(4);
    m_treeDicts->setAlternatingRowColors(true);

    auto addDict = [this](const QString &name, const QString &lang, const QString &type, const QString &size) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeDicts);
        item->setText(0, name);
        item->setText(1, lang);
        item->setText(2, type);
        item->setText(3, size);
    };

    addDict("Русский", "ru_RU", "Орфография", "12 МБ");
    addDict("English (US)", "en_US", "Орфография", "8 МБ");
    addDict("English (UK)", "en_GB", "Орфография", "8 МБ");
    addDict("Немецкий", "de_DE", "Орфография", "9 МБ");
    addDict("Русский → Английский", "ru→en", "Перевод", "15 МБ");
    addDict("Английский → Русский", "en→ru", "Перевод", "15 МБ");
    m_treeDicts->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    dictLayout->addWidget(m_treeDicts);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnAdd = new QPushButton("Добавить словарь...", grpDicts);
    QPushButton *btnDownload = new QPushButton("Скачать из интернета...", grpDicts);
    QPushButton *btnRemove = new QPushButton("Удалить", grpDicts);
    btnLayout->addWidget(btnAdd);
    btnLayout->addWidget(btnDownload);
    btnLayout->addWidget(btnRemove);
    btnLayout->addStretch();
    dictLayout->addLayout(btnLayout);
    layout->addWidget(grpDicts);

    QGroupBox *grpPaths = new QGroupBox("Пути к словарям", page);
    QFormLayout *pathForm = new QFormLayout(grpPaths);
    m_edtDictPath = new QLineEdit(grpPaths);
    m_edtDictPath->setReadOnly(true);
    QPushButton *btnBrowse = new QPushButton("...", grpPaths);
    btnBrowse->setFixedWidth(30);
    QHBoxLayout *pathRow = new QHBoxLayout();
    pathRow->addWidget(m_edtDictPath, 1);
    pathRow->addWidget(btnBrowse);
    pathForm->addRow("Каталог словарей:", pathRow);
    layout->addWidget(grpPaths);
    layout->addStretch();

    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Выбрать каталог словарей");
        if (!dir.isEmpty())
            m_edtDictPath->setText(dir);
    });
    connect(btnAdd, &QPushButton::clicked, [this]() {
        QFileDialog::getOpenFileName(this, "Добавить словарь", QString(),
            "Файлы словарей (*.dic *.aff *.dicx);;Все файлы (*)");
    });

    return page;
}

// ===================== 2.7 Автосохранение =====================
QWidget *SettingsDialog::createAutosavePage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *grpAuto = new QGroupBox("Автосохранение", page);
    QFormLayout *form = new QFormLayout(grpAuto);
    m_chkAutoSave = new QCheckBox("Включить автосохранение", grpAuto);
    form->addRow("", m_chkAutoSave);
    m_spnInterval = new QSpinBox(grpAuto);
    m_spnInterval->setRange(1, 120);
    m_spnInterval->setSuffix(" мин");
    form->addRow("Интервал:", m_spnInterval);
    m_chkSaveOnExit = new QCheckBox("Сохранять при выходе", grpAuto);
    form->addRow("", m_chkSaveOnExit);
    m_chkSaveBeforeExport = new QCheckBox("Сохранять перед запуском экспорта", grpAuto);
    form->addRow("", m_chkSaveBeforeExport);
    layout->addWidget(grpAuto);

    QGroupBox *grpHistory = new QGroupBox("История версий", page);
    QFormLayout *histForm = new QFormLayout(grpHistory);
    m_chkKeepHistory = new QCheckBox("Сохранять историю изменений", grpHistory);
    histForm->addRow("", m_chkKeepHistory);
    m_spnMaxVersions = new QSpinBox(grpHistory);
    m_spnMaxVersions->setRange(1, 100);
    m_spnMaxVersions->setSuffix(" версий");
    histForm->addRow("Макс. версий:", m_spnMaxVersions);
    m_chkAutoClean = new QCheckBox("Автоочистка старых версий", grpHistory);
    histForm->addRow("", m_chkAutoClean);
    layout->addWidget(grpHistory);

    QGroupBox *grpBackup = new QGroupBox("Резервное копирование", page);
    QFormLayout *bakForm = new QFormLayout(grpBackup);
    m_edtBackupPath = new QLineEdit(grpBackup);
    m_edtBackupPath->setReadOnly(true);
    QPushButton *btnBrowse = new QPushButton("...", grpBackup);
    btnBrowse->setFixedWidth(30);
    QHBoxLayout *bakPath = new QHBoxLayout();
    bakPath->addWidget(m_edtBackupPath, 1);
    bakPath->addWidget(btnBrowse);
    bakForm->addRow("Каталог бэкапов:", bakPath);
    m_chkCompress = new QCheckBox("Сжимать бэкапы (ZIP)", grpBackup);
    bakForm->addRow("", m_chkCompress);
    layout->addWidget(grpBackup);
    layout->addStretch();

    connect(m_chkAutoSave, &QCheckBox::toggled, m_spnInterval, &QWidget::setEnabled);
    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Выбрать каталог бэкапов");
        if (!dir.isEmpty())
            m_edtBackupPath->setText(dir);
    });

    return page;
}

// ===================== 2.8 Git =====================
QWidget *SettingsDialog::createGitPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *grpAuth = new QGroupBox("Аутентификация GitHub", page);
    QFormLayout *authForm = new QFormLayout(grpAuth);

    m_edtGitLogin = new QLineEdit(grpAuth);
    m_edtGitLogin->setPlaceholderText("username");
    authForm->addRow("Логин:", m_edtGitLogin);

    m_edtGitPassword = new QLineEdit(grpAuth);
    m_edtGitPassword->setEchoMode(QLineEdit::Password);
    m_edtGitPassword->setPlaceholderText("ghp_... (Personal Access Token)");
    authForm->addRow("Токен (PAT):", m_edtGitPassword);

    m_edtGitApiKey = new QLineEdit(grpAuth);
    m_edtGitApiKey->setPlaceholderText("Необязательно");
    authForm->addRow("API ключ:", m_edtGitApiKey);

    layout->addWidget(grpAuth);

    QGroupBox *grpRemote = new QGroupBox("Удалённый репозиторий", page);
    QFormLayout *remoteForm = new QFormLayout(grpRemote);

    m_edtGitRemoteUrl = new QLineEdit(grpRemote);
    m_edtGitRemoteUrl->setPlaceholderText("https://github.com/user/repo.git");
    remoteForm->addRow("URL:", m_edtGitRemoteUrl);

    layout->addWidget(grpRemote);

    QLabel *hint = new QLabel(
        "Для публикации на GitHub создайте Personal Access Token\n"
        "в настройках GitHub → Developer Settings → Tokens.\n"
        "Токен нужен для доступа к репозиторию (repo scope).", page);
    hint->setStyleSheet("color: gray; font-size: 10px;");
    layout->addWidget(hint);

    layout->addStretch();
    return page;
}

// ===================== ImageEditorSettingsDialog =====================
ImageEditorSettingsDialog::ImageEditorSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Настройки редактора изображений");
    resize(550, 450);
    setupUI();
    loadSettings();
    connect(this, &QDialog::accepted, this, &ImageEditorSettingsDialog::saveSettings);
}

void ImageEditorSettingsDialog::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    QTabWidget *tabs = new QTabWidget(this);

    QWidget *tabGeneral = new QWidget(this);
    QFormLayout *formGeneral = new QFormLayout(tabGeneral);
    m_cmbDefaultFormat = new QComboBox(tabGeneral);
    m_cmbDefaultFormat->addItems({"PNG", "JPEG", "BMP", "GIF", "TIFF"});
    formGeneral->addRow("Формат по умолчанию:", m_cmbDefaultFormat);
    m_spnQuality = new QSpinBox(tabGeneral);
    m_spnQuality->setRange(1, 100);
    m_spnQuality->setSuffix("%");
    formGeneral->addRow("Качество:", m_spnQuality);
    m_chkPreserveExif = new QCheckBox("Сохранять EXIF данные", tabGeneral);
    formGeneral->addRow("", m_chkPreserveExif);
    m_chkAutoOrient = new QCheckBox("Авто-ориентация по EXIF", tabGeneral);
    formGeneral->addRow("", m_chkAutoOrient);
    tabs->addTab(tabGeneral, "Основные");

    QWidget *tabEditor = new QWidget(this);
    QFormLayout *formEditor = new QFormLayout(tabEditor);
    m_spnHistorySize = new QSpinBox(tabEditor);
    m_spnHistorySize->setRange(10, 200);
    formEditor->addRow("Шаги отмены:", m_spnHistorySize);
    m_chkSmoothZoom = new QCheckBox("Плавное масштабирование", tabEditor);
    formEditor->addRow("", m_chkSmoothZoom);
    m_chkShowGrid = new QCheckBox("Показывать сетку при выделении", tabEditor);
    formEditor->addRow("", m_chkShowGrid);
    m_spnGridSize = new QDoubleSpinBox(tabEditor);
    m_spnGridSize->setRange(1, 100);
    m_spnGridSize->setSuffix(" px");
    formEditor->addRow("Размер сетки:", m_spnGridSize);
    tabs->addTab(tabEditor, "Редактор");

    QWidget *tabResize = new QWidget(this);
    QFormLayout *formResize = new QFormLayout(tabResize);
    m_cmbResizeAlgo = new QComboBox(tabResize);
    m_cmbResizeAlgo->addItems({"Bilinear", "Bicubic", "Lanczos", "Nearest neighbor"});
    formResize->addRow("Алгоритм масштабирования:", m_cmbResizeAlgo);
    m_chkKeepAspect = new QCheckBox("Сохранять пропорции", tabResize);
    formResize->addRow("", m_chkKeepAspect);
    m_chkUpScale = new QCheckBox("Разрешить увеличение изображений", tabResize);
    formResize->addRow("", m_chkUpScale);
    tabs->addTab(tabResize, "Изменение размера");

    layout->addWidget(tabs);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *btnOk = new QPushButton("ОК", this);
    QPushButton *btnCancel = new QPushButton("Отмена", this);
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    layout->addLayout(btnLayout);

    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);

    setStyleSheet(R"(
        QTabWidget::pane { border: 1px solid #c0c0c0; background: white; padding: 8px; }
        QGroupBox { font-weight: bold; border: 1px solid #c0c0c0; border-radius: 4px;
                     margin-top: 8px; padding-top: 16px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
        QPushButton { padding: 5px 16px; min-width: 70px; }
    )");
}

void ImageEditorSettingsDialog::loadSettings()
{
    SettingsManager &s = SettingsManager::instance();
    int idx = m_cmbDefaultFormat->findText(s.imageDefaultFormat());
    if (idx >= 0) m_cmbDefaultFormat->setCurrentIndex(idx);
    m_spnQuality->setValue(s.imageQuality());
    m_chkPreserveExif->setChecked(s.imagePreserveExif());
    m_chkAutoOrient->setChecked(s.imageAutoOrient());
    m_spnHistorySize->setValue(s.imageHistorySize());
    m_chkSmoothZoom->setChecked(s.imageSmoothZoom());
    m_chkShowGrid->setChecked(s.imageShowGrid());
    m_spnGridSize->setValue(s.imageGridSize());
    int algoIdx = m_cmbResizeAlgo->findText(s.imageResizeAlgorithm());
    if (algoIdx >= 0) m_cmbResizeAlgo->setCurrentIndex(algoIdx);
    m_chkKeepAspect->setChecked(s.imageKeepAspect());
    m_chkUpScale->setChecked(s.imageAllowUpscale());
}

void ImageEditorSettingsDialog::saveSettings()
{
    SettingsManager &s = SettingsManager::instance();
    s.setImageDefaultFormat(m_cmbDefaultFormat->currentText());
    s.setImageQuality(m_spnQuality->value());
    s.setImagePreserveExif(m_chkPreserveExif->isChecked());
    s.setImageAutoOrient(m_chkAutoOrient->isChecked());
    s.setImageHistorySize(m_spnHistorySize->value());
    s.setImageSmoothZoom(m_chkSmoothZoom->isChecked());
    s.setImageShowGrid(m_chkShowGrid->isChecked());
    s.setImageGridSize(m_spnGridSize->value());
    s.setImageResizeAlgorithm(m_cmbResizeAlgo->currentText());
    s.setImageKeepAspect(m_chkKeepAspect->isChecked());
    s.setImageAllowUpscale(m_chkUpScale->isChecked());
    s.sync();
}
