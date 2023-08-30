/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "splitbar.h"
#include "splitmanage.h"
#include "wayland_server.h"
#include "workspace.h"


namespace KWin {

SplitBar::SplitBar(QString screenName)
    : QWidget()
    , m_screenName(screenName)
{
    setWindowFlags(Qt::X11BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus);
    if (waylandServer()) {
        setWindowFlags(Qt::FramelessWindowHint);
    }
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowTitle(screenName);
    connect(workspace()->getSplitManage(), &SplitManage::signalSplitWindow, this, &SplitBar::slotUpdateState);
    // setWindowOpacity(1.0);
    setGeometry(950, 0, 20, 1080);
    m_opacityEffect = new QGraphicsOpacityEffect;
    setGraphicsEffect(m_opacityEffect);
    m_opacityEffect->setOpacity(0.7);
    show();
}

SplitBar::~SplitBar()
{
}

void SplitBar::mousePressEvent(QMouseEvent* e)
{
}

void SplitBar::mouseMoveEvent(QMouseEvent*e)
{
    move(e->screenPos().x(), 0);
    Q_EMIT splitbarPosChanged(m_screenName, e->screenPos(), false);
}

void SplitBar::mouseReleaseEvent(QMouseEvent* e)
{
    // m_mainWindowPress = false;
    Q_EMIT splitbarPosChanged(m_screenName, QPointF(), true);
}

void SplitBar::enterEvent(QEvent *)
{
    // if (waylandServer())
    //     setWindowOpacity(1.0);
    m_opacityEffect->setOpacity(1.0);
}

void SplitBar::leaveEvent(QEvent *)
{
    // if (waylandServer())
    //     setWindowOpacity(0.0);
    m_opacityEffect->setOpacity(0.0);
}

void SplitBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter SplitBarPainter(this);
    QPen pen;
    pen.setColor(QColor("#BBBBBB"));
    pen.setWidth(1);
    SplitBarPainter.setRenderHint(QPainter::Antialiasing, true);
    SplitBarPainter.setBrush(QColor("#DFDFDF"));
    SplitBarPainter.setPen(pen);
    SplitBarPainter.drawRect(0, 0, width(), height());

    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal radius = 4;

    // QRectF rect = QRect(6, m_workspaceRect.height()/2-50, 9, 100);
    QRectF rect = QRect(6, 1080/2-50, 9, 100);
    QPainterPath path;

    path.moveTo(rect.bottomRight() - QPointF(0, radius));
    path.lineTo(rect.topRight() + QPointF(0, radius));
    path.arcTo(QRectF(QPointF(rect.topRight() - QPointF(radius * 2, 0)), QSize(radius * 2, radius *2)), 0, 90);
    path.lineTo(rect.topLeft() + QPointF(radius, 0));
    path.arcTo(QRectF(QPointF(rect.topLeft()), QSize(radius * 2, radius * 2)), 90, 90);
    path.lineTo(rect.bottomLeft() - QPointF(0, radius));
    path.arcTo(QRectF(QPointF(rect.bottomLeft() - QPointF(0, radius * 2)), QSize(radius * 2, radius * 2)), 180, 90);
    path.lineTo(rect.bottomLeft() + QPointF(radius, 0));
    path.arcTo(QRectF(QPointF(rect.bottomRight() - QPointF(radius * 2, radius * 2)), QSize(radius * 2, radius * 2)), 270, 90);
    painter.fillPath(path, QColor(Qt::gray));
    QWidget::paintEvent(event);
}

void SplitBar::slotUpdateState(QString &name, Window *w)
{
    if (m_screenName != name)
        return;
    if (w == nullptr) {
        // m_opacityEffect->setOpacity(0.0);
        setGeometry(0, 0, 1, 1);
        return;
    }

    QRectF geo = w->clientGeometry();
    if (w->quickTileMode() == int(QuickTileFlag::Left)) {
        setGeometry(geo.x() + geo.width() - 10, geo.y(), 20, geo.height());
    } else if (w->quickTileMode() == int(QuickTileFlag::Right)) {
        setGeometry(geo.x() -10, geo.y(), 20, geo.height());
    }

    show();
    update();
}

}
