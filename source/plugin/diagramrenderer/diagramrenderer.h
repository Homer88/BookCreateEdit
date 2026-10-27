#ifndef Diagramrenderer_PLUGIN_H
#define Diagramrenderer_PLUGIN_H

#include <QObject>
#include "interfaces/IBookPlugin.h"

/**
 * @brief Плагин отрисовки диаграмм для BookCreateEdit.
 *
 * Реализует интерфейс IBookPlugin и экспортируется системе плагинов
 * через механизм метаданных Qt (Q_PLUGIN_METADATA).
 */
class DiagramrendererPlugin : public QObject, public IBookPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.bookcreateedit.plugin/1.0" FILE "diagramrenderer.json")
    Q_INTERFACES(IBookPlugin)

public:
    /**
     * @brief Конструктор плагина.
     * @param parent Родительский объект Qt (может быть nullptr).
     */
    explicit DiagramrendererPlugin(QObject *parent = nullptr);

    /**
     * @brief Деструктор плагина.
     *
     * Вызывает shutdown(), чтобы гарантировать освобождение ресурсов
     * даже при удалении без явного завершения работы.
     */
    ~DiagramrendererPlugin() override;

    /**
     * @brief Возвращает имя плагина.
     * @return Короткое уникальное имя плагина.
     */
    QString name() const override;

    /**
     * @brief Возвращает описание назначения плагина.
     * @return Строка с описанием плагина.
     */
    QString description() const override;

    /**
     * @brief Возвращает версию плагина.
     * @return Строка версии в формате «X.Y.Z».
     */
    QString version() const override;

    /**
     * @brief Возвращает имя автора плагина.
     * @return Строка с именем автора.
     */
    QString author() const override;

    /**
     * @brief Возвращает список зависимостей плагина.
     * @return Список имён плагинов, необходимых для работы данного плагина.
     */
    QStringList dependencies() const override;

    /**
     * @brief Инициализирует плагин контекстом приложения.
     * @param ctx Указатель на контекст книги/приложения, через который
     *            плагин взаимодействует с системой.
     */
    void initialize(BookContext *ctx) override;

    /**
     * @brief Завершает работу плагина и освобождает выделенные ресурсы.
     */
    void shutdown() override;

    /**
     * @brief Проверяет, выполнена ли инициализация плагина.
     * @return true, если плагин успешно инициализирован, иначе false.
     */
    bool isInitialized() const override;

private:
    BookContext *m_context;   ///< Контекст приложения, полученный при инициализации.
    bool m_initialized;       ///< Флаг успешной инициализации плагина.
};

#endif // Diagramrenderer_PLUGIN_H
