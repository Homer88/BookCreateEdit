#ifndef GITPLUGIN_H
#define GITPLUGIN_H

#include <QObject>
#include <QString>

class BookContext;

/**
 * @brief Плагин публикации книг на GitHub.
 *
 * Предоставляет функцию публикации .bceproj проекта в Git-репозиторий:
 *   - Инициализация git-репозитория (git init)
 *   - Добавление файлов (git add)
 *   - Коммит (git commit)
 *   - Создание/привязка к репозиторию на GitHub (git remote add)
 *   - Пуш (git push)
 *
 * Настройки хранятся в SettingsManager (секция Git):
 *   - git/login — логин GitHub
 *   - git/password — токен доступа (PAT)
 *   - git/apiKey — API ключ (опционально)
 *   - git/remoteUrl — URL удалённого репозитория
 */
class GitPlugin : public QObject
{
    Q_OBJECT

public:
    explicit GitPlugin(QObject *parent = nullptr);
    ~GitPlugin();

    QString name() const;
    QString description() const;
    QString version() const;

    /**
     * @brief Публикует проект в Git-репозиторий.
     * @param projectDir — каталог проекта (содержит .bceproj и .bce файлы).
     * @param commitMessage — сообщение коммита.
     * @return true при успехе.
     */
    bool publishToGit(const QString &projectDir, const QString &commitMessage);

    /**
     * @brief Инициализирует git-репозиторий в каталоге.
     * @param projectDir — каталог проекта.
     * @return true при успехе.
     */
    bool initRepository(const QString &projectDir);

    /**
     * @brief Создаёт .gitignore в каталоге проекта.
     * @param projectDir — каталог проекта.
     */
    void createGitIgnore(const QString &projectDir);

    /** @brief Устанавливает контекст приложения для логирования. */
    void setContext(BookContext *ctx);

private:
    /** @brief Выполняет shell-команду и возвращает вывод. */
    QString runCommand(const QString &command, const QString &workingDir);

    /** @brief Проверяет доступность git в системе. */
    bool isGitAvailable();

    BookContext *m_ctx;   ///< Контекст приложения
};

#endif // GITPLUGIN_H
