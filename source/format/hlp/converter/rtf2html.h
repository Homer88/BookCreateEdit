#ifndef RTF2HTML_H
#define RTF2HTML_H

#include <QString>
#include <QStringList>

namespace Rtf {

// Метаданные темы, извлекаемые из сносок WinHelp ($ = заголовок, # = контекст).
struct Meta
{
    QString title;
    QStringList anchors; // у темы может быть несколько контекстных имён (#)
};

// Преобразует RTF-фрагмент темы в HTML:
//  - переходы {\v ...} -> <a href="#..."> (метка - текст, идущий перед переходом);
//  - изображения {bmc/bml/bmr ...} -> <img src="..."> (файлы лежат рядом с RTF);
//  - шрифты/цвета/размеры/начертания через \f, \cf, \fs, \b, \i, \u, \strike, \scaps;
//  - сноски \footnote (метки {\up +/$/#/K}) в контент не попадают;
//    из них заполняется Meta (title/anchor), если передан указатель.
// rtfAll - весь RTF-документ (нужен для таблиц шрифтов/цветов); если пуст,
// таблицы ищутся в самом фрагменте.
QString toHtml(const QString &rtfTopic,
               const QString &rtfAll = QString(),
               Meta *meta = nullptr);

// Разбивает RTF-документ на темы по управляющему слову \page (вне групп).
QStringList splitTopics(const QString &rtf);

// Запасной вариант заголовка: текст первой строки без RTF-разметки.
QString headingOf(const QString &rtfTopic);

} // namespace Rtf

#endif // RTF2HTML_H
