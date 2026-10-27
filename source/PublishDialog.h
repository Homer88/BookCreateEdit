#ifndef PUBLISHDIALOG_H
#define PUBLISHDIALOG_H

#include <QDialog>
#include <QStringList>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QCheckBox;
class QLabel;
class BookDocument;
class ProjectManager;

/**
 * @brief Диалог публикации книги (по требованиям dosc.md).
 *
 *   1. Сетка иконок форматов — по числу загруженных плагинов
 *      (визуально похоже на окно создания проектов Visual Studio).
 *      Иконка берётся из ресурсов (:/icons/fmt_<ext>.svg), а если её нет —
 *      генерируется программно с расширением формата.
 *   2. Строка «Название книги».
 *   3. Строка «Куда сохранить» (+ кнопка обзора).
 *   4. Информация об авторе/годе/издательстве берётся из файла проекта
 *      (.bceproj) и переносится в метаданные книги перед публикацией.
 *   5. Возможен выбор двух и более форматов одновременно (флажки).
 */
class PublishDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор диалога публикации.
     * @param doc     Документ для экспорта.
     * @param project Менеджер проекта (.bceproj); может быть nullptr —
     *                тогда метаданные берутся только из документа.
     */
    explicit PublishDialog(BookDocument *doc, ProjectManager *project,
                           QWidget *parent = nullptr);

private slots:
    void onPublish();
    void onBrowse();

private:
    /** @brief Заполняет сетку форматов из загруженных плагинов. */
    void populateFormats();

    /** @brief Подставляет название книги и каталог по умолчанию. */
    void updateDefaults();

    /** @brief Переносит метаданные проекта в документ перед экспортом. */
    void applyProjectMetaToDocument();

    BookDocument *m_document;        ///< Экспортируемый документ
    ProjectManager *m_project;       ///< Проект .bceproj (может быть nullptr)
    QListWidget *m_lstFormats;       ///< Сетка форматов (иконки + флажки)
    QLineEdit *m_edtBookName;        ///< Название книги
    QLineEdit *m_edtOutputDir;       ///< Каталог сохранения
    QCheckBox *m_chkOpenAfter;       ///< Открыть после публикации
    QLabel *m_lblProjectInfo;        ///< Сводка метаданных проекта
};

#endif // PUBLISHDIALOG_H
