// SPDX-FileCopyrightText: Copyright 2026 Lichii contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

#include "render/screenshot_feedback.h"

namespace {

constexpr int FlashMs = 250;
constexpr qreal FlashOpacity = 0.7;
// The message holds for most of its time and fades out at the end.
constexpr int MessageMs = 2200;
constexpr qreal MessageFadeStart = 0.75;

} // Anonymous namespace

ScreenshotFeedback::ScreenshotFeedback(QWidget* parent)
    : QWidget(parent), flash_animation{new QVariantAnimation(this)},
      message_animation{new QVariantAnimation(this)} {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);

    flash_animation->setDuration(FlashMs);
    flash_animation->setStartValue(FlashOpacity);
    flash_animation->setEndValue(0.0);
    flash_animation->setEasingCurve(QEasingCurve::OutQuad);
    connect(flash_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        flash_opacity = value.toReal();
        update();
    });
    connect(flash_animation, &QVariantAnimation::finished, this,
            &ScreenshotFeedback::HideWhenDone);

    message_animation->setDuration(MessageMs);
    message_animation->setStartValue(0.0);
    message_animation->setEndValue(1.0);
    connect(message_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
                const qreal progress = value.toReal();
                message_opacity = progress < MessageFadeStart
                                      ? 1.0
                                      : (1.0 - progress) / (1.0 - MessageFadeStart);
                update();
            });
    connect(message_animation, &QVariantAnimation::finished, this,
            &ScreenshotFeedback::HideWhenDone);
}

void ScreenshotFeedback::Flash(const QRect& area) {
    Cover(area);
    flash_opacity = FlashOpacity;
    flash_animation->stop();
    flash_animation->start();
}

void ScreenshotFeedback::ShowMessage(const QRect& area, const QString& text) {
    Cover(area);
    message = text;
    message_opacity = 1.0;
    message_animation->stop();
    message_animation->start();
}

void ScreenshotFeedback::Cover(const QRect& area) {
    setGeometry(area);
    show();
    raise();
}

void ScreenshotFeedback::HideWhenDone() {
    if (flash_animation->state() != QAbstractAnimation::Running &&
        message_animation->state() != QAbstractAnimation::Running) {
        flash_opacity = 0.0;
        message_opacity = 0.0;
        hide();
    }
}

void ScreenshotFeedback::paintEvent(QPaintEvent*) {
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    if (flash_opacity > 0.0) {
        painter.fillRect(rect(), QColor(255, 255, 255, static_cast<int>(flash_opacity * 255)));
    }
    if (message_opacity <= 0.0 || message.isEmpty()) {
        return;
    }

    QFont font = painter.font();
    font.setPointSizeF(font.pointSizeF() * 1.15);
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    const QFontMetrics metrics{font};
    const int padding_x = metrics.height();
    const int padding_y = metrics.height() / 2;
    const QSize box_size{metrics.horizontalAdvance(message) + padding_x * 2,
                         metrics.height() + padding_y * 2};
    const int margin = metrics.height();
    const QRect box{QPoint{margin, height() - margin - box_size.height()}, box_size};

    painter.setOpacity(message_opacity);
    QPainterPath path;
    path.addRoundedRect(box, box_size.height() / 2.0, box_size.height() / 2.0);
    painter.fillPath(path, QColor(20, 20, 24, 220));
    painter.setPen(Qt::white);
    painter.drawText(box, Qt::AlignCenter, message);
}
