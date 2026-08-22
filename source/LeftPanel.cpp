#include "LeftPanel.h"
#include "BookDocument.h"

#include <QVBoxLayout>
#include <QTabWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLabel>
#include <QHeaderView>
#include <QLineEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QMenu>
#include <QAction>
#include <QInputDialog>

LeftPanel::LeftPanel(QWidget *parent)
    : QWidget(parent)
    , m_document(nullptr)
{
    setupUI();
    setStyleSheet(R"(
        LeftPanel {
            background-color: #f0f0f0;
            border-right: 1px solid #c0c0c0;
        }
    )");
}

void LeftPanel::setDocument(BookDocument *doc)
{
    m_document = doc;
    if (m_document) {
        connect(m_document, &BookDocument::modified, this, &LeftPanel::refreshContents);
        connect(m_document, &BookDocument::pageAdded, this, &LeftPanel::refreshContents);
        connect(m_document, &BookDocument::pageRemoved, this, &LeftPanel::refreshContents);
    }
    populateFromDocument();
}

void LeftPanel::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #c0c0c0;
            background: white;
        }
        QTabBar::tab {
            background: #e8e8e8;
            border: 1px solid #c0c0c0;
            padding: 5px 12px;
            margin-right: 1px;
        }
        QTabBar::tab:selected {
            background: white;
            border-bottom: 1px solid white;
            font-weight: bold;
        }
    )");

    m_treeContents = new QTreeWidget(m_tabWidget);
    m_treeContents->setHeaderLabels({"Оглавление"});
    m_treeContents->setRootIsDecorated(true);
    m_treeContents->setAnimated(true);
    m_treeContents->setStyleSheet(R"(
        QTreeWidget { border: none; }
        QTreeWidget::item { padding: 3px; }
        QTreeWidget::item:selected { background-color: #d0e8ff; }
    )");
    connect(m_treeContents, &QTreeWidget::itemClicked,
            this, &LeftPanel::onTreeItemClicked);

    m_treeContents->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeContents, &QTreeWidget::customContextMenuRequested,
            this, &LeftPanel::showContextMenu);

    m_fileList = new QListWidget(m_tabWidget);
    m_fileList->setStyleSheet(R"(
        QListWidget { border: none; }
        QListWidget::item { padding: 4px; }
        QListWidget::item:selected { background-color: #d0e8ff; }
    )");

    m_projectSettings = new QTreeWidget(m_tabWidget);
    m_projectSettings->setHeaderHidden(true);
    m_projectSettings->setRootIsDecorated(false);
    m_projectSettings->setColumnCount(2);
    m_projectSettings->header()->setStretchLastSection(true);
    m_projectSettings->setStyleSheet(R"(
        QTreeWidget { border: none; }
        QTreeWidget::item { padding: 3px; }
    )");

    m_tabWidget->addTab(m_treeContents, "Оглавление");
    m_tabWidget->addTab(m_fileList, "Файлы");
    m_tabWidget->addTab(m_projectSettings, "Свойства");

    layout->addWidget(m_tabWidget);
}

void LeftPanel::populateFromDocument()
{
    m_treeContents->clear();
    m_fileList->clear();
    m_projectSettings->clear();

    if (!m_document) {
        new QTreeWidgetItem(m_treeContents, {"(нет документа)"});
        return;
    }

    for (int i = 0; i < m_document->pageCount(); ++i) {
        QString pageId = m_document->pageId(i);
        QString pageXml = m_document->pageXml(i);

        QString title = QString("Страница %1 [%2]").arg(i + 1).arg(pageId);

        QRegularExpression re("<h[1-6][^>]*>([^<]*)</h[1-6]>",
                              QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = re.match(pageXml);
        if (match.hasMatch()) {
            title = match.captured(1).trimmed();
            if (title.isEmpty())
                title = QString("Страница %1").arg(i + 1);
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeContents, {title});
        item->setData(0, Qt::UserRole, i);
        item->setToolTip(0, QString("Страница %1").arg(i + 1) + " — " + pageId);
    }

    BookMeta m = m_document->meta();

    QTreeWidgetItem *itemTitle = new QTreeWidgetItem(m_projectSettings, {"Название", m.title});
    QTreeWidgetItem *itemAuthor = new QTreeWidgetItem(m_projectSettings, {"Автор", m.author});
    QTreeWidgetItem *itemLang = new QTreeWidgetItem(m_projectSettings, {"Язык", m.language});
    QTreeWidgetItem *itemPages = new QTreeWidgetItem(m_projectSettings,
        {"Страниц", QString::number(m_document->pageCount())});
    QString filePath = m_document->filePath().isEmpty() ? "(не сохранён)" : m_document->filePath();
    QTreeWidgetItem *itemFile = new QTreeWidgetItem(m_projectSettings,
        {"Файл", filePath});

    m_projectSettings->resizeColumnToContents(0);
}

void LeftPanel::refreshContents()
{
    populateFromDocument();
}

void LeftPanel::onTreeItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;

    bool ok = false;
    int pageIndex = item->data(0, Qt::UserRole).toInt(&ok);
    if (ok && pageIndex >= 0 && pageIndex < m_document->pageCount()) {
        emit pageSelected(pageIndex);
    }
}

void LeftPanel::onMetaEdited()
{
    if (!m_document) return;

    BookMeta m = m_document->meta();
    m.title = m_edtTitle ? m_edtTitle->text() : m.title;
    m.author = m_edtAuthor ? m_edtAuthor->text() : m.author;
    m.language = m_edtLanguage ? m_edtLanguage->text() : m.language;
    m_document->setMeta(m);
}

void LeftPanel::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_treeContents->itemAt(pos);
    QMenu menu;

    QAction *actAdd = menu.addAction("Добавить страницу");
    QAction *actDelete = nullptr;
    QAction *actRename = nullptr;

    if (item) {
        actDelete = menu.addAction("Удалить страницу");
        actRename = menu.addAction("Переименовать");
        menu.addSeparator();
    }

    QAction *selected = menu.exec(m_treeContents->viewport()->mapToGlobal(pos));
    if (!selected) return;

    if (selected == actAdd) {
        if (m_document) {
            int insertAt = -1;
            if (item) {
                insertAt = item->data(0, Qt::UserRole).toInt();
            }
            m_document->addPage(insertAt);
            refreshContents();
        }
    } else if (selected == actDelete && item) {
        int idx = item->data(0, Qt::UserRole).toInt();
        if (m_document && m_document->pageCount() > 1) {
            m_document->removePage(idx);
            refreshContents();
            emit pageDeleted(idx);
        }
    } else if (selected == actRename && item) {
        int idx = item->data(0, Qt::UserRole).toInt();
        if (m_document) {
            bool ok;
            QString newTitle = QInputDialog::getText(this, "Переименовать",
                "Название:", QLineEdit::Normal, item->text(0), &ok);
            if (ok && !newTitle.isEmpty()) {
                emit pageRenamed(idx, newTitle);
            }
        }
    }
}
