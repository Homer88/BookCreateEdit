#include "XmlEditor.h"

#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QKeyEvent>
#include <QTextCursor>

// ===================== XmlHighlighter =====================
XmlHighlighter::XmlHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    m_tagFormat.setForeground(QColor("#569CD6"));
    m_tagFormat.setFontWeight(QFont::Bold);

    m_attrFormat.setForeground(QColor("#9CDCFE"));

    m_valueFormat.setForeground(QColor("#CE9178"));

    m_commentFormat.setForeground(QColor("#6A9955"));
    m_commentFormat.setFontItalic(true);

    m_entityFormat.setForeground(QColor("#D7BA7D"));

    m_rules.append({QRegularExpression("<\\?[^?]*\\?>"), m_commentFormat});
    m_rules.append({QRegularExpression("<!--[\\s\\S]*?-->"), m_commentFormat});
    m_rules.append({QRegularExpression("<!\\[CDATA\\[[\\s\\S]*?\\]\\]>"), m_commentFormat});
    m_rules.append({QRegularExpression("</?\\w[\\w:-]*"), m_tagFormat});
    m_rules.append({QRegularExpression("/?>"), m_tagFormat});
    m_rules.append({QRegularExpression("\\b\\w+(?=\\s*=)"), m_attrFormat});
    m_rules.append({QRegularExpression("\"[^\"]*\""), m_valueFormat});
    m_rules.append({QRegularExpression("'[^']*'"), m_valueFormat});
    m_rules.append({QRegularExpression("&\\w+;"), m_entityFormat});
}

void XmlHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightRule &rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

// ===================== LineNumberArea =====================
LineNumberArea::LineNumberArea(XmlEditor *editor)
    : QWidget(editor), m_editor(editor)
{
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_editor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    m_editor->lineNumberAreaPaintEvent(event);
}

// ===================== XmlEditor =====================
XmlEditor::XmlEditor(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_highlighter(nullptr)
    , m_lineNumberArea(nullptr)
    , m_matchedBracketPos(-1)
{
    setupHighlighter();

    m_lineNumberArea = new LineNumberArea(this);

    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopWidth(4);
    setMinimumWidth(400);

    setStyleSheet(R"(
        QPlainTextEdit {
            background-color: #1e1e1e;
            color: #d4d4d4;
            font-family: "Consolas", "Courier New", monospace;
            font-size: 12px;
            border: none;
            padding: 4px;
        }
    )");

    connect(this, &QPlainTextEdit::blockCountChanged, this, &XmlEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &XmlEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &XmlEditor::highlightCurrentLine);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &XmlEditor::updateBracketHighlight);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

void XmlEditor::setupHighlighter()
{
    m_highlighter = new XmlHighlighter(document());
}

void XmlEditor::setXmlContent(const QString &xml)
{
    bool oldState = blockSignals(true);
    setPlainText(xml);
    blockSignals(oldState);
}

QString XmlEditor::xmlContent() const
{
    return toPlainText();
}

void XmlEditor::setReadOnly(bool readOnly)
{
    QPlainTextEdit::setReadOnly(readOnly);
}

int XmlEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int maxBlock = qMax(1, blockCount());
    while (maxBlock >= 10) {
        maxBlock /= 10;
        ++digits;
    }
    int space = 20 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 10;
    return space;
}

void XmlEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void XmlEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void XmlEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void XmlEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor("#252526"));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(QColor("#858585"));

            if (blockNumber == textCursor().blockNumber())
                painter.setPen(QColor("#c6c6c6"));

            painter.drawText(0, top, m_lineNumberArea->width() - 8,
                             fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void XmlEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(QColor("#2a2d2e"));
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);
    setExtraSelections(extraSelections);
}

void XmlEditor::updateBracketHighlight()
{
    QTextCursor cursor = textCursor();
    int pos = cursor.position();

    QChar ch = charAtCursor();
    QChar chBefore = charBeforeCursor();

    m_matchedBracketPos = -1;

    if ((ch == '>' || ch == '/') && pos > 0) {
        QTextCursor search = cursor;
        search.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor);
        QChar openChar = (chBefore == '/') ? QChar('<') : chBefore;
        if (openChar == '<') {
            int depth = 1;
            search.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor);
            while (!search.atStart() && depth > 0) {
                QString blockText = search.block().text();
                int blockPos = search.positionInBlock();
                for (int i = blockPos; i >= 0; --i) {
                    QChar c = blockText[i];
                    if (c == '<') {
                        if (i + 1 < blockText.size() && blockText[i + 1] == '/')
                            continue;
                        --depth;
                        if (depth == 0) {
                            m_matchedBracketPos = search.position() - blockPos + i;
                            highlightMatchingBracket(m_matchedBracketPos, '<');
                            return;
                        }
                    }
                }
                search.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor);
                search.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
            }
        }
    }

    if (ch == '<') {
        QTextCursor search = cursor;
        search.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor);
        int depth = 1;
        while (!search.atEnd() && depth > 0) {
            QString blockText = search.block().text();
            int blockPos = search.positionInBlock();
            for (int i = blockPos; i < blockText.size(); ++i) {
                QChar c = blockText[i];
                if (c == '<') {
                    if (i + 1 < blockText.size() && blockText[i + 1] == '/') {
                        --depth;
                        if (depth == 0) {
                            m_matchedBracketPos = search.position() - blockPos + i + 1;
                            highlightMatchingBracket(pos, '<');
                            highlightMatchingBracket(m_matchedBracketPos, '>');
                            return;
                        }
                    } else {
                        ++depth;
                    }
                } else if (c == '>' && depth == 1) {
                    break;
                }
            }
            search.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor);
            search.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        }
    }
}

void XmlEditor::highlightMatchingBracket(int pos, QChar)
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(pos);
    QTextCursor mergedCursor = cursor;
    mergedCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);

    QTextCharFormat bracketFormat;
    bracketFormat.setForeground(QColor("#FFD700"));
    bracketFormat.setFontWeight(QFont::Bold);

    QList<QTextEdit::ExtraSelection> extras = extraSelections();
    QTextEdit::ExtraSelection sel;
    sel.format = bracketFormat;
    sel.cursor = mergedCursor;
    extras.append(sel);
    setExtraSelections(extras);
}

QChar XmlEditor::charAtCursor() const
{
    QTextCursor cursor = textCursor();
    if (cursor.position() >= document()->toPlainText().size())
        return QChar();
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor);
    return document()->toPlainText().at(cursor.position() - 1);
}

QChar XmlEditor::charBeforeCursor() const
{
    QTextCursor cursor = textCursor();
    if (cursor.position() == 0)
        return QChar();
    cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor);
    return document()->toPlainText().at(cursor.position() - 1);
}

void XmlEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Tab) {
        QTextCursor cursor = textCursor();
        cursor.insertText("    ");
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}
