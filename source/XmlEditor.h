#ifndef XMLEDITOR_H
#define XMLEDITOR_H

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QWidget>

class XmlHighlighter;

class XmlHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit XmlHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightRule> m_rules;
    QTextCharFormat m_tagFormat;
    QTextCharFormat m_attrFormat;
    QTextCharFormat m_valueFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_entityFormat;
};

class XmlEditor;

class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(XmlEditor *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    XmlEditor *m_editor;
};

class XmlEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit XmlEditor(QWidget *parent = nullptr);

    void setXmlContent(const QString &xml);
    QString xmlContent() const;
    void setReadOnly(bool readOnly);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

signals:
    void contentChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateBracketHighlight();

private:
    void setupHighlighter();
    void updateLineNumberArea(const QRect &rect, int dy);
    QChar charAtCursor() const;
    QChar charBeforeCursor() const;
    void highlightMatchingBracket(int pos, QChar ch);

    XmlHighlighter *m_highlighter;
    LineNumberArea *m_lineNumberArea;
    int m_matchedBracketPos;
};

#endif // XMLEDITOR_H
