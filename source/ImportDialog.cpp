#include "ImportDialog.h"
#include "BookDocument.h"
#include "PluginManager.h"
#include "interfaces/IBookFormat.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDomDocument>
#include <QApplication>

/** Расширения файлов-изображений, распаковываемых из контейнеров. */
static const QStringList kImageExts = {
    "png", "jpg", "jpeg", "gif", "bmp", "svg", "webp"
};

ImportDialog::ImportDialog(BookDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_document(doc)
{
    setWindowTitle("Импорт файла");
    setMinimumWidth(500);
    resize(550, 200);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QFormLayout *formLayout = new QFormLayout();

    m_formatCombo = new QComboBox(this);
    buildFormatList();
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportDialog::onFormatChanged);
    formLayout->addRow("Формат:", m_formatCombo);

    QHBoxLayout *fileLayout = new QHBoxLayout();
    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setPlaceholderText("Выберите файл для импорта...");
    QPushButton *btnBrowse = new QPushButton("Обзор...", this);
    connect(btnBrowse, &QPushButton::clicked, this, &ImportDialog::onBrowse);
    fileLayout->addWidget(m_filePathEdit);
    fileLayout->addWidget(btnBrowse);
    formLayout->addRow("Файл:", fileLayout);

    mainLayout->addLayout(formLayout);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: gray;");
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *btnCancel = new QPushButton("Отмена", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(btnCancel);

    QPushButton *btnImport = new QPushButton("Импорт", this);
    btnImport->setDefault(true);
    btnImport->setMinimumWidth(100);
    connect(btnImport, &QPushButton::clicked, this, &ImportDialog::onImport);
    btnLayout->addWidget(btnImport);

    mainLayout->addLayout(btnLayout);

    if (m_formatCombo->count() > 0)
        updateFileFilter();
}

void ImportDialog::setProjectDir(const QString &dir)
{
    m_projectDir = dir;
}

void ImportDialog::buildFormatList()
{
    m_formatCombo->clear();

    // Список форматов берём из загруженных плагинов; в него попадают
    // только форматы с поддержкой чтения (EXE/ELF/APP/PDF — только запись
    // и в список импорта по dosc.md не попадают).
    QStringList formatNames = PluginManager::instance().availableFormats();
    for (const QString &name : formatNames) {
        IBookFormat *fmt = PluginManager::instance().format(name);
        if (fmt && fmt->canRead())
            m_formatCombo->addItem(fmt->formatName(), name);
    }

    if (m_formatCombo->count() == 0) {
        m_formatCombo->addItem("(нет загруженных форматов)", QString());
        m_formatCombo->setEnabled(false);
    }
}

void ImportDialog::onFormatChanged(int index)
{
    Q_UNUSED(index);
    updateFileFilter();
}

void ImportDialog::updateFileFilter()
{
    int idx = m_formatCombo->currentIndex();
    if (idx < 0) return;

    QString formatId = m_formatCombo->currentData().toString();
    IBookFormat *fmt = PluginManager::instance().format(formatId);
    if (!fmt) return;

    QStringList exts = fmt->supportedExtensions();
    m_statusLabel->setText(
        QString("Поддерживаемые расширения: %1").arg(exts.join(", ")));
}

void ImportDialog::onBrowse()
{
    int idx = m_formatCombo->currentIndex();
    QString filter = "Все файлы (*)";

    if (idx >= 0) {
        QString formatId = m_formatCombo->currentData().toString();
        IBookFormat *fmt = PluginManager::instance().format(formatId);
        if (fmt) {
            QStringList exts = fmt->supportedExtensions();
            QString specific;
            for (const QString &ext : exts)
                specific += (specific.isEmpty() ? QString() : " ") + ("*." + ext);
            filter = QString("%1 (%2);;Все файлы (*)").arg(fmt->formatName(), specific);
        }
    }

    QString filePath = QFileDialog::getOpenFileName(this,
        "Выберите файл для импорта", QString(), filter);
    if (filePath.isEmpty()) return;

    m_filePathEdit->setText(filePath);

    // Автовыбор плагина по расширению выбранного файла
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    for (int i = 0; i < m_formatCombo->count(); ++i) {
        QString formatId = m_formatCombo->itemData(i).toString();
        IBookFormat *fmt = PluginManager::instance().format(formatId);
        if (fmt && fmt->supportedExtensions().contains(ext)) {
            m_formatCombo->setCurrentIndex(i);
            break;
        }
    }
}

QString ImportDialog::uniqueImgName(const QString &desiredName) const
{
    QDir imgDir(m_projectDir + "/img");
    QString base = QFileInfo(desiredName).completeBaseName();
    QString ext = QFileInfo(desiredName).suffix();

    QString name = desiredName;
    int counter = 2;
    while (imgDir.exists(name)) {
        name = ext.isEmpty()
            ? QString("%1_%2").arg(base).arg(counter)
            : QString("%1_%2.%3").arg(base).arg(counter).arg(ext);
        ++counter;
    }
    return name;
}

int ImportDialog::extractZipImages(const QByteArray &zip)
{
    int saved = 0;
    QDir().mkpath(m_projectDir + "/img");

    // Разбор локальных заголовков ZIP; извлекаем только записи без сжатия
    // (наши плагины EPUB/DOCX/MSHC пишут именно так).
    int pos = 0;
    while (pos + 30 <= zip.size()) {
        if (zip.mid(pos, 4) != "PK\x03\x04")
            break;

        quint16 method = quint16(uchar(zip.at(pos + 8))) |
                         (quint16(uchar(zip.at(pos + 9))) << 8);
        quint32 compSize = 0;
        for (int b = 0; b < 4; ++b)
            compSize |= quint32(uchar(zip.at(pos + 18 + b))) << (8 * b);
        quint32 uncompSize = 0;
        for (int b = 0; b < 4; ++b)
            uncompSize |= quint32(uchar(zip.at(pos + 22 + b))) << (8 * b);
        quint16 nameLen = quint16(uchar(zip.at(pos + 26))) |
                          (quint16(uchar(zip.at(pos + 27))) << 8);
        quint16 extraLen = quint16(uchar(zip.at(pos + 28))) |
                           (quint16(uchar(zip.at(pos + 29))) << 8);

        int nameStart = pos + 30;
        if (nameStart + nameLen > zip.size())
            break;

        QString name = QString::fromUtf8(zip.mid(nameStart, nameLen));
        int dataStart = nameStart + nameLen + extraLen;
        if (dataStart + int(compSize) > zip.size())
            break;

        bool isImage = false;
        for (const QString &e : kImageExts)
            if (name.endsWith("." + e, Qt::CaseInsensitive)) { isImage = true; break; }

        if (method == 0 && isImage && compSize > 0) {
            QByteArray data = zip.mid(dataStart, int(compSize));
            QString rel = uniqueImgName(QFileInfo(name).fileName());
            QFile out(m_projectDir + "/img/" + rel);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(data);
                out.close();
                // Карта замены: полный путь внутри архива и просто имя файла
                m_resourceMap.insert(name, "img/" + rel);
                m_resourceMap.insert(QFileInfo(name).fileName(), "img/" + rel);
                ++saved;
            }
        }

        pos = dataStart + int(compSize);
    }
    return saved;
}

int ImportDialog::extractFb2Binaries(const QByteArray &xmlBytes)
{
    QDomDocument doc;
    if (!doc.setContent(xmlBytes))
        return 0;

    int saved = 0;
    QDir().mkpath(m_projectDir + "/img");

    QDomNodeList binaries = doc.elementsByTagName("binary");
    for (int i = 0; i < binaries.count(); ++i) {
        QDomElement bin = binaries.at(i).toElement();
        QString id = bin.attribute("id");
        QString ctype = bin.attribute("content-type");   // например image/png
        QString ext = ctype.contains("/") ? ctype.section('/', -1) : QString("png");
        if (ext == "jpeg") ext = "jpg";
        if (id.isEmpty())
            continue;

        QByteArray data = QByteArray::fromBase64(bin.text().toLatin1());
        if (data.isEmpty())
            continue;

        QString rel = uniqueImgName(id + "." + ext);
        QFile out(m_projectDir + "/img/" + rel);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(data);
            out.close();
            // FB2 ссылается как «#id», некоторые конвертеры — просто «id»
            m_resourceMap.insert("#" + id, "img/" + rel);
            m_resourceMap.insert(id, "img/" + rel);
            ++saved;
        }
    }
    return saved;
}

int ImportDialog::copyCompanionImages(const QString &sourceFile)
{
    if (!m_document)
        return 0;

    QString srcDir = QFileInfo(sourceFile).absolutePath();
    int saved = 0;
    QDir().mkpath(m_projectDir + "/img");

    const QStringList sources = m_document->imageSources();
    for (const QString &src : sources) {
        // Внутренние ссылки и URL пропускаем
        if (src.startsWith('#') || src.startsWith("http:") ||
            src.startsWith("https:") || src.startsWith("data:"))
            continue;

        // Относительный путь разрешаем относительно каталога исходного файла
        QString candidate = QFileInfo(src).isAbsolute()
            ? src : srcDir + "/" + src;
        QFileInfo fi(candidate);
        if (!fi.exists() || !fi.isFile())
            continue;

        QString rel = uniqueImgName(fi.fileName());
        if (QFile::copy(fi.absoluteFilePath(), m_projectDir + "/img/" + rel)) {
            m_resourceMap.insert(src, "img/" + rel);
            ++saved;
        }
    }
    return saved;
}

void ImportDialog::extractResources(const QString &sourceFile)
{
    m_resourceMap.clear();
    if (m_projectDir.isEmpty() || !m_document)
        return;

    QFile f(sourceFile);
    if (!f.open(QIODevice::ReadOnly))
        return;
    QByteArray head = f.read(4);
    bool isZip = head.startsWith("PK\x03\x04");
    bool maybeFb2 = head.startsWith("<?xm") || head.startsWith("<Fiction");
    f.seek(0);
    QByteArray all = f.readAll();
    f.close();

    int count = 0;
    if (isZip)
        count += extractZipImages(all);
    else if (maybeFb2)
        count += extractFb2Binaries(all);

    // Картинки, лежащие рядом с исходным файлом (HTML/MD/Rtf-спутники)
    count += copyCompanionImages(sourceFile);

    // Переписываем ссылки в страницах документа на новые пути img/...
    if (!m_resourceMap.isEmpty())
        m_document->remapImageSources(m_resourceMap);

    if (count > 0)
        m_statusLabel->setText(
            QString("Импорт выполнен. Ресурсов распаковано: %1 → img/").arg(count));
}

void ImportDialog::onImport()
{
    QString filePath = m_filePathEdit->text().trimmed();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "Импорт", "Выберите файл для импорта.");
        return;
    }

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        QMessageBox::warning(this, "Импорт", "Файл не найден: " + filePath);
        return;
    }

    int idx = m_formatCombo->currentIndex();
    if (idx < 0) {
        QMessageBox::warning(this, "Импорт", "Выберите формат.");
        return;
    }

    QString formatId = m_formatCombo->currentData().toString();
    IBookFormat *fmt = PluginManager::instance().format(formatId);
    if (!fmt) {
        QMessageBox::critical(this, "Импорт", "Плагин формата не найден.");
        return;
    }

    if (!m_document) {
        QMessageBox::critical(this, "Импорт", "Документ не создан.");
        return;
    }

    m_statusLabel->setText("Импорт...");
    QApplication::processEvents();   // чтобы статус успел отрисоваться

    bool ok = fmt->read(filePath, m_document);

    if (ok) {
        // Требование dosc.md №2: распаковка составных ресурсов по каталогам
        extractResources(filePath);

        QMessageBox::information(this, "Импорт",
            QString("Файл успешно импортирован.\nСтраниц в документе: %1")
                .arg(m_document->pageCount()));
        accept();   // после accept() главное окно перезагружает оглавление
    } else {
        m_statusLabel->setText("Ошибка импорта.");
        QMessageBox::warning(this, "Импорт",
            "Не удалось импортировать файл.\nПроверьте формат файла.");
    }
}
