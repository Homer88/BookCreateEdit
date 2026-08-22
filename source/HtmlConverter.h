#ifndef HTMLCONVERTER_H
#define HTMLCONVERTER_H

#include <QString>
#include <QDomDocument>
#include <QDomElement>

class HtmlConverter
{
public:
    static QString htmlToBcePage(const QString &html);
    static QString bcePageToHtml(const QString &bceXml);

    static QDomDocument bcePageToDom(const QString &bceXml);
    static QString domToBcePage(const QDomDocument &doc);

private:
    static void htmlNodeToBce(const QDomNode &htmlNode, QDomElement &bceParent, QDomDocument &bceDoc);
    static void bceNodeToHtml(const QDomNode &bceNode, QString &html, int indent);
    static void convertInlineStyle(const QString &style, QString &openTags, QString &closeTags);
};

#endif // HTMLCONVERTER_H
