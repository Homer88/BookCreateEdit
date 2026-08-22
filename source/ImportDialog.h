#ifndef IMPORTDIALOG_H
#define IMPORTDIALOG_H

#include <QDialog>

class QComboBox;
class QLineEdit;
class QLabel;
class BookDocument;

class ImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportDialog(BookDocument *doc, QWidget *parent = nullptr);

private slots:
    void onBrowse();
    void onFormatChanged(int index);
    void onImport();

private:
    void buildFormatList();
    void updateFileFilter();

    BookDocument *m_document;
    QComboBox *m_formatCombo;
    QLineEdit *m_filePathEdit;
    QLabel *m_statusLabel;
};

#endif // IMPORTDIALOG_H
