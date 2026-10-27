#include "HtmlConverter.h"

#include <QTextStream>
#include <QRegularExpression>
#include <QStack>

/**
 * @brief Преобразует HTML-строку в страницу формата BCE XML.
 *
 * Оборачивает входной HTML в корневые теги <html><body>, разбирает его
 * в DOM-дерево и рекурсивно переносит узлы body в новый документ с корнем <page>.
 *
 * @param html Входная строка HTML.
 * @return XML-строку вида "<page>...</page>". При ошибке разбора весь
 *         исходный текст экранируется и помещается в один абзац <p>.
 */
QString HtmlConverter::htmlToBcePage(const QString &html)
{
    QDomDocument htmlDoc;
    // Оборачиваем фрагмент HTML в полноценный документ для успешного парсинга
    QString wrappedHtml = "<html><body>" + html + "</body></html>";

    QString errMsg; int errLine;
    if (!htmlDoc.setContent(wrappedHtml, &errMsg, &errLine)) {
        // Fallback: некорректный HTML возвращаем как экранированный текст в одном абзаце
        return "<page><p>" + html.toHtmlEscaped() + "</p></page>";
    }

    // Создаём целевой документ BCE с корневым элементом <page>
    QDomDocument bceDoc;
    bceDoc.setContent(QString("<page/>"));
    QDomElement bceRoot = bceDoc.documentElement();

    // Берём содержимое <body>; при его отсутствии — сам корневой элемент
    QDomElement body = htmlDoc.documentElement().firstChildElement("body");
    if (body.isNull())
        body = htmlDoc.documentElement();

    // Рекурсивно конвертируем каждый дочерний узел body
    QDomNodeList children = body.childNodes();
    for (int i = 0; i < children.count(); ++i) {
        htmlNodeToBce(children.at(i), bceRoot, bceDoc);
    }

    return bceDoc.toString(-1);
}

/**
 * @brief Рекурсивно преобразует один HTML-узел в соответствующий элемент BCE.
 *
 * Поддерживает заголовки, абзацы со встроенным форматированием (b/i/u/a/img/code/br),
 * списки (в том числе вложенные), таблицы (с thead/tbody/tfoot, th, colspan),
 * pre, blockquote, hr, изображения, переносы строк, контейнеры (div/section/article/main),
 * ссылки, сворачиваемые блоки details/summary -> collapsible.
 * Нераспознанные теги обрабатываются рекурсивно без создания собственного элемента.
 *
 * @param htmlNode  Исходный HTML-узел (должен быть элементом).
 * @param bceParent Родительский элемент BCE, куда добавляется результат.
 * @param bceDoc    Целевой DOM-документ BCE (используется для создания узлов).
 */
void HtmlConverter::htmlNodeToBce(const QDomNode &htmlNode, QDomElement &bceParent, QDomDocument &bceDoc)
{
    // Обрабатываем только элементы; текстовые и прочие узлы пропускаем
    if (!htmlNode.isElement())
        return;

    QDomElement htmlEl = htmlNode.toElement();
    QString tag = htmlEl.tagName().toLower();

    if (tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6") {
        // Заголовок: имя тега сохраняется, всё содержимое сводится к простому тексту
        QDomElement el = bceDoc.createElement(tag);
        el.appendChild(bceDoc.createTextNode(htmlEl.text()));
        bceParent.appendChild(el);
    }
    else if (tag == "p") {
        QDomElement el = bceDoc.createElement("p");
        QDomNodeList children = htmlEl.childNodes();
        // Разбираем встроенное содержимое абзаца по типам дочерних тегов
        for (int i = 0; i < children.count(); ++i) {
            QDomNode child = children.at(i);
            if (child.isText()) {
                el.appendChild(bceDoc.createTextNode(child.toText().data()));
            } else if (child.isElement()) {
                QDomElement childEl = child.toElement();
                QString ctag = childEl.tagName().toLower();
                if (ctag == "b" || ctag == "strong") {
                    // Полужирный: strong нормализуется к <b>
                    QDomElement b = bceDoc.createElement("b");
                    b.appendChild(bceDoc.createTextNode(childEl.text()));
                    el.appendChild(b);
                } else if (ctag == "i" || ctag == "em") {
                    // Курсив: em нормализуется к <i>
                    QDomElement i = bceDoc.createElement("i");
                    i.appendChild(bceDoc.createTextNode(childEl.text()));
                    el.appendChild(i);
                } else if (ctag == "u") {
                    QDomElement u = bceDoc.createElement("u");
                    u.appendChild(bceDoc.createTextNode(childEl.text()));
                    el.appendChild(u);
                } else if (ctag == "a") {
                    // Ссылка: атрибут href переносится как есть
                    QDomElement a = bceDoc.createElement("a");
                    a.setAttribute("href", childEl.attribute("href"));
                    a.appendChild(bceDoc.createTextNode(childEl.text()));
                    el.appendChild(a);
                } else if (ctag == "img") {
                    // Изображение внутри абзаца: <img> переименовывается в <image>
                    QDomElement img = bceDoc.createElement("image");
                    img.setAttribute("src", childEl.attribute("src"));
                    if (!childEl.attribute("alt").isEmpty())
                        img.setAttribute("alt", childEl.attribute("alt"));
                    el.appendChild(img);
                } else if (ctag == "code" || ctag == "tt") {
                    // Моноширинный код: tt нормализуется к <code>
                    QDomElement code = bceDoc.createElement("code");
                    code.appendChild(bceDoc.createTextNode(childEl.text()));
                    el.appendChild(code);
                } else if (ctag == "br") {
                    el.appendChild(bceDoc.createElement("br"));
                } else if (ctag == "span") {
                    // span не имеет аналога в BCE — переносим только его текстовое содержимое
                    QDomNodeList spanChildren = childEl.childNodes();
                    for (int j = 0; j < spanChildren.count(); ++j) {
                        if (spanChildren.at(j).isText())
                            el.appendChild(bceDoc.createTextNode(spanChildren.at(j).toText().data()));
                    }
                } else {
                    // Неизвестный тег внутри абзаца заменяется своим текстом
                    el.appendChild(bceDoc.createTextNode(childEl.text()));
                }
            }
        }
        bceParent.appendChild(el);
    }
    else if (tag == "ul" || tag == "ol") {
        // Список: тип (ul/ol) сохраняется в атрибуте type
        QDomElement listEl = bceDoc.createElement("list");
        listEl.setAttribute("type", tag);

        QDomNodeList items = htmlEl.childNodes();
        for (int i = 0; i < items.count(); ++i) {
            QDomElement itemEl = items.at(i).toElement();
            if (itemEl.tagName().toLower() == "li") {
                QDomElement li = bceDoc.createElement("li");

                // Разбираем содержимое пункта списка
                QDomNodeList liChildren = itemEl.childNodes();
                for (int j = 0; j < liChildren.count(); ++j) {
                    QDomNode child = liChildren.at(j);
                    if (child.isText()) {
                        li.appendChild(bceDoc.createTextNode(child.toText().data()));
                    } else if (child.isElement()) {
                        QDomElement ce = child.toElement();
                        QString ct = ce.tagName().toLower();
                        if (ct == "ul" || ct == "ol") {
                            // Вложенный список обрабатывается тем же рекурсивным методом
                            htmlNodeToBce(child, li, bceDoc);
                        } else if (ct == "a") {
                            QDomElement a = bceDoc.createElement("a");
                            a.setAttribute("href", ce.attribute("href"));
                            a.appendChild(bceDoc.createTextNode(ce.text()));
                            li.appendChild(a);
                        } else if (ct == "b" || ct == "strong") {
                            QDomElement b = bceDoc.createElement("b");
                            b.appendChild(bceDoc.createTextNode(ce.text()));
                            li.appendChild(b);
                        } else if (ct == "i" || ct == "em") {
                            QDomElement ii = bceDoc.createElement("i");
                            ii.appendChild(bceDoc.createTextNode(ce.text()));
                            li.appendChild(ii);
                        } else {
                            li.appendChild(bceDoc.createTextNode(ce.text()));
                        }
                    }
                }
                listEl.appendChild(li);
            }
        }
        bceParent.appendChild(listEl);
    }
    else if (tag == "table") {
        QDomElement tableEl = bceDoc.createElement("table");

        QDomNodeList rows = htmlEl.childNodes();
        for (int i = 0; i < rows.count(); ++i) {
            QDomElement rowEl = rows.at(i).toElement();
            QString rtag = rowEl.tagName().toLower();
            if (rtag == "tr" || rtag == "thead" || rtag == "tbody" || rtag == "tfoot") {
                if (rtag != "tr") {
                    // Строки, обёрнутые в thead/tbody/tfoot: извлекаем вложенные tr
                    QDomNodeList subRows = rowEl.childNodes();
                    for (int sr = 0; sr < subRows.count(); ++sr) {
                        QDomElement subRow = subRows.at(sr).toElement();
                        if (subRow.tagName().toLower() == "tr") {
                            QDomElement tr = bceDoc.createElement("row");
                            QDomNodeList cells = subRow.childNodes();
                            for (int c = 0; c < cells.count(); ++c) {
                                QDomElement cellEl = cells.at(c).toElement();
                                QString cellTag = cellEl.tagName().toLower();
                                if (cellTag == "td" || cellTag == "th") {
                                    QDomElement td = bceDoc.createElement("cell");
                                    // Заголовочная ячейка th отмечается атрибутом bold="true"
                                    if (cellTag == "th")
                                        td.setAttribute("bold", "true");
                                    // Объединение ячеек по горизонтали переносится в атрибут colspan
                                    int colspan = cellEl.attribute("colspan", "1").toInt();
                                    if (colspan > 1) td.setAttribute("colspan", QString::number(colspan));
                                    QDomNodeList cellChildren = cellEl.childNodes();
                                    for (int k = 0; k < cellChildren.count(); ++k) {
                                        QDomNode cc = cellChildren.at(k);
                                        if (cc.isText()) {
                                            // Текст в ячейке заворачивается в <p>
                                            QDomElement cp = bceDoc.createElement("p");
                                            cp.appendChild(bceDoc.createTextNode(cc.toText().data()));
                                            td.appendChild(cp);
                                        } else if (cc.isElement()) {
                                            QDomElement cce = cc.toElement();
                                            QString cct = cce.tagName().toLower();
                                            if (cct == "p" || cct == "div") {
                                                QDomElement cp = bceDoc.createElement("p");
                                                cp.appendChild(bceDoc.createTextNode(cce.text()));
                                                td.appendChild(cp);
                                            } else if (cct == "img") {
                                                QDomElement img = bceDoc.createElement("image");
                                                img.setAttribute("src", cce.attribute("src"));
                                                td.appendChild(img);
                                            } else if (cct == "table") {
                                                // Вложенная таблица конвертируется рекурсивно внутрь ячейки
                                                htmlNodeToBce(cc, td, bceDoc);
                                            } else {
                                                QDomElement cp = bceDoc.createElement("p");
                                                cp.appendChild(bceDoc.createTextNode(cce.text()));
                                                td.appendChild(cp);
                                            }
                                        }
                                    }
                                    tr.appendChild(td);
                                }
                            }
                            tableEl.appendChild(tr);
                        }
                    }
                } else {
                    // Прямая строка <tr> без секционных обёрток
                    QDomElement tr = bceDoc.createElement("row");
                    QDomNodeList cells = rowEl.childNodes();
                    for (int c = 0; c < cells.count(); ++c) {
                        QDomElement cellEl = cells.at(c).toElement();
                        QString cellTag = cellEl.tagName().toLower();
                        if (cellTag == "td" || cellTag == "th") {
                            QDomElement td = bceDoc.createElement("cell");
                            // Заголовочная ячейка th отмечается атрибутом bold="true"
                            if (cellTag == "th")
                                td.setAttribute("bold", "true");
                            int colspan = cellEl.attribute("colspan", "1").toInt();
                            if (colspan > 1) td.setAttribute("colspan", QString::number(colspan));
                            QDomNodeList cellChildren = cellEl.childNodes();
                            for (int k = 0; k < cellChildren.count(); ++k) {
                                QDomNode cc = cellChildren.at(k);
                                if (cc.isText()) {
                                    // Текст в ячейке заворачивается в <p>
                                    QDomElement cp = bceDoc.createElement("p");
                                    cp.appendChild(bceDoc.createTextNode(cc.toText().data()));
                                    td.appendChild(cp);
                                } else if (cc.isElement()) {
                                    QDomElement cce = cc.toElement();
                                    QString cct = cce.tagName().toLower();
                                    if (cct == "p" || cct == "div") {
                                        QDomElement cp = bceDoc.createElement("p");
                                        cp.appendChild(bceDoc.createTextNode(cce.text()));
                                        td.appendChild(cp);
                                    } else if (cct == "img") {
                                        QDomElement img = bceDoc.createElement("image");
                                        img.setAttribute("src", cce.attribute("src"));
                                        td.appendChild(img);
                                    } else if (cct == "table") {
                                        // Вложенная таблица конвертируется рекурсивно внутрь ячейки
                                        htmlNodeToBce(cc, td, bceDoc);
                                    } else {
                                        QDomElement cp = bceDoc.createElement("p");
                                        cp.appendChild(bceDoc.createTextNode(cce.text()));
                                        td.appendChild(cp);
                                    }
                                }
                            }
                            tr.appendChild(td);
                        }
                    }
                    tableEl.appendChild(tr);
                }
            }
        }
        bceParent.appendChild(tableEl);
    }
    else if (tag == "pre") {
        // Предварительно отформатированный блок переносится как простой текст
        QDomElement el = bceDoc.createElement("pre");
        el.appendChild(bceDoc.createTextNode(htmlEl.text()));
        bceParent.appendChild(el);
    }
    else if (tag == "blockquote") {
        // Цитата: внутренние блоки конвертируются рекурсивно
        QDomElement el = bceDoc.createElement("blockquote");
        QDomNodeList children = htmlEl.childNodes();
        for (int i = 0; i < children.count(); ++i) {
            htmlNodeToBce(children.at(i), el, bceDoc);
        }
        bceParent.appendChild(el);
    }
    else if (tag == "hr") {
        QDomElement el = bceDoc.createElement("hr");
        bceParent.appendChild(el);
    }
    else if (tag == "img") {
        // Самостоятельное изображение: <img> -> <image>, alt добавляется только если задан
        QDomElement el = bceDoc.createElement("image");
        el.setAttribute("src", htmlEl.attribute("src"));
        if (!htmlEl.attribute("alt").isEmpty())
            el.setAttribute("alt", htmlEl.attribute("alt"));
        bceParent.appendChild(el);
    }
    else if (tag == "br") {
        bceParent.appendChild(bceDoc.createElement("br"));
    }
    else if (tag == "div" || tag == "section" || tag == "article" || tag == "main") {
        // Контейнерные теги прозрачны: дети добавляются прямо в текущего родителя
        QDomNodeList children = htmlEl.childNodes();
        for (int i = 0; i < children.count(); ++i) {
            htmlNodeToBce(children.at(i), bceParent, bceDoc);
        }
    }
    else if (tag == "a") {
        QDomElement el = bceDoc.createElement("a");
        el.setAttribute("href", htmlEl.attribute("href"));
        el.appendChild(bceDoc.createTextNode(htmlEl.text()));
        bceParent.appendChild(el);
    }
    else if (tag == "b" || tag == "strong") {
        QDomElement el = bceDoc.createElement("b");
        el.appendChild(bceDoc.createTextNode(htmlEl.text()));
        bceParent.appendChild(el);
    }
    else if (tag == "i" || tag == "em") {
        QDomElement el = bceDoc.createElement("i");
        el.appendChild(bceDoc.createTextNode(htmlEl.text()));
        bceParent.appendChild(el);
    }
    else if (tag == "u") {
        QDomElement el = bceDoc.createElement("u");
        el.appendChild(bceDoc.createTextNode(htmlEl.text()));
        bceParent.appendChild(el);
    }
    else if (tag == "code" || tag == "tt" || tag == "kbd" || tag == "samp") {
        // Все варианты моноширинного текста сводятся к <code>
        QDomElement el = bceDoc.createElement("code");
        el.appendChild(bceDoc.createTextNode(htmlEl.text()));
        bceParent.appendChild(el);
    }
    else if (tag == "details") {
        // Сворачиваемый блок: <details>/<summary> -> <collapsible title="...">
        QDomElement el = bceDoc.createElement("collapsible");
        QString summary;
        QDomElement summaryEl = htmlEl.firstChildElement("summary");
        if (!summaryEl.isNull()) {
            summary = summaryEl.text();
            el.setAttribute("title", summary);
        }
        QDomNodeList children = htmlEl.childNodes();
        // Заголовок summary уже учтён в атрибуте title — повторно не конвертируем
        for (int i = 0; i < children.count(); ++i) {
            QDomNode child = children.at(i);
            if (child.isElement()) {
                QDomElement ce = child.toElement();
                if (ce.tagName().toLower() != "summary")
                    htmlNodeToBce(child, el, bceDoc);
            }
        }
        bceParent.appendChild(el);
    }
    else {
        // Нераспознанный тег: прозрачно пробрасываем детей дальше вверх по дереву
        QDomNodeList children = htmlEl.childNodes();
        for (int i = 0; i < children.count(); ++i) {
            htmlNodeToBce(children.at(i), bceParent, bceDoc);
        }
    }
}

// ==================== BCE → HTML ====================

/**
 * @brief Преобразует XML-страницу формата BCE обратно в HTML.
 *
 * Разбирает входной XML в DOM и последовательно сериализует все дочерние
 * узлы корневого элемента в HTML-текст с отступами.
 *
 * @param bceXml Входная строка BCE XML.
 * @return HTML-разметку. При ошибке разбора исходный текст экранируется
 *         и возвращается внутри одного абзаца <p>.
 */
QString HtmlConverter::bcePageToHtml(const QString &bceXml)
{
    QDomDocument bceDoc;
    QString errMsg; int errLine;
    if (!bceDoc.setContent(bceXml, &errMsg, &errLine)) {
        // Fallback: некорректный XML возвращаем как экранированный текст в одном абзаце
        return "<p>" + bceXml.toHtmlEscaped() + "</p>";
    }

    QString html;
    QDomNodeList children = bceDoc.documentElement().childNodes();
    // Рекурсивно генерируем HTML для каждого блока верхнего уровня
    for (int i = 0; i < children.count(); ++i) {
        bceNodeToHtml(children.at(i), html, 0);
    }
    return html;
}

/**
 * @brief Рекурсивно сериализует один узел BCE в HTML-текст.
 *
 * Обрабатывает заголовки, абзацы со встроенными элементами, списки (включая
 * один уровень вложенности), таблицы, изображения, ссылки, форматирование,
 * код, цитаты, формулы, диаграммы и сворачиваемые блоки. Текст экранируется
 * через toHtmlEscaped().
 *
 * @param bceNode Исходный BCE-узел (должен быть элементом).
 * @param html    Выходной буфер, куда дописывается HTML.
 * @param indent  Текущий уровень отступа (каждый уровень — 2 пробела).
 */
void HtmlConverter::bceNodeToHtml(const QDomNode &bceNode, QString &html, int indent)
{
    // Обрабатываем только элементы; текстовые и прочие узлы пропускаем
    if (!bceNode.isElement())
        return;

    QDomElement el = bceNode.toElement();
    QString tag = el.tagName();
    // Строка отступа: два пробела на уровень вложенности
    QString ind = QString(indent * 2, ' ');

    if (tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6") {
        html += ind + "<" + tag + ">" + el.text().toHtmlEscaped() + "</" + tag + ">\n";
    }
    else if (tag == "p") {
        html += ind + "<p>";
        // Собираем смешанное содержимое абзаца: текст и встроенные элементы
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i) {
            QDomNode child = children.at(i);
            if (child.isText()) {
                html += child.toText().data().toHtmlEscaped();
            } else if (child.isElement()) {
                QDomElement ce = child.toElement();
                QString ct = ce.tagName();
                if (ct == "b") {
                    // <b> из BCE экспортируется как семантический <strong>
                    html += "<strong>" + ce.text().toHtmlEscaped() + "</strong>";
                } else if (ct == "i") {
                    // <i> из BCE экспортируется как семантический <em>
                    html += "<em>" + ce.text().toHtmlEscaped() + "</em>";
                } else if (ct == "u") {
                    html += "<u>" + ce.text().toHtmlEscaped() + "</u>";
                } else if (ct == "a") {
                    html += "<a href=\"" + ce.attribute("href").toHtmlEscaped() + "\">" + ce.text().toHtmlEscaped() + "</a>";
                } else if (ct == "code") {
                    html += "<code>" + ce.text().toHtmlEscaped() + "</code>";
                } else if (ct == "image") {
                    // Встроенное изображение внутри абзаца
                    html += "<img src=\"" + ce.attribute("src").toHtmlEscaped() + "\"";
                    if (!ce.attribute("alt").isEmpty())
                        html += " alt=\"" + ce.attribute("alt").toHtmlEscaped() + "\"";
                    html += "/>";
                } else if (ct == "br") {
                    html += "<br/>";
                } else if (ct == "formula") {
                    // Формула внутри строки: формат хранится в data-format
                    html += "<span class=\"formula\" data-format=\"" + ce.attribute("format").toHtmlEscaped() + "\">" + ce.text().toHtmlEscaped() + "</span>";
                } else {
                    html += ce.text().toHtmlEscaped();
                }
            }
        }
        html += "</p>\n";
    }
    else if (tag == "list") {
        // Тип списка берётся из атрибута; ul используется по умолчанию
        QString listTag = el.attribute("type", "ul");
        html += ind + "<" + listTag + ">\n";
        QDomNodeList items = el.childNodes();
        for (int i = 0; i < items.count(); ++i) {
            QDomElement li = items.at(i).toElement();
            if (li.tagName() == "li") {
                html += ind + "  <li>";
                QDomNodeList liChildren = li.childNodes();
                for (int j = 0; j < liChildren.count(); ++j) {
                    QDomNode child = liChildren.at(j);
                    if (child.isText()) {
                        html += child.toText().data().toHtmlEscaped();
                    } else if (child.isElement()) {
                        QDomElement ce = child.toElement();
                        QString ct = ce.tagName();
                        if (ct == "a") {
                            html += "<a href=\"" + ce.attribute("href").toHtmlEscaped() + "\">" + ce.text().toHtmlEscaped() + "</a>";
                        } else if (ct == "b") {
                            html += "<strong>" + ce.text().toHtmlEscaped() + "</strong>";
                        } else if (ct == "i") {
                            html += "<em>" + ce.text().toHtmlEscaped() + "</em>";
                        } else if (ct == "list") {
                            // Вложенный список раскрывается вручную (один уровень)
                            QString subTag = ce.attribute("type", "ul");
                            html += "\n" + ind + "    <" + subTag + ">\n";
                            QDomNodeList subItems = ce.childNodes();
                            for (int k = 0; k < subItems.count(); ++k) {
                                QDomElement subLi = subItems.at(k).toElement();
                                if (subLi.tagName() == "li") {
                                    html += ind + "      <li>" + subLi.text().toHtmlEscaped() + "</li>\n";
                                }
                            }
                            html += ind + "    </" + subTag + ">\n" + ind + "  ";
                        } else {
                            html += ce.text().toHtmlEscaped();
                        }
                    }
                }
                html += "</li>\n";
            }
        }
        html += ind + "</" + listTag + ">\n";
    }
    else if (tag == "table") {
        html += ind + "<table border=\"1\">\n";
        QDomNodeList rows = el.childNodes();
        for (int i = 0; i < rows.count(); ++i) {
            QDomElement rowEl = rows.at(i).toElement();
            if (rowEl.tagName() == "row") {
                html += ind + "  <tr>\n";
                QDomNodeList cells = rowEl.childNodes();
                for (int j = 0; j < cells.count(); ++j) {
                    QDomElement cellEl = cells.at(j).toElement();
                    if (cellEl.tagName() == "cell") {
                        // Ячейка bold="true" превращается в заголовочную <th>
                        QString cellTag = cellEl.attribute("bold", "false") == "true" ? "th" : "td";
                        html += ind + "    <" + cellTag;
                        // Атрибут colspan восстанавливается только если он больше 1
                        int colspan = cellEl.attribute("colspan", "1").toInt();
                        if (colspan > 1) html += " colspan=\"" + QString::number(colspan) + "\"";
                        html += ">\n";
                        QDomNodeList cellChildren = cellEl.childNodes();
                        for (int k = 0; k < cellChildren.count(); ++k) {
                            QDomNode cc = cellChildren.at(k);
                            if (cc.isText()) {
                                html += ind + "      " + cc.toText().data().toHtmlEscaped() + "\n";
                            } else if (cc.isElement()) {
                                QDomElement cce = cc.toElement();
                                QString cct = cce.tagName();
                                if (cct == "p") {
                                    html += ind + "      <p>" + cce.text().toHtmlEscaped() + "</p>\n";
                                } else if (cct == "image") {
                                    html += ind + "      <img src=\"" + cce.attribute("src").toHtmlEscaped() + "\"/>\n";
                                } else if (cct == "table") {
                                    // Вложенная таблица сериализуется рекурсивно с увеличенным отступом
                                    bceNodeToHtml(cc, html, indent + 3);
                                } else {
                                    html += ind + "      " + cce.text().toHtmlEscaped() + "\n";
                                }
                            }
                        }
                        html += ind + "    </" + cellTag + ">\n";
                    }
                }
                html += ind + "  </tr>\n";
            }
        }
        html += ind + "</table>\n";
    }
    else if (tag == "image") {
        // Блочное изображение оборачивается в отдельный абзац
        html += ind + "<p><img src=\"" + el.attribute("src").toHtmlEscaped() + "\"";
        if (!el.attribute("alt").isEmpty())
            html += " alt=\"" + el.attribute("alt").toHtmlEscaped() + "\"";
        html += "/></p>\n";
    }
    else if (tag == "a") {
        html += "<a href=\"" + el.attribute("href").toHtmlEscaped() + "\">" + el.text().toHtmlEscaped() + "</a>";
    }
    else if (tag == "b") {
        html += "<strong>" + el.text().toHtmlEscaped() + "</strong>";
    }
    else if (tag == "i") {
        html += "<em>" + el.text().toHtmlEscaped() + "</em>";
    }
    else if (tag == "u") {
        html += "<u>" + el.text().toHtmlEscaped() + "</u>";
    }
    else if (tag == "code") {
        html += "<code>" + el.text().toHtmlEscaped() + "</code>";
    }
    else if (tag == "pre") {
        html += ind + "<pre>" + el.text().toHtmlEscaped() + "</pre>\n";
    }
    else if (tag == "br") {
        html += "<br/>";
    }
    else if (tag == "hr") {
        html += ind + "<hr/>\n";
    }
    else if (tag == "blockquote") {
        // Цитата: содержимое сериализуется рекурсивно с увеличением отступа
        html += ind + "<blockquote>\n";
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i) {
            bceNodeToHtml(children.at(i), html, indent + 1);
        }
        html += ind + "</blockquote>\n";
    }
    else if (tag == "formula") {
        // Блочная формула: $$...$$ внутри div, формат по умолчанию — latex
        QString fmt = el.attribute("format", "latex");
        html += ind + "<div class=\"formula\" data-format=\"" + fmt.toHtmlEscaped() + "\">$$" + el.text().toHtmlEscaped() + "$$</div>\n";
    }
    else if (tag == "diagram") {
        // Диаграмма: исходный код в <pre>, движок по умолчанию — plantuml
        QString engine = el.attribute("engine", "plantuml");
        html += ind + "<div class=\"diagram\" data-engine=\"" + engine.toHtmlEscaped() + "\"><pre>" + el.text().toHtmlEscaped() + "</pre></div>\n";
    }
    else if (tag == "collapsible") {
        // Сворачиваемый блок: title становится заголовком <summary>
        QString title = el.attribute("title", "");
        html += ind + "<details>\n";
        html += ind + "  <summary>" + title.toHtmlEscaped() + "</summary>\n";
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i) {
            bceNodeToHtml(children.at(i), html, indent + 1);
        }
        html += ind + "</details>\n";
    }
    else {
        // Нераспознанный тег: сериализуем только его детей
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i) {
            bceNodeToHtml(children.at(i), html, indent);
        }
    }
}

/**
 * @brief Разбирает BCE XML и возвращает его в виде DOM-документа.
 *
 * Удобная обёртка над QDomDocument::setContent для случаев, когда нужен
 * программный доступ к структуре документа, а не строка.
 *
 * @param bceXml Входная строка BCE XML.
 * @return DOM-документ; при ошибке разбора будет пустым.
 */
QDomDocument HtmlConverter::bcePageToDom(const QString &bceXml)
{
    QDomDocument doc;
    doc.setContent(bceXml);
    return doc;
}

/**
 * @brief Сериализует DOM-документ обратно в строку BCE XML.
 *
 * @param doc Исходный DOM-документ.
 * @return XML-строку без автоотступов (-1).
 */
QString HtmlConverter::domToBcePage(const QDomDocument &doc)
{
    return doc.toString(-1);
}

/**
 * @brief Разбирает значение CSS-атрибута style и формирует парные HTML-теги.
 *
 * Распознаёт полужирное начертание (font-weight:bold), курсив
 * (font-style:italic) и подчёркивание (text-decoration:underline).
 * Порядок закрывающих тегов инвертирован относительно открывающих.
 *
 * @param style     Строка стиля вида "font-weight:bold;font-style:italic".
 * @param openTags  Выход: конкатенация открывающих тегов (<strong><em>...).
 * @param closeTags Выход: конкатенация закрывающих тегов (</em></strong>...)
 *                  в обратном порядке относительно открытых.
 */
void HtmlConverter::convertInlineStyle(const QString &style, QString &openTags, QString &closeTags)
{
    openTags.clear();
    closeTags.clear();

    // Допускаем оба варианта написания: с пробелом после двоеточия и без него
    if (style.contains("font-weight:bold") || style.contains("font-weight: bold")) {
        openTags += "<strong>";
        // Закрывающий тег дописывается слева, чтобы порядок был обратным открывающим
        closeTags = "</strong>" + closeTags;
    }
    if (style.contains("font-style:italic") || style.contains("font-style: italic")) {
        openTags += "<em>";
        closeTags = "</em>" + closeTags;
    }
    if (style.contains("text-decoration:underline") || style.contains("text-decoration: underline")) {
        openTags += "<u>";
        closeTags = "</u>" + closeTags;
    }
}
