#ifndef PUBLISHDIALOG_H
#define PUBLISHDIALOG_H

#include <QDialog>

class QTreeWidget;
class QTreeWidgetItem;
class QCheckBox;
class QLineEdit;
class BookDocument;

class PublishDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PublishDialog(BookDocument *doc, QWidget *parent = nullptr);

private slots:
    void onPublish();
    void onBrowse();
    void onFormatChanged();

private:
    void populateFormats();

    BookDocument *m_document;
    QTreeWidget *m_treeFormats;
    QLineEdit *m_edtOutputPath;
    QCheckBox *m_chkOpenAfter;
};

#endif // PUBLISHDIALOG_H
