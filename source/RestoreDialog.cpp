#include "RestoreDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>

RestoreDialog::RestoreDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Восстановить из автосохранений");
    resize(550, 400);
    setupUI();
    loadBackups();
}

void RestoreDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *lblInfo = new QLabel("Выберите версию для восстановления:", this);
    lblInfo->setStyleSheet("font-weight: bold;");
    mainLayout->addWidget(lblInfo);

    m_treeBackups = new QTreeWidget(this);
    m_treeBackups->setHeaderLabels({"Дата", "Время", "Размер", "Путь"});
    m_treeBackups->setRootIsDecorated(false);
    m_treeBackups->setColumnCount(4);
    m_treeBackups->setAlternatingRowColors(true);
    m_treeBackups->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeBackups->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_treeBackups->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    mainLayout->addWidget(m_treeBackups, 1);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *btnRestore = new QPushButton("Восстановить", this);
    QPushButton *btnPreview = new QPushButton("Просмотр", this);
    QPushButton *btnCancel = new QPushButton("Отмена", this);
    btnLayout->addWidget(btnRestore);
    btnLayout->addWidget(btnPreview);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnRestore, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnPreview, &QPushButton::clicked, this, [this]() {
        QTreeWidgetItem *item = m_treeBackups->currentItem();
        if (item) {
            QString path = item->text(3);
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });

    setStyleSheet(R"(
        QPushButton { padding: 5px 16px; min-width: 70px; }
    )");
}

void RestoreDialog::loadBackups()
{
    QDir backupDir("./history");
    if (!backupDir.exists())
        backupDir.mkdir(".");

    QStringList filters;
    filters << "*.bak" << "*.zip" << "*.backup";
    QFileInfoList files = backupDir.entryInfoList(filters, QDir::Files, QDir::Time);

    if (files.isEmpty()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeBackups);
        item->setText(0, "Автосохранения не найдены");
        item->setText(1, "");
        item->setText(2, "");
        item->setText(3, "./history");
        return;
    }

    for (const QFileInfo &fi : files) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeBackups);
        item->setText(0, fi.lastModified().toString("dd.MM.yyyy"));
        item->setText(1, fi.lastModified().toString("HH:mm:ss"));
        qint64 size = fi.size();
        QString sizeStr;
        if (size > 1024 * 1024)
            sizeStr = QString("%1 МБ").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
        else if (size > 1024)
            sizeStr = QString("%1 КБ").arg(size / 1024.0, 0, 'f', 1);
        else
            sizeStr = QString("%1 Б").arg(size);
        item->setText(2, sizeStr);
        item->setText(3, fi.absoluteFilePath());
    }
}
