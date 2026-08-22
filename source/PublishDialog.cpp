#include "PublishDialog.h"
#include "BookDocument.h"
#include "PluginManager.h"
#include "interfaces/IBookFormat.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QTreeWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>

PublishDialog::PublishDialog(BookDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_document(doc)
{
    setWindowTitle("Опубликовать");
    resize(550, 450);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QGroupBox *grpFormat = new QGroupBox("Формат книги", this);
    QVBoxLayout *fmtLayout = new QVBoxLayout(grpFormat);

    m_treeFormats = new QTreeWidget(grpFormat);
    m_treeFormats->setHeaderLabels({"Формат", "Расширение"});
    m_treeFormats->setRootIsDecorated(false);
    m_treeFormats->setColumnCount(2);
    m_treeFormats->setAlternatingRowColors(true);
    m_treeFormats->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeFormats->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_treeFormats->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    populateFormats();
    fmtLayout->addWidget(m_treeFormats);

    if (m_treeFormats->topLevelItemCount() > 0) {
        m_treeFormats->setCurrentItem(m_treeFormats->topLevelItem(0));
    }

    mainLayout->addWidget(grpFormat);

    QGroupBox *grpOutput = new QGroupBox("Путь сохранения", this);
    QFormLayout *outLayout = new QFormLayout(grpOutput);
    m_edtOutputPath = new QLineEdit(grpOutput);
    m_edtOutputPath->setPlaceholderText("Выберите путь для сохранения...");
    QPushButton *btnBrowse = new QPushButton("...", grpOutput);
    btnBrowse->setFixedWidth(30);
    QHBoxLayout *pathRow = new QHBoxLayout();
    pathRow->addWidget(m_edtOutputPath, 1);
    pathRow->addWidget(btnBrowse);
    outLayout->addRow("Каталог:", pathRow);
    mainLayout->addWidget(grpOutput);

    QGroupBox *grpOptions = new QGroupBox("Дополнительно", this);
    QVBoxLayout *optLayout = new QVBoxLayout(grpOptions);
    m_chkOpenAfter = new QCheckBox("Открыть после публикации", grpOptions);
    m_chkOpenAfter->setChecked(true);
    optLayout->addWidget(m_chkOpenAfter);
    mainLayout->addWidget(grpOptions);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *btnPublish = new QPushButton("Опубликовать", this);
    QPushButton *btnCancel = new QPushButton("Отмена", this);
    btnPublish->setMinimumWidth(120);
    btnLayout->addWidget(btnPublish);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnPublish, &QPushButton::clicked, this, &PublishDialog::onPublish);
    connect(btnBrowse, &QPushButton::clicked, this, &PublishDialog::onBrowse);
    connect(m_treeFormats, &QTreeWidget::currentItemChanged,
            this, [this]() { onFormatChanged(); });

    setStyleSheet(R"(
        QGroupBox { font-weight: bold; border: 1px solid #c0c0c0; border-radius: 4px;
                     margin-top: 8px; padding-top: 16px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
        QPushButton { padding: 5px 16px; min-width: 70px; }
    )");
}

void PublishDialog::populateFormats()
{
    m_treeFormats->clear();

    QStringList formatNames = PluginManager::instance().availableFormats();
    for (const QString &name : formatNames) {
        IBookFormat *fmt = PluginManager::instance().format(name);
        if (fmt && fmt->canWrite()) {
            QTreeWidgetItem *item = new QTreeWidgetItem(m_treeFormats);
            item->setText(0, fmt->formatName());
            item->setText(1, fmt->supportedExtensions().join(", "));
            item->setData(0, Qt::UserRole, name);
        }
    }
}

void PublishDialog::onFormatChanged()
{
    QTreeWidgetItem *item = m_treeFormats->currentItem();
    if (!item) return;

    QString formatId = item->data(0, Qt::UserRole).toString();
    IBookFormat *fmt = PluginManager::instance().format(formatId);
    if (!fmt) return;

    QString ext = fmt->fileExtension();
    QString defaultName = "book";

    if (m_document) {
        BookMeta m = m_document->meta();
        if (!m.title.isEmpty())
            defaultName = m.title;
    }

    defaultName.replace(QRegExp("[\\\\/:*?\"<>|]"), "_");
    QString defaultPath = QDir::currentPath() + "/" + defaultName + "." + ext;
    m_edtOutputPath->setText(defaultPath);
}

void PublishDialog::onBrowse()
{
    QTreeWidgetItem *item = m_treeFormats->currentItem();
    QString ext = "html";
    if (item) {
        QString formatId = item->data(0, Qt::UserRole).toString();
        IBookFormat *fmt = PluginManager::instance().format(formatId);
        if (fmt)
            ext = fmt->fileExtension();
    }

    QString filePath = QFileDialog::getSaveFileName(this, "Сохранить как",
                                                     m_edtOutputPath->text(),
                                                     QString("*.%1").arg(ext));
    if (!filePath.isEmpty())
        m_edtOutputPath->setText(filePath);
}

void PublishDialog::onPublish()
{
    QTreeWidgetItem *item = m_treeFormats->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Ошибка", "Выберите формат.");
        return;
    }

    QString outputPath = m_edtOutputPath->text().trimmed();
    if (outputPath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Укажите путь для сохранения.");
        return;
    }

    QString formatId = item->data(0, Qt::UserRole).toString();
    IBookFormat *fmt = PluginManager::instance().format(formatId);
    if (!fmt) {
        QMessageBox::critical(this, "Ошибка", "Плагин формата не найден.");
        return;
    }

    if (!m_document) {
        QMessageBox::critical(this, "Ошибка", "Документ не создан.");
        return;
    }

    QFileInfo fi(outputPath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists())
        dir.mkpath(".");

    bool ok = fmt->write(m_document, outputPath);

    if (ok) {
        QMessageBox::information(this, "Публикация",
            QString("Книга успешно опубликована:\n%1").arg(outputPath));

        if (m_chkOpenAfter->isChecked())
            QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));

        accept();
    } else {
        QMessageBox::warning(this, "Ошибка",
            "Не удалось опубликовать книгу.\nПроверьте права на запись.");
    }
}
