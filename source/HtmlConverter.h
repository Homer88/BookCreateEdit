#ifndef HTMLCONVERTER_H
#define HTMLCONVERTER_H

#include <QString>
#include <QDomDocument>
#include <QDomElement>

/**
 * @brief Статический конвертер между HTML и форматом BCE XML.
 *
 * BCE (BookCreateEdit) — нативный XML-формат приложения.
 * Страницы хранятся как <page><body>...</body></page>.
 *
 * Этот конвертер используется плагинами форматов для преобразования
 * HTML-контента (из EPUB, CHM, MOBI и т.д.) в BCE и обратно.
 *
 * Поддерживаемые HTML-элементы:
 *   - <p>, <h1>-<h6>, <br>, <hr>
 *   - <b>/<strong>, <i>/<em>, <u>, <strike>, <code>, <pre>
 *   - <a href>, <img src alt>
 *   - <ul>/<ol>/<li>, <table>/<tr>/<td>
 *   - <blockquote>, <div>, <span>
 *   - Inline-стили: font-size, font-weight, color, text-decoration
 */
class HtmlConverter
{
public:
    /**
     * @brief Конвертирует HTML-строку в BCE XML страницу.
     * @param html — строка с HTML-разметкой (может содержать несколько <p>).
     * @return BCE XML вида <page><body>...</body></page>.
     */
    static QString htmlToBcePage(const QString &html);

    /**
     * @brief Конвертирует BCE XML страницу в HTML-строку.
     * @param bceXml — BCE XML (<page><body>...</body></page>).
     * @return HTML-строка с тегами <p>, <b>, <i> и т.д.
     */
    static QString bcePageToHtml(const QString &bceXml);

    /**
     * @brief Конвертирует BCE XML страницу в QDomDocument для навигации по дереву.
     * @param bceXml — BCE XML страницы.
     * @return QDomDocument с корневым элементом <page>.
     */
    static QDomDocument bcePageToDom(const QString &bceXml);

    /**
     * @brief Конвертирует QDomDocument обратно в BCE XML строку.
     * @param doc — QDomDocument с корнем <page>.
     * @return BCE XML строка.
     */
    static QString domToBcePage(const QDomDocument &doc);

private:
    /** @brief Рекурсивно конвертирует HTML-узел в BCE XML элемент. */
    static void htmlNodeToBce(const QDomNode &htmlNode, QDomElement &bceParent,
                              QDomDocument &bceDoc);

    /** @brief Рекурсивно конвертирует BCE XML узел в HTML-строку. */
    static void bceNodeToHtml(const QDomNode &bceNode, QString &html, int indent);

    /**
     * @brief Парсит inline CSS-стиль и генерирует открывающие/закрывающие HTML-теги.
     * @param style     — CSS-строка (например, "font-weight:bold; color:#f00").
     * @param openTags  — сюда добавляются открывающие теги (<b>, <font> и т.д.).
     * @param closeTags — сюда добавляются закрывающие теги в обратном порядке.
     */
    static void convertInlineStyle(const QString &style, QString &openTags,
                                   QString &closeTags);
};

#endif // HTMLCONVERTER_H
