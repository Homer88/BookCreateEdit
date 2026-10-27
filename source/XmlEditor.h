#ifndef XMLEDITOR_H
#define XMLEDITOR_H

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QWidget>

class XmlHighlighter;

/**
 * @brief Подсветка синтаксиса XML для XmlEditor.
 *
 * Подсвечивает:
 *   - XML-теги (синий)
 *   - Атрибуты (зелёный)
 *   - Значения атрибутов (коричневый)
 *   - Комментарии <!-- ... --> (серый)
 *   - XML-сущности &amp; &lt; и т.д. (красный)
 */
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
        QRegularExpression pattern; ///< Регулярное выражение для поиска
        QTextCharFormat format;     ///< Формат выделения
    };

    QVector<HighlightRule> m_rules;  ///< Правила подсветки
    QTextCharFormat m_tagFormat;     ///< Формат тегов
    QTextCharFormat m_attrFormat;    ///< Формат атрибутов
    QTextCharFormat m_valueFormat;   ///< Формат значений
    QTextCharFormat m_commentFormat; ///< Формат комментариев
    QTextCharFormat m_entityFormat;  ///< Формат сущностей
};

class XmlEditor;

/**
 * @brief Виджет области номеров строк (отрисовывается слева от текста).
 */
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(XmlEditor *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    XmlEditor *m_editor; ///< Редактор, которому принадлежит область номеров
};

/**
 * @brief XML-редактор с подсветкой синтаксиса, номерами строк и подсветкой скобок.
 *
 * Основан на QPlainTextEdit. Особенности:
 *   - Автоматические номера строк (LineNumberArea)
 *   - Подсветка текущей строки
 *   - Подсветка парных скобок < > и [ ]
 *   - XML-подсветка через XmlHighlighter
 *   - Tab вставляет 2 пробела
 *   - Авто-缩进 новой строки
 */
class XmlEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit XmlEditor(QWidget *parent = nullptr);

    /** @brief Устанавливает XML-контент и запускает подсветку. */
    void setXmlContent(const QString &xml);

    /** @brief Возвращает текущий XML-контент. */
    QString xmlContent() const;

    /** @brief Устанавливает режим «только чтение». */
    void setReadOnly(bool readOnly);
    void applyScale(int percent);

    /** @brief Отрисовка номеров строк (вызывается из LineNumberArea). */
    void lineNumberAreaPaintEvent(QPaintEvent *event);

    /** @brief Вычисляет ширину области номеров строк. */
    int lineNumberAreaWidth();

signals:
    /** @brief Сигнал об изменении контента. */
    void contentChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;  ///< Обновляет позицию LineNumberArea
    void keyPressEvent(QKeyEvent *event) override;    ///< Tab = 2 пробела, авто-缩进

private slots:
    void updateLineNumberAreaWidth(int newBlockCount); ///< Ширина номеров строк зависит от числа строк
    void highlightCurrentLine();                       ///< Подсветка фона текущей строки
    void updateBracketHighlight();                     ///< Подсветка парных скобок

private:
    void setupHighlighter();                        ///< Создаёт XmlHighlighter
    void updateLineNumberArea(const QRect &rect, int dy); ///< Обновляет область номеров при скролле
    QChar charAtCursor() const;                     ///< Символ под курсором
    QChar charBeforeCursor() const;                 ///< Символ перед курсором
    void highlightMatchingBracket(int pos, QChar ch); ///< Подсвечивает парную скобку

    XmlHighlighter *m_highlighter;   ///< Подсветка синтаксиса XML
    LineNumberArea *m_lineNumberArea; ///< Область номеров строк
    int m_matchedBracketPos;         ///< Позиция найденной парной скобки (-1 если нет)
    double m_scale;                  ///< Текущий множитель масштаба
};

#endif // XMLEDITOR_H
