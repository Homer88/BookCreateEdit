#ifndef IMPORTDIALOG_H
#define IMPORTDIALOG_H

#include <QDialog>
#include <QMap>

class QComboBox;
class QLineEdit;
class QLabel;
class BookDocument;

/**
 * @brief Диалог импорта файла в формат книги (.bce).
 *
 * По требованиям dosc.md:
 *   1. Выбирается файл, который конвертируется в наш формат через плагин.
 *   2. Все составные ресурсы файла (картинки из ZIP-контейнеров EPUB/DOCX/
 *      MSHC/FB3, base64-вложения FB2, соседние файлы картинок HTML/MD)
 *      распаковываются в каталог img/ проекта, ссылки в страницах
 *      переписываются на новые пути.
 *   3. После импорта главное окно перезагружает оглавление.
 */
class ImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportDialog(BookDocument *doc, QWidget *parent = nullptr);

    /**
     * @brief Задаёт каталог проекта для распаковки ресурсов (img/ внутри).
     * @param dir Каталог проекта; пустая строка — распаковка не выполняется.
     */
    void setProjectDir(const QString &dir);

private slots:
    void onBrowse();
    void onFormatChanged(int index);
    void onImport();

private:
    void buildFormatList();
    void updateFileFilter();

    /** @brief Извлекает ресурсы импортированного файла в каталог img/ проекта. */
    void extractResources(const QString &sourceFile);

    /** @brief Извлекает картинки из ZIP-контейнера (записи без сжатия). @return число файлов. */
    int extractZipImages(const QByteArray &zip);

    /** @brief Декодирует base64-вложения <binary> файла FB2. @return число файлов. */
    int extractFb2Binaries(const QByteArray &xml);

    /** @brief Копирует картинки-спутники (рядом с исходным файлом). @return число файлов. */
    int copyCompanionImages(const QString &sourceFile);

    /** @brief Подбирает свободное имя в каталоге img (без перезаписи). */
    QString uniqueImgName(const QString &desiredName) const;

    BookDocument *m_document;              ///< Целевой документ
    QComboBox *m_formatCombo;              ///< Выбор формата импорта
    QLineEdit *m_filePathEdit;             ///< Путь к исходному файлу
    QLabel *m_statusLabel;                 ///< Строка состояния диалога
    QString m_projectDir;                  ///< Каталог проекта (для img/)
    QMap<QString, QString> m_resourceMap;  ///< Карта «старая ссылка → img/имя»
};

#endif // IMPORTDIALOG_H
