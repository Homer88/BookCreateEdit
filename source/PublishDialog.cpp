#include "PublishDialog.h"
#include "BookDocument.h"
#include "ProjectManager.h"
#include "PluginManager.h"
#include "interfaces/IBookFormat.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QFile>
#include <QHash>
#include <QDesktopServices>
#include <QUrl>

/**
 * @brief Генерирует иконку формата: скруглённый цветной прямоугольник
 *        с расширением формата по центру.
 *
 * Цвет выбирается детерминированно из имени формата — иконки стабильны
 * между запусками. Если в ресурсах появится файл fmt_<ext>.svg, он будет
 * использован вместо сгенерированной картинки.
 */
static QIcon makeFormatIcon(const QString &formatName, const QString &ext)
{
    // Приоритет — SVG-иконка из ресурсов (динамическая подгрузка)
    const QString qrcPath = ":/icons/fmt_" + ext + ".svg";
    if (QFile::exists(qrcPath))
        return QIcon(qrcPath);

    // Детерминированный цвет по имени формата
    quint32 h = 2166136261u;                       // FNV-1a
    for (QChar c : formatName) {
        h ^= c.unicode();
        h *= 16777619u;
    }
    const int hue = static_cast<int>(h % 360);

    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QColor base = QColor::fromHsl(hue, 150, 140);
    QLinearGradient grad(0, 0, 0, 64);
    grad.setColorAt(0, base.lighter(125));
    grad.setColorAt(1, base.darker(120));
    p.setBrush(grad);
    p.setPen(QPen(base.darker(160), 2));
    p.drawRoundedRect(4, 4, 56, 56, 10, 10);

    QString label = ext.toUpper();
    if (label.size() > 5)
        label = label.left(5);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(label.size() > 3 ? 12 : 15);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(pm.rect().adjusted(2, 2, -2, -2), Qt::AlignCenter, label);
    p.end();

    return QIcon(pm);
}

PublishDialog::PublishDialog(BookDocument *doc, ProjectManager *project,
                             QWidget *parent)
    : QDialog(parent)
    , m_document(doc)
    , m_project(project)
{
    setWindowTitle("Опубликовать");
    resize(820, 720);
    setMinimumSize(700, 620);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // --- Сетка форматов с иконками (как «Создать проект» в Visual Studio) ---
    QGroupBox *grpFormat = new QGroupBox(
        "Форматы публикации (можно выбрать несколько)", this);
    QVBoxLayout *fmtLayout = new QVBoxLayout(grpFormat);

    m_lstFormats = new QListWidget(grpFormat);
    m_lstFormats->setViewMode(QListView::IconMode);
    m_lstFormats->setResizeMode(QListView::Adjust);
    m_lstFormats->setMovement(QListView::Static);
    // Широкие плитки: при стандартной ширине окна в ряду помещается
    // около 4 форматов, названия («Help & Manual», «FictionBook2»)
    // умещаются целиком без переносов и обрезки
    m_lstFormats->setGridSize(QSize(184, 108));
    m_lstFormats->setIconSize(QSize(48, 48));
    m_lstFormats->setSpacing(6);
    m_lstFormats->setWordWrap(true);
    m_lstFormats->setSelectionMode(QAbstractItemView::ExtendedSelection);
    populateFormats();
    // Видно ~3 ряда, остальные доступны прокруткой
    m_lstFormats->setMinimumHeight(qMax(360, 3 * 108 + 36));
    fmtLayout->addWidget(m_lstFormats);

    mainLayout->addWidget(grpFormat, 1);

    // --- Параметры книги ---
    QGroupBox *grpOutput = new QGroupBox("Книга", this);
    QFormLayout *outLayout = new QFormLayout(grpOutput);

    m_edtBookName = new QLineEdit(grpOutput);
    outLayout->addRow("Название книги:", m_edtBookName);

    QHBoxLayout *dirRow = new QHBoxLayout();
    m_edtOutputDir = new QLineEdit(grpOutput);
    QPushButton *btnBrowse = new QPushButton("Обзор...", grpOutput);
    connect(btnBrowse, &QPushButton::clicked, this, &PublishDialog::onBrowse);
    dirRow->addWidget(m_edtOutputDir, 1);
    dirRow->addWidget(btnBrowse);
    outLayout->addRow("Сохранить в:", dirRow);

    mainLayout->addWidget(grpOutput);

    // --- Информация из файла проекта ---
    m_lblProjectInfo = new QLabel(this);
    m_lblProjectInfo->setStyleSheet("color: gray;");
    m_lblProjectInfo->setWordWrap(true);
    if (m_project && !m_project->filePath().isEmpty()) {
        m_lblProjectInfo->setText(QString(
            "Проект: «%1», год %2, издательство «%3». Эти данные будут "
            "включены в книгу.")
            .arg(m_project->bookTitle(),
                 QString::number(m_project->bookYear()),
                 m_project->bookPublisher().isEmpty()
                     ? QStringLiteral("—")
                     : m_project->bookPublisher()));
    } else {
        m_lblProjectInfo->setText(
            "Проект не открыт: метаданные берутся из текущего документа.");
    }
    mainLayout->addWidget(m_lblProjectInfo);

    // --- Опции и кнопки ---
    QHBoxLayout *bottomRow = new QHBoxLayout();
    m_chkOpenAfter = new QCheckBox("Открыть после публикации", this);
    m_chkOpenAfter->setChecked(true);
    bottomRow->addWidget(m_chkOpenAfter);
    bottomRow->addStretch();

    QPushButton *btnCancel = new QPushButton("Отмена", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    QPushButton *btnPublish = new QPushButton("Опубликовать", this);
    btnPublish->setDefault(true);
    btnPublish->setMinimumWidth(130);
    connect(btnPublish, &QPushButton::clicked, this, &PublishDialog::onPublish);
    bottomRow->addWidget(btnCancel);
    bottomRow->addWidget(btnPublish);
    mainLayout->addLayout(bottomRow);

    updateDefaults();

    setStyleSheet(R"(
        QGroupBox { font-weight: bold; border: 1px solid #c0c0c0; border-radius: 4px;
                     margin-top: 8px; padding-top: 16px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
        QPushButton { padding: 5px 16px; min-width: 70px; }
        QListWidget { background: white; }
    )");
}

void PublishDialog::populateFormats()
{
    m_lstFormats->clear();

    // Иконки появляются строго по загруженным плагинам форматов
    const QStringList formatNames = PluginManager::instance().availableFormats();
    for (const QString &name : formatNames) {
        IBookFormat *fmt = PluginManager::instance().format(name);
        if (!fmt || !fmt->canWrite())
            continue;

        QListWidgetItem *item = new QListWidgetItem(m_lstFormats);
        item->setText(fmt->formatName());
        item->setIcon(makeFormatIcon(fmt->formatName(), fmt->fileExtension()));
        item->setData(Qt::UserRole, name);                    // id плагина
        item->setData(Qt::UserRole + 1, fmt->fileExtension());
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                       Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setToolTip(QString("%1\nРасширения: %2")
            .arg(fmt->description(),
                 fmt->supportedExtensions().join(", ")));
    }
}

void PublishDialog::updateDefaults()
{
    // Название книги: проект → документ → «book»
    QString title;
    if (m_project && !m_project->bookTitle().isEmpty())
        title = m_project->bookTitle();
    else if (m_document && !m_document->meta().title.isEmpty())
        title = m_document->meta().title;
    else
        title = "book";
    m_edtBookName->setText(title);

    // Каталог: каталог проекта → рабочий каталог
    m_edtOutputDir->setText(
        (m_project && !m_project->filePath().isEmpty())
            ? m_project->projectDir() : QDir::currentPath());
}

void PublishDialog::applyProjectMetaToDocument()
{
    if (!m_document)
        return;

    BookMeta meta = m_document->meta();

    // Требование dosc.md №4: данные книги берутся из файла проекта
    if (m_project && !m_project->filePath().isEmpty()) {
        if (!m_project->bookTitle().isEmpty())
            meta.title = m_project->bookTitle();
    }

    // Название из строки диалога имеет наивысший приоритет
    if (!m_edtBookName->text().trimmed().isEmpty())
        meta.title = m_edtBookName->text().trimmed();

    m_document->setMeta(meta);
}

QStringList collectCheckedFormats(QListWidget *lst)
{
    QStringList ids;
    for (int i = 0; i < lst->count(); ++i)
        if (lst->item(i)->checkState() == Qt::Checked)
            ids << lst->item(i)->data(Qt::UserRole).toString();
    return ids;
}

void PublishDialog::onBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        "Каталог сохранения книги", m_edtOutputDir->text());
    if (!dir.isEmpty())
        m_edtOutputDir->setText(dir);
}

void PublishDialog::onPublish()
{
    // 1. Хоть один формат должен быть отмечен
    QStringList checked = collectCheckedFormats(m_lstFormats);
    if (checked.isEmpty()) {
        QMessageBox::warning(this, "Публикация",
                             "Отметьте хотя бы один формат (флажок на иконке).");
        return;
    }

    QString name = m_edtBookName->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Публикация", "Введите название книги.");
        return;
    }
    name.replace(QRegExp("[\\\\/:*?\"<>|]"), "_");

    QDir outDir(m_edtOutputDir->text().trimmed());
    if (outDir.relativeFilePath(".") != "." && !outDir.exists())
        outDir.mkpath(".");
    if (!outDir.exists()) {
        QMessageBox::warning(this, "Публикация",
            "Каталог сохранения недоступен:\n" + outDir.absolutePath());
        return;
    }

    applyProjectMetaToDocument();

    // 2. Последовательная публикация во все выбранные форматы
    QStringList okFiles, failFormats;
    for (const QString &id : checked) {
        IBookFormat *fmt = PluginManager::instance().format(id);
        if (!fmt) { failFormats << "?"; continue; }

        QString filePath = outDir.filePath(name + "." + fmt->fileExtension());
        if (fmt->write(m_document, filePath))
            okFiles << filePath;
        else
            failFormats << fmt->formatName();
    }

    // 3. Отчёт о результате
    if (!failFormats.isEmpty()) {
        QMessageBox::warning(this, "Публикация",
            QString("Не удалось опубликовать в форматы: %1")
                .arg(failFormats.join(", ")));
    }
    if (okFiles.isEmpty())
        return;

    QMessageBox::information(this, "Публикация",
        QString("Опубликовано файлов: %1\n%2")
            .arg(okFiles.size()).arg(okFiles.join("\n")));

    // Открываем первый успешный файл (при нескольких — только его,
    // чтобы не заваливать систему окнами)
    if (m_chkOpenAfter->isChecked())
        QDesktopServices::openUrl(QUrl::fromLocalFile(okFiles.first()));

    accept();
}
