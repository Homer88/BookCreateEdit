#ifndef RIBBONWIDGET_H
#define RIBBONWIDGET_H

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QIcon>
#include <QPropertyAnimation>

class QToolButton;
class QLabel;
class QHBoxLayout;
class QVBoxLayout;

/**
 * @brief Группа кнопок внутри вкладки ленты (например, «Буфер обмена», «Шрифт»).
 */
struct RibbonGroup
{
    QString title;                   ///< Заголовок группы (отображается снизу)
    QList<QToolButton*> buttons;     ///< Кнопки группы
};

/**
 * @brief Пользовательская вкладка ленты с поддержкой закрашенного «Файл».
 *
 * По умолчанию вкладки окрашены в голубой цвет.
 * Для вкладки «Файл» используется синий/тёмный стиль.
 */
class RibbonTabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit RibbonTabBar(QWidget *parent = nullptr);

    /** @brief Устанавливает индекс вкладки «Файл» для особой стилизации. */
    void setFileTabIndex(int index) { m_fileTabIndex = index; }

protected:
    void paintEvent(QPaintEvent *event) override; ///< Отрисовка вкладок с кастомными стилями

private:
    int m_fileTabIndex; ///< Индекс вкладки «Файл» (-1 если нет)
};

/**
 * @brief Содержимое одной вкладки ленты (группы кнопок).
 */
class RibbonTab : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonTab(QWidget *parent = nullptr);

    /**
     * @brief Добавляет группу кнопок на вкладку.
     * @param title   — заголовок группы
     * @param buttons — список кнопок для отображения
     */
    void addGroup(const QString &title, const QList<QToolButton*> &buttons);

    /** @brief Устанавливает 특циальный стиль для вкладки «Файл». */
    void setFileTabStyle(bool isFileTab);

private:
    QWidget *createGroupWidget(const QString &title, const QList<QToolButton*> &buttons,
                               bool isLast); ///< Создаёт виджет группы с рамкой и заголовком
    QHBoxLayout *m_layout;  ///< Горизонтальный layout групп
    bool m_isFileTab;       ///< true для вкладки «Файл»
};

/**
 * @brief Виджет ленты (Ribbon) — основной элемент навигации по функциям приложения.
 *
 * Имитирует ленту Microsoft Office:
 *   - Вкладки вверху (Файл, Главная, Вставка, Формат и т.д.)
 *   - Группы кнопок внутри каждой вкладки
 *   - Вкладка «Файл» выделена другим цветом
 *   - Сворачивание по Ctrl+F1 (скрытие кнопок, остаются только вкладки)
 *   - Анимация сворачивания/разворачивания
 *
 * Пример:
 *   @code
 *   RibbonWidget *ribbon = new RibbonWidget(this);
 *   RibbonTab *home = ribbon->addTab("Главная");
 *   home->addGroup("Буфер обмена", {pasteBtn, cutBtn, copyBtn});
 *   RibbonTab *file = ribbon->addFileTab("Файл");
 *   @endcode
 */
class RibbonWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int ribbonHeight READ ribbonHeight WRITE setRibbonHeight)

public:
    explicit RibbonWidget(QWidget *parent = nullptr);

    /**
     * @brief Добавляет обычную вкладку.
     * @param title — название вкладки.
     * @return Указатель на созданный RibbonTab для добавления групп.
     */
    RibbonTab *addTab(const QString &title);

    /**
     * @brief Добавляет вкладку «Файл» с особым стилем (синий/тёмный фон).
     * @param title — название по умолчанию "Файл".
     * @return Указатель на созданный RibbonTab.
     */
    RibbonTab *addFileTab(const QString &title = "Файл");

    /**
     * @brief Создаёт кнопку для размещения на ленте.
     * @param text    — текст кнопки
     * @param icon    — иконка (может быть пустой)
     * @param tooltip — всплывающая подсказка
     */
    QToolButton *createButton(const QString &text, const QIcon &icon = QIcon(),
                              const QString &tooltip = QString());

    /** @brief Возвращает true, если лента свёрнута (только вкладки). */
    bool isCollapsed() const { return m_collapsed; }

    /** @brief Возвращает текущую высоту содержимого ленты. */
    int ribbonHeight() const { return m_contentHeight; }

    /** @brief Устанавливает высоту ленты (для анимации сворачивания). */
    void setRibbonHeight(int h);

signals:
    /** @brief Сигнал о смене активной вкладки. */
    void tabChanged(int index);

    /** @brief Сигнал о изменении состояния свёрнутости. */
    void collapsedChanged(bool collapsed);

public slots:
    /** @brief Переключает свёрнутость ленты (Ctrl+F1). */
    void toggleCollapse();

protected:
    void keyPressEvent(QKeyEvent *event) override; ///< Обработка Ctrl+F1 для сворачивания

private:
    void setupUI();                        ///< Создаёт tab bar и stacked widget
    void onTabClicked(int index);          ///< Переключает видимую вкладку
    void applyThemeStyle();                ///< Применяет стили темы к ленте
    void updateTabStyle(int index);        ///< Обновляет стиль активной/неактивных вкладок

    RibbonTabBar *m_tabBar;     ///< Панель вкладок
    QStackedWidget *m_stack;    ///< Стек содержимого вкладок
    QList<RibbonTab*> m_tabs;   ///< Все вкладки
    QVBoxLayout *m_mainLayout;  ///< Основной vertical layout

    QWidget *m_contentWidget;   ///< Обёртка для анимации сворачивания
    int m_fullHeight;           ///< Полная высота ленты (развёрнутое состояние)
    int m_contentHeight;        ///< Текущая высота содержимого
    bool m_collapsed;           ///< true = лента свёрнута
    int m_fileTabIndex;         ///< Индекс вкладки «Файл»
};

#endif // RIBBONWIDGET_H
