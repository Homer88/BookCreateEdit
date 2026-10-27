#ifndef EDITORTABS_H
#define EDITORTABS_H

#include <QWidget>
#include <QTabWidget>
#include <QTimer>

class BookDocument;
class QTextEdit;
class XmlEditor;

/**
 * @brief Dual-view редактор (WYSIWYG + XML) с вкладками страниц.
 *
 * Два уровня вкладок:
 *   1. Верхний: вкладки страниц (p1, p2, ...)
 *   2. Нижний: «Визуальный» / «XML»
 *
 * Синхронизация WYSIWYG <-> XML через таймер (100мс) и хеши контента.
 * Защита от рекурсии через флаг m_syncing.
 */
class EditorTabs : public QWidget
{
    Q_OBJECT

public:
    explicit EditorTabs(QWidget *parent = nullptr);

    void setDocument(BookDocument *doc);
    BookDocument *document() const;
    void newDocument();
    void setCurrentPage(int index);
    int currentPage() const;
    int pageCount() const;
    QString currentPageXml() const;
    void addPage();
    void removeCurrentPage();

    /** @brief Сохраняет содержимое активного редактора в текущую страницу документа. */
    void flushCurrentPage();

    /**
     * @brief Перестраивает вкладки страниц из документа и открывает выбранную.
     * @param selectIndex Индекс страницы для открытия (-1 — оставить текущий,
     *                    при выходе за границы выбирается ближайший корректный).
     *
     * Вызывается после структурных изменений списка страниц (вставка/удаление),
     * когда индексы сдвинулись. Правки НЕ сохраняет — вызывающая сторона
     * обязана предварительно вызвать flushCurrentPage().
     */
    void reloadFromDocument(int selectIndex = -1);
    QTextEdit *wysiwygEditor() const { return m_wysiwygEditor; }
    void setFont(const QFont &font);
    void applyScale(int percent); ///< Масштабирует шрифты редакторов

signals:
    void pageCountChanged(int count);
    void currentPageChanged(int index);
    void contentModified();

private slots:
    void onTabChanged(int index);      ///< Смена вкладки страницы
    void onWysiwygChanged();           ///< Изменение в WYSIWYG-редакторе
    void onXmlChanged();               ///< Изменение в XML-редакторе
    void onAddPage();                  ///< Клик «Добавить страницу»
    void onRemovePage();               ///< Клик «Удалить страницу»
    void syncWysiwygToXml();          ///< Конвертация HTML -> BCE XML
    void syncXmlToWysiwyg();          ///< Конвертация BCE XML -> HTML

private:
    void setupUI();
    void rebuildPageTabs();            ///< Пересоздаёт вкладки страниц из документа
    void saveCurrentPage();            ///< Сохраняет текущую страницу в BookDocument
    void loadPage(int index);          ///< Загружает страницу из BookDocument в редакторы
    void removePage(int index);        ///< Удаляет страницу по индексу

    void syncFromWysiwyg();            ///< Запуск синхронизации WYSIWYG -> XML (с таймером)
    void syncFromXml();                ///< Запуск синхронизации XML -> WYSIWYG (с таймером)
    void updateWysiwygFromXml();       ///< Немедленная конвертация XML -> WYSIWYG
    void updateXmlFromWysiwyg();       ///< Немедленная конвертация WYSIWYG -> XML

    QString wysiwygToXml(const QString &html) const; ///< HTML -> BCE XML через HtmlConverter
    QString xmlToWysiwyg(const QString &xml) const;  ///< BCE XML -> HTML через HtmlConverter
    int xmlContentHash(const QString &xml) const;     ///< Хеш XML для отслеживания изменений
    int htmlContentHash(const QString &html) const;   ///< Хеш HTML для отслеживания изменений

    QTabWidget *m_pageTabs;     ///< Вкладки страниц
    QTabWidget *m_viewTabs;     ///< Вкладки режимов (WYSIWYG / XML)

    QWidget *m_wysiwygPage;     ///< Контейнер WYSIWYG-страницы
    QTextEdit *m_wysiwygEditor; ///< WYSIWYG-редактор (QTextEdit)

    QWidget *m_xmlPage;         ///< Контейнер XML-страницы
    XmlEditor *m_xmlEditor;     ///< XML-редактор с подсветкой синтаксиса

    BookDocument *m_document;   ///< Текущий документ
    int m_currentPageIndex;     ///< Индекс текущей страницы
    bool m_syncing;             ///< Флаг предотвращения рекурсии синхронизации

    QTimer m_syncWysiwygTimer;  ///< Таймер отложенной синхронизации WYSIWYG -> XML
    QTimer m_syncXmlTimer;      ///< Таймер отложенной синхронизации XML -> WYSIWYG
    int m_lastWysiwygHash;      ///< Хеш последнего обработанного HTML
    int m_lastXmlHash;          ///< Хеш последнего обработанного XML
    double m_scale;             ///< Текущий множитель масштаба (1.0 = 100%)
};

#endif // EDITORTABS_H
