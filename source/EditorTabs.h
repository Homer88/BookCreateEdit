#ifndef EDITORTABS_H
#define EDITORTABS_H

#include <QWidget>
#include <QTabWidget>
#include <QTimer>

class BookDocument;
class QTextEdit;
class XmlEditor;

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
    QTextEdit *wysiwygEditor() const { return m_wysiwygEditor; }
    void setFont(const QFont &font);

signals:
    void pageCountChanged(int count);
    void currentPageChanged(int index);
    void contentModified();

private slots:
    void onTabChanged(int index);
    void onWysiwygChanged();
    void onXmlChanged();
    void onAddPage();
    void onRemovePage();
    void syncWysiwygToXml();
    void syncXmlToWysiwyg();

private:
    void setupUI();
    void rebuildPageTabs();
    void saveCurrentPage();
    void loadPage(int index);
    void removePage(int index);

    void syncFromWysiwyg();
    void syncFromXml();
    void updateWysiwygFromXml();
    void updateXmlFromWysiwyg();

    QString wysiwygToXml(const QString &html) const;
    QString xmlToWysiwyg(const QString &xml) const;
    int xmlContentHash(const QString &xml) const;
    int htmlContentHash(const QString &html) const;

    QTabWidget *m_pageTabs;
    QTabWidget *m_viewTabs;

    QWidget *m_wysiwygPage;
    QTextEdit *m_wysiwygEditor;

    QWidget *m_xmlPage;
    XmlEditor *m_xmlEditor;

    BookDocument *m_document;
    int m_currentPageIndex;
    bool m_syncing;

    QTimer m_syncWysiwygTimer;
    QTimer m_syncXmlTimer;
    int m_lastWysiwygHash;
    int m_lastXmlHash;
};

#endif // EDITORTABS_H
