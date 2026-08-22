#include "EditorTabs.h"
#include "BookDocument.h"
#include "XmlEditor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QTabWidget>
#include <QToolButton>
#include <QLabel>
#include <QScrollBar>
#include <QTextStream>

EditorTabs::EditorTabs(QWidget *parent)
    : QWidget(parent)
    , m_document(nullptr)
    , m_currentPageIndex(0)
    , m_syncing(false)
    , m_lastWysiwygHash(0)
    , m_lastXmlHash(0)
{
    m_syncWysiwygTimer.setSingleShot(true);
    m_syncWysiwygTimer.setInterval(250);
    m_syncXmlTimer.setSingleShot(true);
    m_syncXmlTimer.setInterval(250);

    connect(&m_syncWysiwygTimer, &QTimer::timeout, this, &EditorTabs::syncWysiwygToXml);
    connect(&m_syncXmlTimer, &QTimer::timeout, this, &EditorTabs::syncXmlToWysiwyg);

    setupUI();
}

void EditorTabs::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QHBoxLayout *topBar = new QHBoxLayout();
    topBar->setContentsMargins(4, 4, 4, 4);
    topBar->setSpacing(4);

    QLabel *lblPages = new QLabel("Страницы:", this);
    lblPages->setStyleSheet("font-weight: bold;");
    topBar->addWidget(lblPages);

    m_pageTabs = new QTabWidget(this);
    m_pageTabs->setTabsClosable(true);
    m_pageTabs->setMovable(true);
    m_pageTabs->setMaximumHeight(32);
    topBar->addWidget(m_pageTabs, 1);

    QToolButton *btnAddPage = new QToolButton(this);
    btnAddPage->setText("+");
    btnAddPage->setToolTip("Добавить страницу");
    btnAddPage->setFixedSize(28, 24);
    btnAddPage->setStyleSheet("QToolButton { font-weight: bold; font-size: 14px; }");
    topBar->addWidget(btnAddPage);

    QToolButton *btnRemovePage = new QToolButton(this);
    btnRemovePage->setText("-");
    btnRemovePage->setToolTip("Удалить страницу");
    btnRemovePage->setFixedSize(28, 24);
    btnRemovePage->setStyleSheet("QToolButton { font-weight: bold; font-size: 14px; }");
    topBar->addWidget(btnRemovePage);

    mainLayout->addLayout(topBar);

    m_viewTabs = new QTabWidget(this);

    m_wysiwygPage = new QWidget(this);
    QVBoxLayout *wysiwygLayout = new QVBoxLayout(m_wysiwygPage);
    wysiwygLayout->setContentsMargins(0, 0, 0, 0);
    m_wysiwygEditor = new QTextEdit(m_wysiwygPage);
    m_wysiwygEditor->setPlaceholderText("Начните вводить текст...");
    m_wysiwygEditor->setStyleSheet(R"(
        QTextEdit {
            background-color: white;
            border: 1px solid #c0c0c0;
            font-family: "Times New Roman", serif;
            font-size: 14px;
            padding: 20px;
        }
    )");
    wysiwygLayout->addWidget(m_wysiwygEditor);

    m_xmlPage = new QWidget(this);
    QVBoxLayout *xmlLayout = new QVBoxLayout(m_xmlPage);
    xmlLayout->setContentsMargins(0, 0, 0, 0);
    m_xmlEditor = new XmlEditor(m_xmlPage);
    xmlLayout->addWidget(m_xmlEditor);

    m_viewTabs->addTab(m_wysiwygPage, "Редактирование (WYSIWYG)");
    m_viewTabs->addTab(m_xmlPage, "XML");

    mainLayout->addWidget(m_viewTabs, 1);

    connect(m_viewTabs, &QTabWidget::currentChanged, this, &EditorTabs::onTabChanged);
    connect(m_pageTabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx >= 0 && !m_syncing) {
            saveCurrentPage();
            m_currentPageIndex = idx;
            loadPage(idx);
            emit currentPageChanged(idx);
        }
    });
    connect(m_pageTabs, &QTabWidget::tabCloseRequested, this, [this](int idx) {
        if (m_pageTabs->count() > 1) {
            removePage(idx);
        }
    });
    connect(m_wysiwygEditor, &QTextEdit::textChanged, this, &EditorTabs::onWysiwygChanged);
    connect(m_xmlEditor, &XmlEditor::contentChanged, this, &EditorTabs::onXmlChanged);
    connect(btnAddPage, &QToolButton::clicked, this, &EditorTabs::onAddPage);
    connect(btnRemovePage, &QToolButton::clicked, this, &EditorTabs::onRemovePage);
}

void EditorTabs::setDocument(BookDocument *doc)
{
    m_document = doc;
    rebuildPageTabs();
    if (m_document && m_document->pageCount() > 0) {
        m_currentPageIndex = 0;
        loadPage(0);
    }
}

BookDocument *EditorTabs::document() const
{
    return m_document;
}

void EditorTabs::newDocument()
{
    if (m_document) {
        m_document->newDocument();
        rebuildPageTabs();
        if (m_document->pageCount() > 0) {
            m_currentPageIndex = 0;
            loadPage(0);
        }
    }
}

void EditorTabs::setCurrentPage(int index)
{
    if (index >= 0 && index < m_pageTabs->count()) {
        saveCurrentPage();
        m_currentPageIndex = index;
        m_pageTabs->setCurrentIndex(index);
        loadPage(index);
    }
}

int EditorTabs::currentPage() const
{
    return m_currentPageIndex;
}

int EditorTabs::pageCount() const
{
    return m_pageTabs->count();
}

QString EditorTabs::currentPageXml() const
{
    if (!m_document)
        return QString();
    return m_document->pageXml(m_currentPageIndex);
}

void EditorTabs::addPage()
{
    onAddPage();
}

void EditorTabs::removeCurrentPage()
{
    onRemovePage();
}

void EditorTabs::setFont(const QFont &font)
{
    m_wysiwygEditor->setFont(font);
}

void EditorTabs::rebuildPageTabs()
{
    m_syncing = true;
    m_pageTabs->clear();
    if (!m_document)
        return;

    for (int i = 0; i < m_document->pageCount(); ++i) {
        QString title = QString("Стр. %1").arg(i + 1);
        QWidget *placeholder = new QWidget();
        m_pageTabs->addTab(placeholder, title);
    }
    m_syncing = false;
}

int EditorTabs::xmlContentHash(const QString &xml) const
{
    return qHash(xml);
}

int EditorTabs::htmlContentHash(const QString &html) const
{
    return qHash(html);
}

// === Переключение вкладок (WYSIWYG ↔ XML) ===

void EditorTabs::onTabChanged(int index)
{
    if (m_syncing)
        return;

    m_syncWysiwygTimer.stop();
    m_syncXmlTimer.stop();

    if (index == 0) {
        syncFromXml();
    } else if (index == 1) {
        syncFromWysiwyg();
    }
}

void EditorTabs::syncFromWysiwyg()
{
    if (m_syncing || !m_document)
        return;

    QString html = m_wysiwygEditor->toHtml();
    int newHash = htmlContentHash(html);
    if (newHash == m_lastWysiwygHash)
        return;

    m_syncing = true;
    m_lastWysiwygHash = newHash;

    QString xml = wysiwygToXml(html);
    m_xmlEditor->setXmlContent(xml);
    m_lastXmlHash = xmlContentHash(xml);

    m_document->setPageXml(m_currentPageIndex, xml);
    m_syncing = false;

    emit contentModified();
}

void EditorTabs::syncFromXml()
{
    if (m_syncing || !m_document)
        return;

    QString xml = m_xmlEditor->xmlContent();
    int newHash = xmlContentHash(xml);
    if (newHash == m_lastXmlHash)
        return;

    m_syncing = true;
    m_lastXmlHash = newHash;

    QString html = xmlToWysiwyg(xml);
    bool oldState = m_wysiwygEditor->blockSignals(true);
    m_wysiwygEditor->setHtml(html);
    m_wysiwygEditor->blockSignals(oldState);
    m_lastWysiwygHash = htmlContentHash(html);

    m_document->setPageXml(m_currentPageIndex, xml);
    m_syncing = false;

    emit contentModified();
}

// === Реалтайм-синхронизация через debounce ===

void EditorTabs::onWysiwygChanged()
{
    if (m_syncing || !m_document)
        return;

    m_syncWysiwygTimer.start();
}

void EditorTabs::onXmlChanged()
{
    if (m_syncing || !m_document)
        return;

    m_syncXmlTimer.start();
}

void EditorTabs::syncWysiwygToXml()
{
    if (m_syncing || m_viewTabs->currentIndex() != 0)
        return;

    QString html = m_wysiwygEditor->toHtml();
    int newHash = htmlContentHash(html);
    if (newHash == m_lastWysiwygHash)
        return;

    m_syncing = true;
    m_lastWysiwygHash = newHash;

    QString xml = wysiwygToXml(html);

    int cursorPos = m_xmlEditor->textCursor().position();
    m_xmlEditor->setXmlContent(xml);
    m_lastXmlHash = xmlContentHash(xml);

    QTextCursor cursor = m_xmlEditor->textCursor();
    cursor.setPosition(qMin(cursorPos, m_xmlEditor->toPlainText().length()));
    m_xmlEditor->setTextCursor(cursor);

    m_document->setPageXml(m_currentPageIndex, xml);
    m_syncing = false;

    emit contentModified();
}

void EditorTabs::syncXmlToWysiwyg()
{
    if (m_syncing || m_viewTabs->currentIndex() != 1)
        return;

    QString xml = m_xmlEditor->xmlContent();
    int newHash = xmlContentHash(xml);
    if (newHash == m_lastXmlHash)
        return;

    m_syncing = true;
    m_lastXmlHash = newHash;

    QString html = xmlToWysiwyg(xml);

    int scrollPos = m_wysiwygEditor->verticalScrollBar()->value();
    bool oldState = m_wysiwygEditor->blockSignals(true);
    m_wysiwygEditor->setHtml(html);
    m_wysiwygEditor->blockSignals(oldState);
    m_wysiwygEditor->verticalScrollBar()->setValue(scrollPos);
    m_lastWysiwygHash = htmlContentHash(html);

    m_document->setPageXml(m_currentPageIndex, xml);
    m_syncing = false;

    emit contentModified();
}

// === Загрузка / сохранение страницы ===

void EditorTabs::saveCurrentPage()
{
    if (!m_document)
        return;

    if (m_viewTabs->currentIndex() == 0) {
        syncFromWysiwyg();
    } else {
        syncFromXml();
    }
}

void EditorTabs::loadPage(int index)
{
    if (!m_document || index < 0 || index >= m_document->pageCount())
        return;

    m_syncWysiwygTimer.stop();
    m_syncXmlTimer.stop();

    m_syncing = true;

    QString xml = m_document->pageXml(index);
    QString html = xmlToWysiwyg(xml);

    bool oldState = m_wysiwygEditor->blockSignals(true);
    m_wysiwygEditor->setHtml(html);
    m_wysiwygEditor->blockSignals(oldState);

    m_xmlEditor->setXmlContent(xml);

    m_lastWysiwygHash = htmlContentHash(html);
    m_lastXmlHash = xmlContentHash(xml);

    m_syncing = false;
}

void EditorTabs::onAddPage()
{
    if (!m_document)
        return;

    saveCurrentPage();
    m_document->addPage(m_currentPageIndex);
    rebuildPageTabs();
    m_pageTabs->setCurrentIndex(m_document->pageCount() - 1);
    m_currentPageIndex = m_document->pageCount() - 1;
    loadPage(m_currentPageIndex);
    emit pageCountChanged(m_document->pageCount());
}

void EditorTabs::onRemovePage()
{
    if (!m_document || m_document->pageCount() <= 1)
        return;

    m_document->removePage(m_currentPageIndex);
    rebuildPageTabs();

    if (m_currentPageIndex >= m_document->pageCount())
        m_currentPageIndex = m_document->pageCount() - 1;

    loadPage(m_currentPageIndex);
    emit pageCountChanged(m_document->pageCount());
}

void EditorTabs::removePage(int index)
{
    if (!m_document || m_document->pageCount() <= 1)
        return;

    saveCurrentPage();
    m_document->removePage(index);
    rebuildPageTabs();

    if (m_currentPageIndex >= m_document->pageCount())
        m_currentPageIndex = m_document->pageCount() - 1;

    loadPage(m_currentPageIndex);
    emit pageCountChanged(m_document->pageCount());
}

// === Конвертеры HTML ↔ XML ===

QString EditorTabs::wysiwygToXml(const QString &html) const
{
    QString xml = html;

    QRegularExpression bodyRe("<body[^>]*>([\\s\\S]*)</body>");
    QRegularExpressionMatch bodyMatch = bodyRe.match(xml);
    if (bodyMatch.hasMatch()) {
        xml = bodyMatch.captured(1);
    }

    xml.replace(QRegularExpression("<html[^>]*>"), "");
    xml.replace("</html>", "");
    xml.replace(QRegularExpression("<head[^>]*>[\\s\\S]*</head>"), "");
    xml.replace(QRegularExpression("<head\\s*/?>"), "");

    xml.replace(QRegularExpression("<body[^>]*>"), "");
    xml.replace("</body>", "");

    xml.replace(QRegularExpression("<style[^>]*>[\\s\\S]*</style>"), "");

    xml.replace(QRegularExpression("<p\\s+style=\"[^\"]*\"\\s*>"), "<p>");
    xml.replace(QRegularExpression("<p\\s+class=\"[^\"]*\"\\s*>"), "<p>");
    xml.replace(QRegularExpression("<p\\s+align=\"[^\"]*\"\\s*>"), "<p>");

    xml.replace(QRegularExpression("<div\\s+style=\"[^\"]*\"\\s*>"), "<p>");
    xml.replace(QRegularExpression("<div\\s+class=\"[^\"]*\"\\s*>"), "<p>");
    xml.replace(QRegularExpression("<div\\s+align=\"[^\"]*\"\\s*>"), "<p>");
    xml.replace("</div>", "</p>");

    xml.replace(QRegularExpression("<span[^>]*>"), "");
    xml.replace("</span>", "");

    xml.replace(QRegularExpression("<b\\s*/?>"), "<b>");
    xml.replace(QRegularExpression("<i\\s*/?>"), "<i>");
    xml.replace(QRegularExpression("<u\\s*/?>"), "<u>");

    xml.replace(QRegularExpression("<br\\s*/?>"), "<br/>");

    xml.replace(QRegularExpression("<img\\s+src=\"([^\"]*)\"[^>]*/>"),
                 "<image src=\"\\1\"/>");
    xml.replace(QRegularExpression("<img\\s+src=\"([^\"]*)\"[^>]*>"),
                 "<image src=\"\\1\"/>");

    xml.replace("&nbsp;", " ");
    xml.replace("&amp;", "&");
    xml.replace("&lt;", "<");
    xml.replace("&gt;", ">");
    xml.replace("&quot;", "\"");

    xml.replace(QRegularExpression("\n{3,}"), "\n\n");

    xml = xml.trimmed();

    QString result;
    QTextStream stream(&result);
    stream.setCodec("UTF-8");
    stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    stream << "<page>\n";

    if (xml.contains("<table") || xml.contains("<ul") || xml.contains("<ol") ||
        xml.contains("<h1") || xml.contains("<h2") || xml.contains("<h3") ||
        xml.contains("<blockquote") || xml.contains("<pre")) {
        stream << "  <body>\n";
        QStringList lines = xml.split("\n");
        for (const QString &line : lines)
            stream << "    " << line.trimmed() << "\n";
        stream << "  </body>\n";
    } else {
        QStringList paragraphs = xml.split("\n");
        stream << "  <body>\n";
        for (const QString &p : paragraphs) {
            QString trimmed = p.trimmed();
            if (!trimmed.isEmpty()) {
                if (!trimmed.startsWith("<"))
                    stream << "    <p>" << trimmed << "</p>\n";
                else
                    stream << "    " << trimmed << "\n";
            }
        }
        stream << "  </body>\n";
    }

    stream << "</page>";
    return result;
}

QString EditorTabs::xmlToWysiwyg(const QString &xml) const
{
    QString html = xml;

    html.replace(QRegularExpression("<\\?xml[^?]*\\?>"), "");
    html.replace("<page>", "<body>");
    html.replace("</page>", "</body>");

    html.replace("<body>",
        "<body style=\"font-family: 'Times New Roman', serif; font-size: 14px; padding: 20px; line-height: 1.6;\">");

    html.replace(QRegularExpression("<table([^>]*)>"),
                 "<table\\1 style=\"border-collapse: collapse; width: 100%; margin: 12px 0;\">");
    html.replace(QRegularExpression("<td([^>]*)>"),
                 "<td\\1 style=\"border: 1px solid #ccc; padding: 8px; vertical-align: top;\">");
    html.replace(QRegularExpression("<th([^>]*)>"),
                 "<th\\1 style=\"border: 1px solid #ccc; padding: 8px; background: #f0f0f0; font-weight: bold;\">");

    html.replace(QRegularExpression("<collapsible\\s+title=\"([^\"]*)\"[^>]*>"),
                 "<details style=\"border: 1px solid #ccc; margin: 8px 0; border-radius: 4px; padding: 4px 8px;\">"
                 "<summary style=\"cursor: pointer; font-weight: bold; color: #2E86C1;\">\\1</summary>"
                 "<div style=\"padding: 8px; margin-top: 4px;\">");
    html.replace("</collapsible>", "</div></details>");

    html.replace(QRegularExpression("<formula\\s+format=\"latex\"[^>]*>([^<]*)</formula>"),
                 "<div style=\"text-align: center; font-style: italic; padding: 12px; margin: 8px 0; "
                 "background: #f8f8f8; border-radius: 4px; font-family: 'Cambria Math', serif;\">"
                 "\\1</div>");

    html.replace(QRegularExpression("<diagram\\s+engine=\"([^\"]*)\"[^>]*>([\\s\\S]*?)</diagram>"),
                 "<div style=\"text-align: center; padding: 16px; margin: 12px 0; "
                 "background: #f0f8ff; border: 2px dashed #6699cc; border-radius: 6px;\">"
                 "<div style=\"color: #6699cc; font-size: 12px;\">[Диаграмма (\\1)]</div></div>");

    html.replace(QRegularExpression("<image\\s+src=\"([^\"]*)\"\\s+width=\"([^\"]*)\"[^/]*/>"),
                 "<div style=\"text-align: center; margin: 12px 0;\">"
                 "<img src=\"\\1\" style=\"max-width: \\2px; max-height: 400px;\"/></div>");
    html.replace(QRegularExpression("<image\\s+src=\"([^\"]*)\"[^/]*/>"),
                 "<div style=\"text-align: center; margin: 12px 0;\">"
                 "<img src=\"\\1\" style=\"max-width: 100%; max-height: 400px;\"/></div>");
    html.replace(QRegularExpression("<image\\s+src=\"([^\"]*)\"[^/]*/>"),
                 "<div style=\"text-align: center; margin: 12px 0;\">"
                 "<img src=\"\\1\" style=\"max-width: 100%; max-height: 400px;\"/></div>");

    html.replace(QRegularExpression("<h1\\s*/>"), "<h1 style=\"font-size: 24px; margin: 20px 0 12px 0;\">");
    html.replace(QRegularExpression("<h2\\s*/>"), "<h2 style=\"font-size: 20px; margin: 16px 0 10px 0;\">");
    html.replace(QRegularExpression("<h3\\s*/>"), "<h3 style=\"font-size: 16px; margin: 12px 0 8px 0;\">");

    html.replace(QRegularExpression("<blockquote([^>]*)>"),
                 "<blockquote\\1 style=\"margin: 12px 0; padding: 12px 20px; border-left: 4px solid #ccc; background: #fafafa;\">");
    html.replace(QRegularExpression("<pre([^>]*)>"),
                 "<pre\\1 style=\"background: #f5f5f5; padding: 12px; border-radius: 4px; overflow-x: auto; font-family: Consolas, monospace; font-size: 13px;\">");

    html.replace("<ul>", "<ul style=\"margin: 8px 0; padding-left: 24px;\">");
    html.replace("<ol>", "<ol style=\"margin: 8px 0; padding-left: 24px;\">");
    html.replace("<li>", "<li style=\"margin: 4px 0;\">");

    html.replace("<br/>", "<br/>");
    html.replace("<br>", "<br/>");

    if (!html.contains("<html>")) {
        html = "<html><head><meta charset=\"utf-8\"/></head>" + html + "</html>";
    }

    return html;
}
