#ifndef LEFTPANEL_H
#define LEFTPANEL_H

#include <QWidget>

class QTabWidget;
class QTreeWidget;
class QListWidget;
class QTreeWidgetItem;
class QLineEdit;
class BookDocument;

class LeftPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LeftPanel(QWidget *parent = nullptr);

    void setDocument(BookDocument *doc);
    BookDocument *document() const { return m_document; }

    QTreeWidget *treeOfContents() const { return m_treeContents; }

signals:
    void pageSelected(int pageIndex);
    void pageRenamed(int pageIndex, const QString &newTitle);
    void pageDeleted(int pageIndex);
    void pageMoved(int fromIndex, int toIndex);

public slots:
    void refreshContents();

private slots:
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onMetaEdited();
    void showContextMenu(const QPoint &pos);

private:
    void setupUI();
    void populateFromDocument();

    QTabWidget *m_tabWidget;
    QTreeWidget *m_treeContents;
    QListWidget *m_fileList;
    QTreeWidget *m_projectSettings;

    QLineEdit *m_edtTitle;
    QLineEdit *m_edtAuthor;
    QLineEdit *m_edtLanguage;

    BookDocument *m_document;
};

#endif // LEFTPANEL_H
