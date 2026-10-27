#include "gitplugin.h"

#include <QProcess>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QSettings>
#include <QDebug>

GitPlugin::GitPlugin(QObject *parent)
    : QObject(parent)
    , m_ctx(nullptr)
{
}

GitPlugin::~GitPlugin()
{
}

QString GitPlugin::name() const { return "Git Publisher"; }
QString GitPlugin::description() const { return "Публикация книг на GitHub"; }
QString GitPlugin::version() const { return "1.0.0"; }

void GitPlugin::setContext(BookContext *ctx)
{
    m_ctx = ctx;
}

bool GitPlugin::isGitAvailable()
{
    QProcess proc;
    proc.start("git", {"--version"});
    proc.waitForFinished(5000);
    return proc.exitCode() == 0;
}

QString GitPlugin::runCommand(const QString &command, const QString &workingDir)
{
    QProcess proc;
    if (!workingDir.isEmpty())
        proc.setWorkingDirectory(workingDir);

    QStringList args = command.split(' ', QString::SkipEmptyParts);
    if (args.isEmpty()) return QString();

    QString prog = args.takeFirst();
    proc.start(prog, args);
    proc.waitForFinished(30000);

    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    QString errors = QString::fromUtf8(proc.readAllStandardError());

    if (proc.exitCode() != 0 && !errors.isEmpty()) {
        qWarning() << "Git command failed:" << command << "\nError:" << errors;
    }

    return output.trimmed();
}

bool GitPlugin::initRepository(const QString &projectDir)
{
    if (!isGitAvailable()) {
        qWarning() << "Git не найден в системе";
        return false;
    }

    QDir dir(projectDir);
    if (!dir.exists()) return false;

    if (QDir(projectDir + "/.git").exists())
        return true;

    runCommand("git init", projectDir);
    createGitIgnore(projectDir);
    runCommand("git add .", projectDir);
    runCommand("git commit -m \"Initial commit: project setup\"", projectDir);

    return true;
}

void GitPlugin::createGitIgnore(const QString &projectDir)
{
    QString gitignorePath = projectDir + "/.gitignore";
    QFile file(gitignorePath);
    if (file.exists()) return;

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << "# BookCreateEdit .gitignore\n";
        stream << "\n";
        stream << "# Резервные копии\n";
        stream << "*.bak\n";
        stream << "history/\n";
        stream << "backup/\n";
        stream << "\n";
        stream << "# Временные файлы\n";
        stream << "*.tmp\n";
        stream << "*.temp\n";
        stream << "~$*\n";
        stream << "\n";
        stream << "# Системные файлы\n";
        stream << "Thumbs.db\n";
        stream << ".DS_Store\n";
        stream << "\n";
        stream << "# Файлы настроек (секретные)\n";
        stream << "*.key\n";
        stream << "*.pem\n";
        stream << "\n";
        stream << "# Собранные DLL\n";
        stream << "plugins/*.dll\n";
        stream << "build/\n";
        stream << "cmake-build-*/\n";
        file.close();
    }
}

bool GitPlugin::publishToGit(const QString &projectDir, const QString &commitMessage)
{
    if (!isGitAvailable()) {
        qWarning() << "Git не найден в системе";
        return false;
    }

    QSettings s("BookCreateEdit.ini", QSettings::IniFormat);
    QString login = s.value("git/login", "").toString();
    QString token = s.value("git/password", "").toString();
    QString remoteUrl = s.value("git/remoteUrl", "").toString();

    if (login.isEmpty() || token.isEmpty()) {
        qWarning() << "Не настроены логин/токен Git в настройках";
        return false;
    }

    QDir dir(projectDir);
    if (!dir.exists()) return false;

    if (!QDir(projectDir + "/.git").exists()) {
        if (!initRepository(projectDir))
            return false;
    }

    runCommand("git add .", projectDir);

    QString msg = commitMessage.isEmpty()
        ? "Update: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")
        : commitMessage;
    runCommand(QString("git commit -m \"%1\"").arg(msg), projectDir);

    if (!remoteUrl.isEmpty()) {
        QString authUrl = remoteUrl;
        if (!authUrl.contains("@")) {
            authUrl.replace("https://github.com/",
                QString("https://%1@github.com/").arg(token));
        }

        runCommand("git remote remove origin", projectDir);
        runCommand(QString("git remote add origin %1").arg(authUrl), projectDir);
        runCommand("git push -u origin main", projectDir);
        runCommand("git push -u origin master", projectDir);
    }

    return true;
}
