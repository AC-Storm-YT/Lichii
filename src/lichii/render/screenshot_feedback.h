// SPDX-FileCopyrightText: Copyright 2026 Lichii contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QWidget>

class QVariantAnimation;

/// A click-through window over the game that flashes when a screenshot is taken and then shows
/// whether it was saved, like the console does.
class ScreenshotFeedback : public QWidget {
    Q_OBJECT

public:
    explicit ScreenshotFeedback(QWidget* parent = nullptr);

    /// Flashes `area` (global coordinates, the game's picture).
    void Flash(const QRect& area);

    /// Shows `text` in the corner of `area` for a moment.
    void ShowMessage(const QRect& area, const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void Cover(const QRect& area);
    void HideWhenDone();

    QVariantAnimation* flash_animation;
    QVariantAnimation* message_animation;
    qreal flash_opacity = 0.0;
    qreal message_opacity = 0.0;
    QString message;
};
