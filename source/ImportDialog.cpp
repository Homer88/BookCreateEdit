#include "ImportDialog.h"
#include "BookDocument.h"
#include "PluginManager.h"
#include "interfaces/IBookFormat.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QApplication>

ImportDialog::ImportDialog(BookDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_document(doc)
{
    setWindowTitle("Импорт файла");
    setMinimumWidth(500);
    resize(550, 200);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QFormLayout *formLayout = new QFormLayout();

    m_formatCombo = new QComboBox(this);
    buildFormatList();
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportDialog::onFormatChanged);
    formLayout->addRow("Формат:", m_formatCombo);

    QHBoxLayout *fileLayout = new QHBoxLayout();
    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setPlaceholderText("Выберите файл для импорта...");
    QPushButton *btnBrowse = new QPushButton("Обзор...", this);
    connect(btnBrowse, &QPushButton::clicked, this, &ImportDialog::onBrowse);
    fileLayout->addWidget(m_filePathEdit);
    fileLayout->addWidget(btnBrowse);
    formLayout->addRow("Файл:", fileLayout);

    mainLayout->addLayout(formLayout);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: gray;");
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *btnCancel = new QPushButton("Отмена", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(btnCancel);

    QPushButton *btnImport = new QPushButton("Импорт", this);
    btnImport->setDefault(true);
    btnImport->setMinimumWidth(100);
    connect(btnImport, &QPushButton::clicked, this, &ImportDialog::onImport);
    btnLayout->addWidget(btnImport);

    mainLayout->addLayout(btnLayout);

    if (m_formatCombo->count() > 0)
        updateFileFilter();
}

void ImportDialog::buildFormatList()
{
    m_formatCombo->clear();

    QStringList formatNames = PluginManager::instance().availableFormats();
    for (const QString &name : formatNames) {
        IBookFormat *fmt = PluginManager::instance().format(name);
        if (fmt && fmt->canRead()) {
            m_formatCombo->addItem(fmt->formatName(), name);
        }
    }

    if (m_formatCombo->count() == 0) {
        m_formatCombo->addItem("(нет загруженных форматов)", QString());
        m_formatCombo->setEnabled(false);
    }
}

void ImportDialog::onFormatChanged(int index)
{
    Q_UNUSED(index);
    updateFileFilter();
}

void ImportDialog::updateFileFilter()
{
    int idx = m_formatCombo->currentIndex();
    if (idx < 0) return;

    QString formatId = m_formatCombo->currentData().toString();
    IBookFormat *fmt = PluginManager::instance().format(formatId);
    if (!fmt) return;

    QStringList exts = fmt->supportedExtensions();
    QString filter;
    for (const QString &ext : exts) {
        if (!filter.isEmpty()) filter += " ";
        filter += "*." + ext;
    }
    filter = QString("%1 (%2);;Все файлы (*)").arg(fmt->formatName(), filter);

    m_statusLabel->setText(QString("Поддерживаемые расширения: %1").arg(exts.join(", ")));
}

void ImportDialog::onBrowse()
{
    int idx = m_formatCombo->currentIndex();
    QString filter = "Все файлы (*)";

    if (idx >= 0) {
        QString formatId = m_formatCombo->currentData().toString();
        IBookFormat *fmt = PluginManager::instance().format(formatId);
        if (fmt) {
            QStringList exts = fmt->supportedExtensions();
            QString specificFilter;
            for (const QString &ext : exts) {
                if (!specificFilter.isEmpty()) specificFilter += " ";
                specificFilter += "*." + ext;
            }
            filter = QString("%1 (%2);;Все файлы (*)").arg(fmt->formatName(), specificFilter);
        }
    }

    QString filePath = QFileDialog::getOpenFileName(this, "Выберите файл для импорта",
                                                     QString(), filter);
    if (!filePath.isEmpty()) {
        m_filePathEdit->setText(filePath);

        QFileInfo fi(filePath);
        QString ext = fi.suffix().toLower();

        for (int i = 0; i < m_formatCombo->count(); ++i) {
            QString formatId = m_formatCombo->itemData(i).toString();
            IBookFormat *fmt = PluginManager::instance().format(formatId);
            if (fmt && fmt->supportedExtensions().contains(ext)) {
                m_formatCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void ImportDialog::onImport()
{
    QString filePath = m_filePathEdit->text().trimmed();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "Импорт", "Выберите файл для импорта.");
        return;
    }

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        QMessageBox::warning(this, "Импорт", "Файл не найден: " + filePath);
        return;
    }

    int idx = m_formatCombo->currentIndex();
    if (idx < 0) {
        QMessageBox::warning(this, "Импорт", "Выберите формат.");
        return;
    }

    QString formatId = m_formatCombo->currentData().toString();
    IBookFormat *fmt = PluginManager::instance().format(formatId);
    if (!fmt) {
        QMessageBox::critical(this, "Импорт", "Плагин формата не найден.");
        return;
    }

    if (!m_document) {
        QMessageBox::critical(this, "Импорт", "Документ не создан.");
        return;
    }

    m_statusLabel->setText("Импорт...");

    bool ok = fmt->read(filePath, m_document);

    if (ok) {
        QMessageBox::information(this, "Импорт",
            QString("Файл успешно импортирован.\nСтраниц в документе: %1")
                .arg(m_document->pageCount()));
        accept();
    } else {
        m_statusLabel->setText("Ошибка импорта.");
        QMessageBox::warning(this, "Импорт",
            "Не удалось импортировать файл.\nПроверьте формат файла.");
    }
}
