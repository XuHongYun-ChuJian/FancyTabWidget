// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fancytabwidget.h"

#include <QCommonStyle>
#include <QDebug>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmapCache>
#include <QStackedLayout>
#include <QStatusBar>
#include <QStyleFactory>
#include <QStyleOption>
#include <QToolTip>
#include <QVBoxLayout>

#include "qapplication.h"
#include "qmenu.h"

static const int kMenuButtonWidth = 16;

namespace Core {
namespace Constants {

const int MODEBAR_ICON_SIZE             = 34;
const int MODEBAR_ICONSONLY_BUTTON_SIZE = MODEBAR_ICON_SIZE + 4;
}  // namespace Constants
}  // namespace Core

QPixmap setPixmapColor(QPixmap p, QColor color)
{
    QPixmap pixmap = p;
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    return pixmap;
}

void drawArrow(QStyle::PrimitiveElement element,
               QPainter* painter,
               const QStyleOption* option,
               const QColor& disableColor,
               const QColor& baseColor)
{
    if (option->rect.width() <= 1 || option->rect.height() <= 1)
    {
        return;
    }

    const qreal devicePixelRatio = painter->device()->devicePixelRatio();
    const bool enabled           = option->state & QStyle::State_Enabled;
    QRect r                      = option->rect;
    int size                     = qMin(r.height(), r.width());
    QPixmap pixmap;
    const QString pixmapName = QString::asprintf("StyleHelper::drawArrow-%d-%d-%d-%f-d%-d%",
                                                 element,
                                                 size,
                                                 enabled,
                                                 devicePixelRatio,
                                                 baseColor.rgb(),
                                                 disableColor.rgb());
    if (!QPixmapCache::find(pixmapName, &pixmap))
    {
        QImage image(size * devicePixelRatio, size * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform, true);

        QStyleOption tweakedOption(*option);
        tweakedOption.state = QStyle::State_Enabled;

        auto drawCommonStyleArrow = [ &tweakedOption, element, &painter ](const QRect& rect,
                                                                          const QColor& color) -> void
        {
            static const QCommonStyle* const style = qobject_cast<QCommonStyle*>(QApplication::style());
            if (!style)
            {
                return;
            }

            QPalette pal = tweakedOption.palette;
            pal.setBrush(QPalette::Base, pal.text());  // Base and Text differ, causing a detachment.
            // Inspired by tst_QPalette::cacheKey()
            pal.setColor(QPalette::ButtonText, color.rgb());

            tweakedOption.palette = pal;
            tweakedOption.rect    = rect;
            painter.setOpacity(color.alphaF());
            style->QCommonStyle::drawPrimitive(element, &tweakedOption, &painter);
        };

        if (!enabled)
        {
            drawCommonStyleArrow(image.rect(), disableColor);
        }
        else
        {
            drawCommonStyleArrow(image.rect(), baseColor);
        }
        painter.end();
        pixmap = QPixmap::fromImage(image);
        pixmap.setDevicePixelRatio(devicePixelRatio);
        QPixmapCache::insert(pixmapName, pixmap);
    }
    int xOffset = r.x() + (r.width() - size) / 2;
    int yOffset = r.y() + (r.height() - size) / 2;
    painter->drawPixmap(xOffset, yOffset, pixmap);
}

void FancyTab::fadeIn()
{
    m_animator.stop();
    m_animator.setDuration(80);
    m_animator.setEndValue(1);
    m_animator.start();
}

void FancyTab::fadeOut()
{
    m_animator.stop();
    m_animator.setDuration(160);
    m_animator.setEndValue(0);
    m_animator.start();
}

void FancyTab::setFader(qreal value)
{
    m_fader = value;
    m_tabbar->update();
}

FancyTabBar::FancyTabBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("FancyTabBar");
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);  // Needed for hover events
}

QSize FancyTabBar::tabSizeHint(bool minimum) const
{
    int pWidth, pHeight;
    if (m_iconsOnly)
    {
        pWidth  = Core::Constants::MODEBAR_ICONSONLY_BUTTON_SIZE;
        pHeight = Core::Constants::MODEBAR_ICONSONLY_BUTTON_SIZE / (minimum ? 3 : 1);
    }
    else
    {
        const QFont boldFont;
        const QFontMetrics fm(boldFont);
        const int spacing = 8;
        const int width   = 60 + spacing + 2;
        int maxLabelwidth = 0;
        for (auto tab : std::as_const(m_tabs))
        {
            const int width = fm.horizontalAdvance(tab->text);
            if (width > maxLabelwidth)
            {
                maxLabelwidth = width;
            }
        }
        const int iconHeight = minimum ? 0 : 32;
        pWidth               = qMax(width, maxLabelwidth + 4);
        pHeight              = iconHeight + spacing + fm.height();
    }

    return { pWidth, pHeight };
}

void FancyTabBar::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform, true);

    p.fillRect(event->rect(), getFancyTabBarBackgroundColor());

    int visibleIndex        = 0;
    int visibleCurrentIndex = -1;
    for (int i = 0; i < count(); ++i)
    {
        if (!m_tabs.at(i)->visible)
        {
            continue;
        }
        if (i != currentIndex())
        {
            paintTab(&p, i, visibleIndex, QIcon::Off);
        }
        else
        {
            visibleCurrentIndex = visibleIndex;
        }
        ++visibleIndex;
    }

    // paint active tab last, since it overlaps the neighbors
    if (currentIndex() != -1)
    {
        paintTab(&p, currentIndex(), visibleCurrentIndex, QIcon::On);
    }
}

// Handle hover events for mouse fade ins
void FancyTabBar::mouseMoveEvent(QMouseEvent* event)
{
    int newHover     = -1;
    int visibleIndex = 0;
    for (int i = 0; i < count(); ++i)
    {
        if (!m_tabs.at(i)->visible)
        {
            continue;
        }
        const QRect area = tabRect(visibleIndex);
        if (area.contains(event->pos()))
        {
            newHover = i;
            break;
        }
        ++visibleIndex;
    }
    if (newHover == m_hoverIndex)
    {
        return;
    }

    if (validIndex(m_hoverIndex))
    {
        m_tabs[ m_hoverIndex ]->fadeOut();
    }

    m_hoverIndex = newHover;

    if (validIndex(m_hoverIndex))
    {
        m_tabs[ m_hoverIndex ]->fadeIn();
        m_hoverRect = tabRect(visibleIndex);
    }
}

bool FancyTabBar::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        if (validIndex(m_hoverIndex))
        {
            const QString tt = tabToolTip(m_hoverIndex);
            if (!tt.isEmpty())
            {
                QToolTip::showText(static_cast<QHelpEvent*>(event)->globalPos(), tt, this);
                return true;
            }
        }
    }
    return QWidget::event(event);
}

// Resets hover animation on mouse enter
void FancyTabBar::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event)
    m_hoverRect  = QRect();
    m_hoverIndex = -1;
}

// Resets hover animation on mouse enter
void FancyTabBar::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    m_hoverIndex = -1;
    m_hoverRect  = QRect();
    for (auto tab : std::as_const(m_tabs))
    {
        tab->fadeOut();
    }
}

QSize FancyTabBar::sizeHint() const
{
    const QSize sh = tabSizeHint();
    return { sh.width(), sh.height() * int(m_tabs.count()) };
}

QSize FancyTabBar::minimumSizeHint() const
{
    const QSize sh = tabSizeHint(true);
    return { sh.width(), sh.height() * int(m_tabs.count()) };
}

QRect FancyTabBar::tabRect(int visibleIndex) const
{
    QSize sh = tabSizeHint();

    if (sh.height() * m_tabs.count() > height())
    {
        sh.setHeight(height() / m_tabs.count());
    }

    return { 0, visibleIndex * sh.height(), sh.width(), sh.height() };
}

int FancyTabBar::visibleIndex(int index) const
{
    int vIndex = 0;
    for (int i = 0; i < m_tabs.size(); ++i)
    {
        if (i == index)
        {
            return vIndex;
        }
        if (m_tabs.at(i)->visible)
        {
            ++vIndex;
        }
    }
    return vIndex;
}

void FancyTabBar::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    menu.addAction(QStringLiteral("Icons and Text"), [ this ]() { setIconsOnly(false); });
    menu.addAction(QStringLiteral("Icons"), [ this ]() { setIconsOnly(true); });

    menu.exec(event->globalPos());
}

void FancyTabBar::mousePressEvent(QMouseEvent* event)
{
    event->accept();
    int visibleIndex = 0;
    for (int index = 0; index < m_tabs.count(); ++index)
    {
        if (!m_tabs.at(index)->visible)
        {
            continue;
        }
        const QRect rect = tabRect(visibleIndex);
        if (rect.contains(event->pos()))
        {
            if (isTabEnabled(index) && event->button() == Qt::LeftButton)
            {
                if (m_tabs.at(index)->hasMenu && (!m_iconsOnly && rect.right() - event->pos().x() <= kMenuButtonWidth))
                {
                    // menu arrow clicked
                    emit menuTriggered(index, event);
                }
                else
                {
                    if (index != m_currentIndex)
                    {
                        emit currentAboutToChange(index);
                        m_currentIndex = index;
                        update();
                        emit currentChanged(m_currentIndex);
                    }
                }
            }
            else if (event->button() == Qt::RightButton)
            {
                emit menuTriggered(index, event);
            }
            return;
        }
        ++visibleIndex;
    }
    // not in a mode button
    if (event->button() == Qt::RightButton)
    {
        emit menuTriggered(-1, event);
    }
}

static void paintHighlight(QPainter* painter, const QRect& rect, const QColor& color)
{
    QRect accentRect = rect;
    accentRect.setWidth(2);
    painter->fillRect(accentRect, color);
}

static void paintIcon(QPainter* painter,
                      const QRect& rect,
                      const QIcon& icon,
                      QIcon::State iconState,
                      bool enabled,
                      bool selected,
                      QColor c)
{
    painter->save();
    const QIcon::Mode iconMode = enabled ? (selected ? QIcon::Active : QIcon::Normal) : QIcon::Disabled;
    QRect iconRect(0, 0, Core::Constants::MODEBAR_ICON_SIZE, Core::Constants::MODEBAR_ICON_SIZE);
    iconRect.moveCenter(rect.center());
    iconRect = iconRect.intersected(rect);

    if (!enabled)
    {
        painter->setOpacity(0.7);
    }

    QPixmap pixmap = icon.pixmap(iconRect.size(), iconMode, iconState);
    pixmap         = setPixmapColor(pixmap, c);
    painter->drawPixmap(iconRect, pixmap);

    painter->restore();
}

void FancyTabBar::paintIconAndText(QPainter* painter,
                                   const QRect& rect,
                                   const QIcon& icon,
                                   QIcon::State iconState,
                                   const QString& text,
                                   bool enabled,
                                   bool selected) const
{
    painter->save();
    QFont boldFont = qApp->font();
    boldFont.setPointSize(8);
    boldFont.setBold(true);
    painter->setFont(boldFont);

    const bool drawIcon = rect.height() > 36;
    if (drawIcon)
    {
        const int textHeight = painter->fontMetrics().boundingRect(rect, Qt::TextWordWrap, text).height();
        const QRect tabIconRect(rect.adjusted(0, 4, 0, -textHeight));
        const QIcon::Mode iconMode = enabled ? (selected ? QIcon::Active : QIcon::Normal) : QIcon::Disabled;
        QRect iconRect(0, 0, Core::Constants::MODEBAR_ICON_SIZE, Core::Constants::MODEBAR_ICON_SIZE);
        iconRect.moveCenter(tabIconRect.center());
        iconRect = iconRect.intersected(tabIconRect);
        if (!enabled)
        {
            painter->setOpacity(0.7);
        }

        QPixmap pixmap = icon.pixmap(iconRect.size(), iconMode, iconState);
        pixmap         = setPixmapColor(pixmap, getFancyTabBarIconColor());
        painter->drawPixmap(iconRect, pixmap);
    }

    painter->setOpacity(1.0);  // FIXME: was 0.7 before?
    if (enabled)
    {
        painter->setPen(selected ? getFancyTabWidgetEnabledSelectedTextColor()
                                 : getFancyTabWidgetEnabledUnselectedTextColor());
    }
    else
    {
        painter->setPen(selected ? getFancyTabWidgetDisabledSelectedTextColor()
                                 : getFancyTabWidgetDisabledUnselectedTextColor());
    }

    painter->translate(0, -1);
    QRect tabTextRect(rect);
    tabTextRect.translate(0, drawIcon ? -2 : 1);
    const int textFlags = Qt::AlignCenter | (drawIcon ? Qt::AlignBottom : Qt::AlignVCenter) | Qt::TextWordWrap;
    painter->drawText(tabTextRect, textFlags, text);
    painter->restore();
}

void FancyTabBar::paintTab(QPainter* painter, int tabIndex, int visibleIndex, QIcon::State iconState) const
{
    if (!validIndex(tabIndex))
    {
        qWarning("invalid index");
        return;
    }
    painter->save();

    const FancyTab* tab = m_tabs.at(tabIndex);
    const QRect rect    = tabRect(visibleIndex);
    const bool selected = (tabIndex == m_currentIndex);
    const bool enabled  = isTabEnabled(tabIndex);

    if (selected)
    {
        painter->fillRect(rect, getFancyTabBarSelectedBackgroundColor());
    }

    const qreal fader = tab->fader();
    if (fader > 0 && !selected && enabled)
    {
        painter->save();
        painter->setOpacity(fader);
        painter->fillRect(rect, getFancyToolButtonHoverColor());
        painter->restore();
    }

    if (m_iconsOnly)
    {
        paintIcon(painter, rect, tab->icon, iconState, enabled, selected, getFancyTabBarIconColor());
    }
    else
    {
        paintIconAndText(painter, rect, tab->icon, iconState, tab->text, enabled, selected);
    }

    if (selected)
    {
        paintHighlight(painter, rect, getFancyToolButtonHighlightColor());
    }

    // menu arrow
    if (tab->hasMenu && !m_iconsOnly)
    {
        QStyleOption opt;
        opt.initFrom(this);
        opt.rect = rect.adjusted(rect.width() - kMenuButtonWidth, 0, -8, 0);
        drawArrow(QStyle::PE_IndicatorArrowRight,
                  painter,
                  &opt,
                  getFancyTabWidgetDisabledSelectedTextColor(),
                  getFancyTabBarIconColor());
    }
    painter->restore();
}

void FancyTabBar::setCurrentIndex(int index)
{
    if ((index == -1 || isTabEnabled(index)) && index != m_currentIndex)
    {
        emit currentAboutToChange(index);
        m_currentIndex = index;
        update();
        emit currentChanged(m_currentIndex);
    }
}

void FancyTabBar::setIconsOnly(bool iconsOnly)
{
    m_iconsOnly = iconsOnly;
    updateGeometry();
}

void FancyTabBar::setTabEnabled(int index, bool enable)
{
    Q_ASSERT(index < m_tabs.size());
    Q_ASSERT(index >= 0);

    if (index < m_tabs.size() && index >= 0)
    {
        m_tabs[ index ]->enabled = enable;
        if (m_tabs[ index ]->visible)
        {
            update(tabRect(visibleIndex(index)));
        }
    }
}

bool FancyTabBar::isTabEnabled(int index) const
{
    Q_ASSERT(index < m_tabs.size());
    Q_ASSERT(index >= 0);

    if (index < m_tabs.size() && index >= 0)
    {
        return m_tabs[ index ]->enabled;
    }

    return false;
}

void FancyTabBar::setTabVisible(int index, bool visible)
{
    Q_ASSERT(index < m_tabs.size());
    Q_ASSERT(index >= 0);

    m_tabs[ index ]->visible = visible;
    update();
}

class FancyColorButton : public QWidget
{
    Q_OBJECT
    QPROPERTY_CREATE(QColor, SplitterColor, QColor(0x59, 0x59, 0x59, 0xff))

public:
    explicit FancyColorButton(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    }

    void mousePressEvent(QMouseEvent* ev) override { emit clicked(ev); }

    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);
        QPainter p(this);

        p.setPen(getSplitterColor());
        const QRectF innerRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.drawLine(innerRect.bottomLeft(), innerRect.bottomRight());
    }

signals:
    void clicked(QMouseEvent* ev);
};

#if 1
//////
// FancyTabWidget
//////

FancyTabWidget::FancyTabWidget(QWidget* parent)
    : QWidget(parent)
{
    m_tabBar = new FancyTabBar();

    m_modesStack = new QStackedLayout;

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_tabBar);
    mainLayout->addLayout(m_modesStack);

    connect(m_tabBar, &FancyTabBar::currentAboutToChange, this, &FancyTabWidget::currentAboutToShow);
    connect(m_tabBar, &FancyTabBar::currentChanged, this, &FancyTabWidget::showWidget);
    connect(m_tabBar, &FancyTabBar::menuTriggered, this, &FancyTabWidget::menuTriggered);
}

void FancyTabWidget::insertTab(int index, QWidget* tab, const QIcon& icon, const QString& label, bool hasMenu)
{
    m_modesStack->insertWidget(index, tab);
    m_tabBar->insertTab(index, icon, label, hasMenu);
}

void FancyTabWidget::removeTab(int index)
{
    m_modesStack->removeWidget(m_modesStack->widget(index));
    m_tabBar->removeTab(index);
}

void FancyTabWidget::setBackgroundBrush(const QBrush& brush)
{
    QPalette pal;
    pal.setBrush(QPalette::Mid, brush);
    m_tabBar->setPalette(pal);
}

int FancyTabWidget::currentIndex() const
{
    return m_tabBar->currentIndex();
}

void FancyTabWidget::setCurrentIndex(int index)
{
    m_tabBar->setCurrentIndex(index);
}

void FancyTabWidget::showWidget(int index)
{
    m_modesStack->setCurrentIndex(index);
    QWidget* w = m_modesStack->currentWidget();
    if (w)
    {
        if (QWidget* focusWidget = w->focusWidget())
        {
            w = focusWidget;
        }
        w->setFocus();
    }
    emit currentChanged(index);
}

void FancyTabWidget::setTabToolTip(int index, const QString& toolTip)
{
    m_tabBar->setTabToolTip(index, toolTip);
}

void FancyTabWidget::setTabEnabled(int index, bool enable)
{
    m_tabBar->setTabEnabled(index, enable);
}

bool FancyTabWidget::isTabEnabled(int index) const
{
    return m_tabBar->isTabEnabled(index);
}

void FancyTabWidget::setTabVisible(int index, bool visible)
{
    m_tabBar->setTabVisible(index, visible);
}

void FancyTabWidget::setIconsOnly(bool iconsOnly)
{
    m_tabBar->setIconsOnly(iconsOnly);
}
#endif

#include "fancytabwidget.moc"
