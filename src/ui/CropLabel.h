#pragma once

#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QRect>
#include <QSize>

namespace magnify::ui {

// Displays an image at a fixed scale (no letterboxing — the label is sized
// exactly to the scaled pixmap) and lets the user drag out a crop
// rectangle directly on it. Reports the rectangle back in the *original*
// image's pixel coordinates, not widget coordinates.
class CropLabel : public QLabel {
    Q_OBJECT
public:
    explicit CropLabel(QWidget *parent = nullptr);

    // displayImage should already reflect color/blur/text so the crop
    // handles are drawn over what the user is actually about to get; it may
    // be scaled down from the real image just to fit the dialog.
    // originalSize is the real, full-resolution image size — cropRect() is
    // always reported in that coordinate space, regardless of how small
    // displayImage actually is on screen.
    void setPreviewImage(const QImage &displayImage, const QSize &originalSize);

    QRect cropRect() const { return m_cropRect; }
    void clearCrop();

signals:
    void cropChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void redraw();
    QPoint clampToPixmap(const QPoint &p) const;

    QPixmap m_basePixmap;
    QSize m_imageSize; // the original (pre-scale) image size cropRect() is expressed in
    QRect m_cropRect;  // in ORIGINAL image coordinates; null = no crop
    QPoint m_dragStartWidget;
    bool m_dragging = false;
};

} // namespace magnify::ui
