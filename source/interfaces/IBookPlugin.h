#ifndef IBOOKPLUGIN_H
#define IBOOKPLUGIN_H

#include <QString>
#include <QStringList>

class BookDocument;

/**
 * @brief Контекст приложения, передаваемый плагину при инициализации.
 *
 * Предоставляет плагину доступ к текущему документу и логированию.
 * Создаётся ядром приложения и передаётся в IBookPlugin::initialize().
 */
class BookContext
{
public:
    virtual ~BookContext() = default;

    /** @brief Возвращает указатель на текущий открытый документ (или nullptr). */
    virtual BookDocument *currentDocument() const = 0;

    /** @brief Записывает сообщение в лог приложения (видно в консоли/панели логов). */
    virtual void log(const QString &message) = 0;
};

/** IID для Qt-системы плагинов — используется в Q_PLUGIN_METADATA. */
#define IBookPlugin_iid "com.bookcreateedit.plugin/1.0"

/**
 * @brief Базовый интерфейс всех плагинов приложения BookCreateEdit.
 *
 * Каждый плагин (формат, утилита, рендерер и т.д.) должен наследовать
 * этот интерфейс и реализовать все чисто-виртуальные методы.
 *
 * Жизненный цикл плагина:
 *   1. Ядро загружает DLL/плагин через Qt Plugin System.
 *   2. Вызывает initialize(ctx) — плагин сохраняет контекст и выполняет
 *      первую настройку.
 *   3. Плагин работает, обрабатывая запросы через свою специфичную
 *      функциональность (IBookFormat, IBookTranslator и т.д.).
 *   4. При выгрузке вызывается shutdown() — плагин освобождает ресурсы.
 */
class IBookPlugin
{
public:
    virtual ~IBookPlugin() = default;

    /** @brief Краткое имя плагина (для отображения в списке плагинов). */
    virtual QString name() const = 0;

    /** @brief Человекочитаемое описание того, что делает плагин. */
    virtual QString description() const = 0;

    /** @brief Версия плагина в формате "major.minor.patch". */
    virtual QString version() const = 0;

    /** @brief Имя автора плагина. */
    virtual QString author() const = 0;

    /** @brief Список зависимостей плагина (имена других плагинов или библиотек). */
    virtual QStringList dependencies() const = 0;

    /**
     * @brief Инициализация плагина.
     * @param ctx — контекст приложения; плагин должен сохранить указатель
     *              для дальнейшего использования (логирование, доступ к документу).
     *
     * Вызывается один раз при загрузке плагина. Здесь следует зарегистрировать
     * обработчики событий, загрузить ресурсы и т.д.
     */
    virtual void initialize(BookContext *ctx) = 0;

    /**
     * @brief Завершение работы плагина.
     *
     * Вызывается при выгрузке плагина или закрытии приложения.
     * Здесь нужно освободить все выделенные ресурсы.
     */
    virtual void shutdown() = 0;

    /** @brief Возвращает true, если плагин был успешно инициализирован. */
    virtual bool isInitialized() const = 0;
};

Q_DECLARE_INTERFACE(IBookPlugin, IBookPlugin_iid)

#endif // IBOOKPLUGIN_H
