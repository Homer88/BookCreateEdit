#ifndef NEWPROJECTDIALOG_H
#define NEWPROJECTDIALOG_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

class QLineEdit;
class QLabel;
class QListWidget;

/**
 * @brief Диалог создания проекта (по требованиям dosc.md).
 *
 *   1. Вводится название проекта, выбирается каталог — при создании
 *      разворачивается структура: history/, img/, content/, map/.
 *   2. Кнопка «Импорт файлов» — выбранные файлы копируются в проект
 *      и раскладываются по папкам (картинки → img/, документы → content/).
 *   3. После создания загружается пустое оглавление (одна пустая страница).
 */
class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget *parent = nullptr);

    /** @brief Название проекта (имя каталога и .bceproj). */
    QString projectName() const;

    /** @brief Каталог расположения (без имени проекта). */
    QString locationDir() const;

    /** @brief Полный путь создаваемого проекта: locationDir()/projectName(). */
    QString projectRoot() const;

    /**
     * @brief План импорта файлов.
     * @return Список пар «исходный файл → подкаталог проекта» (img или content).
     */
    QList<QPair<QString, QString>> importPlan() const;

private slots:
    void onBrowse();
    void onAddFiles();
    void onRemoveSelected();
    void onNameOrLocationChanged();

private:
    /** @brief Определяет целевой подкаталог по типу файла. */
    static QString classifyFile(const QString &filePath);

    QLineEdit *m_edtName;        ///< Название проекта
    QLineEdit *m_edtLocation;    ///< Каталог расположения
    QLabel *m_lblFullPath;       ///< Предпросмотр полного пути
    QListWidget *m_lstFiles;     ///< Выбранные для импорта файлы
};

#endif // NEWPROJECTDIALOG_H
