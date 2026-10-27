#include "NewProjectDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>

/** Расширения файлов-изображений → раскладываются в img/. */
static const QStringList kImageExts = {
    "png", "jpg", "jpeg", "gif", "bmp", "svg", "webp"
};

/** Расширения текстовых/документных файлов → content/. */
static const QStringList kContentExts = {
    "bce", "txt", "md", "html", "htm", "xhtml", "rtf", "fb2",
    "xml", "css", "csv", "log"
};

NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Создание проекта");
    resize(560, 480);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // --- Группа «Проект»: имя + расположение ---
    QGroupBox *grpProject = new QGroupBox("Параметры проекта", this);
    QFormLayout *form = new QFormLayout(grpProject);

    m_edtName = new QLineEdit(grpProject);
    m_edtName->setPlaceholderText("Например: Моя книга");
    form->addRow("Название проекта:", m_edtName);

    QHBoxLayout *locRow = new QHBoxLayout();
    m_edtLocation = new QLineEdit(grpProject);
    m_edtLocation->setText(QDir::homePath());
    QPushButton *btnBrowse = new QPushButton("Обзор...", grpProject);
    connect(btnBrowse, &QPushButton::clicked, this, &NewProjectDialog::onBrowse);
    locRow->addWidget(m_edtLocation, 1);
    locRow->addWidget(btnBrowse);
    form->addRow("Каталог:", locRow);

    m_lblFullPath = new QLabel(grpProject);
    m_lblFullPath->setStyleSheet("color: gray;");
    form->addRow("Будет создан:", m_lblFullPath);

    connect(m_edtName, &QLineEdit::textChanged,
            this, &NewProjectDialog::onNameOrLocationChanged);
    connect(m_edtLocation, &QLineEdit::textChanged,
            this, &NewProjectDialog::onNameOrLocationChanged);

    mainLayout->addWidget(grpProject);

    // --- Группа «Импорт файлов» ---
    QGroupBox *grpImport = new QGroupBox(
        "Файлы для импорта (раскладываются по папкам img/ и content/)", this);
    QVBoxLayout *impLayout = new QVBoxLayout(grpImport);

    m_lstFiles = new QListWidget(grpImport);
    impLayout->addWidget(m_lstFiles);

    QHBoxLayout *impBtns = new QHBoxLayout();
    QPushButton *btnAdd = new QPushButton("Импорт файлов...", grpImport);
    QPushButton *btnRemove = new QPushButton("Убрать выбранные", grpImport);
    connect(btnAdd, &QPushButton::clicked, this, &NewProjectDialog::onAddFiles);
    connect(btnRemove, &QPushButton::clicked, this, &NewProjectDialog::onRemoveSelected);
    impBtns->addWidget(btnAdd);
    impBtns->addWidget(btnRemove);
    impBtns->addStretch();
    impLayout->addLayout(impBtns);

    mainLayout->addWidget(grpImport, 1);

    // --- Кнопки ---
    QHBoxLayout *btns = new QHBoxLayout();
    btns->addStretch();
    QPushButton *btnCancel = new QPushButton("Отмена", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    QPushButton *btnCreate = new QPushButton("Создать", this);
    btnCreate->setDefault(true);
    btnCreate->setMinimumWidth(110);
    connect(btnCreate, &QPushButton::clicked, this, [this]() {
        if (projectName().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Создание проекта",
                                 "Введите название проекта.");
            m_edtName->setFocus();
            return;
        }
        QDir root(projectRoot());
        if (root.exists()) {
            auto r = QMessageBox::question(this, "Создание проекта",
                QString("Каталог уже существует:\n%1\nПродолжить в нём?")
                    .arg(root.absolutePath()));
            if (r != QMessageBox::Yes)
                return;
        }
        accept();
    });
    btns->addWidget(btnCancel);
    btns->addWidget(btnCreate);
    mainLayout->addLayout(btns);

    onNameOrLocationChanged();
}

QString NewProjectDialog::projectName() const
{
    return m_edtName->text().trimmed();
}

QString NewProjectDialog::locationDir() const
{
    return m_edtLocation->text().trimmed();
}

QString NewProjectDialog::projectRoot() const
{
    return locationDir() + "/" + projectName();
}

QList<QPair<QString, QString>> NewProjectDialog::importPlan() const
{
    QList<QPair<QString, QString>> plan;
    for (int i = 0; i < m_lstFiles->count(); ++i) {
        QListWidgetItem *it = m_lstFiles->item(i);
        plan.append({ it->data(Qt::UserRole).toString(),
                      it->data(Qt::UserRole + 1).toString() });
    }
    return plan;
}

void NewProjectDialog::onBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        "Выберите каталог для проекта", m_edtLocation->text());
    if (!dir.isEmpty())
        m_edtLocation->setText(dir);
}

void NewProjectDialog::onAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(this,
        "Выберите файлы для импорта",
        QString(),
        "Поддерживаемые файлы (*.png *.jpg *.jpeg *.gif *.bmp *.svg *.webp "
        "*.txt *.md *.html *.htm *.xhtml *.rtf *.fb2 *.xml *.css *.csv);;"
        "Все файлы (*)");
    if (files.isEmpty())
        return;

    for (const QString &f : files) {
        // Не добавляем один и тот же файл дважды
        bool exists = false;
        for (int i = 0; i < m_lstFiles->count(); ++i)
            if (m_lstFiles->item(i)->data(Qt::UserRole).toString() == f)
                { exists = true; break; }
        if (exists)
            continue;

        QString sub = classifyFile(f);
        QListWidgetItem *it = new QListWidgetItem(m_lstFiles);
        it->setText(QString("[%1] %2").arg(sub, f));
        it->setData(Qt::UserRole, f);          // исходный путь
        it->setData(Qt::UserRole + 1, sub);    // целевой подкаталог
    }
}

void NewProjectDialog::onRemoveSelected()
{
    const auto selected = m_lstFiles->selectedItems();
    for (QListWidgetItem *it : selected)
        delete it;
}

void NewProjectDialog::onNameOrLocationChanged()
{
    m_lblFullPath->setText(projectRoot() +
        "   (history/, img/, content/, map/)");
}

QString NewProjectDialog::classifyFile(const QString &filePath)
{
    const QString ext = QFileInfo(filePath).suffix().toLower();

    // Картинки → img/, тексты/документы → content/, остальное → content/
    if (kImageExts.contains(ext))
        return "img";
    return "content";   // включая неизвестные расширения — безопаснее там
}
