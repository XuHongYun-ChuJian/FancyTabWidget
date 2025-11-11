// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QIcon>
#include <QPropertyAnimation>
#include <QWidget>

#define QPROPERTY_CREATE(TYPE, MEM, VALUE)               \
    Q_PROPERTY(TYPE MEM READ get##MEM WRITE set##MEM) \
                                                         \
public:                                                  \
    TYPE get##MEM() const                                \
    {                                                    \
        return p##MEM;                                   \
    }                                                    \
public slots:                                            \
    void set##MEM(TYPE MEM)                              \
    {                                                    \
        if (MEM == p##MEM)                               \
            return;                                      \
        p##MEM = MEM;                                    \
        update();                                        \
    }                                                    \
                                                         \
private:                                                 \
    TYPE p##MEM { VALUE };

QT_BEGIN_NAMESPACE
class QPainter;
class QStackedLayout;
class QStatusBar;
QT_END_NAMESPACE

class FancyTab : public QObject
{
    Q_OBJECT

    Q_PROPERTY(qreal fader READ fader WRITE setFader)

public:
    FancyTab(QWidget* parentTabBar)
        : QObject(parentTabBar)
        , m_tabbar(parentTabBar)
    {
        m_animator.setPropertyName("fader");
        m_animator.setTargetObject(this);
    }

    qreal fader() const { return m_fader; }

    void setFader(qreal qreal);

    void fadeIn();
    void fadeOut();

    QIcon icon;
    QString text;
    QString toolTip;
    bool enabled = false;
    bool visible = true;
    bool hasMenu = false;

private:
    QPropertyAnimation m_animator;
    QWidget* m_tabbar;
    qreal m_fader = 0;
};

class FancyTabBar : public QWidget
{
    Q_OBJECT

    QPROPERTY_CREATE(QColor, FancyTabBarBackgroundColor, QColor(0x23, 0x23, 0x23))
    QPROPERTY_CREATE(QColor, FancyToolButtonHighlightColor, QColor(0xfb, 0xfd, 0xff, 0xbc))
    QPROPERTY_CREATE(QColor, FancyTabWidgetEnabledSelectedTextColor, QColor(0xfb, 0xfd, 0xff, 0xb6))
    QPROPERTY_CREATE(QColor, FancyTabWidgetEnabledUnselectedTextColor, QColor(0xfb, 0xfd, 0xff, 0xb6))
    QPROPERTY_CREATE(QColor, FancyTabWidgetDisabledSelectedTextColor, QColor(0xa5, 0xa6, 0xa7, 0x56))
    QPROPERTY_CREATE(QColor, FancyTabWidgetDisabledUnselectedTextColor, QColor(0xa5, 0xa6, 0xa7, 0x56))
    QPROPERTY_CREATE(QColor, FancyTabBarSelectedBackgroundColor, QColor(0, 0, 0, 0x7a))
    QPROPERTY_CREATE(QColor, FancyToolButtonHoverColor, QColor(0xff, 0xff, 0xff, 0x28))
    QPROPERTY_CREATE(QColor, FancyTabBarIconColor, QColor(0xff, 0xff, 0xff))

public:
    FancyTabBar(QWidget* parent = nullptr);

    bool event(QEvent* event) override;

    void paintEvent(QPaintEvent* event) override;
    void paintTab(QPainter* painter, int tabIndex, int visibleIndex, QIcon::State iconState) const;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

    bool validIndex(int index) const { return index >= 0 && index < m_tabs.count(); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void setTabEnabled(int index, bool enable);
    bool isTabEnabled(int index) const;

    void setTabVisible(int index, bool visible);

    void insertTab(int index, const QIcon& icon, const QString& label, bool hasMenu)
    {
        auto tab     = new FancyTab(this);
        tab->icon    = icon;
        tab->text    = label;
        tab->hasMenu = hasMenu;
        tab->enabled = true;
        m_tabs.insert(index, tab);
        if (m_currentIndex >= index)
        {
            ++m_currentIndex;
        }
        updateGeometry();
    }

    void setEnabled(int index, bool enabled);

    void removeTab(int index)
    {
        FancyTab* tab = m_tabs.takeAt(index);
        delete tab;
        updateGeometry();
    }

    void setCurrentIndex(int index);

    int currentIndex() const { return m_currentIndex; }

    void setTabToolTip(int index, const QString& toolTip) { m_tabs[ index ]->toolTip = toolTip; }

    QString tabToolTip(int index) const { return m_tabs.at(index)->toolTip; }

    void setIconsOnly(bool iconOnly);

    int count() const { return m_tabs.count(); }

    QRect tabRect(int visibleIndex) const;

    int visibleIndex(int index) const;

signals:
    void currentAboutToChange(int index);
    void currentChanged(int index);
    void menuTriggered(int index, QMouseEvent* event);

    // QWidget interface

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

    void paintIconAndText(QPainter* painter,
                          const QRect& rect,
                          const QIcon& icon,
                          QIcon::State iconState,
                          const QString& text,
                          bool enabled,
                          bool selected) const;

private:
    QRect m_hoverRect;
    int m_hoverIndex   = -1;
    int m_currentIndex = -1;
    bool m_iconsOnly   = false;
    QList<FancyTab*> m_tabs;
    QSize tabSizeHint(bool minimum = false) const;
};

#if 1
class FancyTabWidget : public QWidget
{
    Q_OBJECT

public:
    FancyTabWidget(QWidget *parent = nullptr);

    void insertTab(int index, QWidget *tab, const QIcon &icon, const QString &label, bool hasMenu);
    void removeTab(int index);
    void setBackgroundBrush(const QBrush &brush);
    void setTabToolTip(int index, const QString &toolTip);

    int currentIndex() const;

    void setTabEnabled(int index, bool enable);
    bool isTabEnabled(int index) const;
    void setTabVisible(int index, bool visible);

    void setIconsOnly(bool iconsOnly);

signals:
    void currentAboutToShow(int index);
    void currentChanged(int index);
    void menuTriggered(int index, QMouseEvent *event);

public slots:
    void setCurrentIndex(int index);

private:
    void showWidget(int index);

    FancyTabBar *m_tabBar;
    QStackedLayout *m_modesStack;
};
#endif
