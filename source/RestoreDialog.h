#ifndef RESTOREDIALOG_H
#define RESTOREDIALOG_H

#include <QDialog>

class QTreeWidget;

class RestoreDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RestoreDialog(QWidget *parent = nullptr);

private:
    void setupUI();
    void loadBackups();

    QTreeWidget *m_treeBackups;
};

#endif // RESTOREDIALOG_H
