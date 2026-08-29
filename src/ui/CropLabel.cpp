#include "CropLabel.h"

#include <QPainter>

namespace magnify::ui {

CropLabel::CropLabel(QWidget *parent) : QLabel(parent) {
    setCursor(Qt::CrossCursor);
}

void CropLabel::setPreviewImage(const QImage &displayImage, const QSize &originalSize) {
    m_basePixmap = QPixmap::fromImage(displayImage);
    m_imageSize = originalSize;
    setFixedSize(m_basePixmap.size());
    redraw();
}

void CropLabel::clearCrop() {
    m_cropRect = QRect();
    redraw();
    emit cropChanged();
}

QPoint CropLabel::clampToPixmap(const QPoint &p) const {
    return QPoint(qBound(0, p.x(), m_basePixmap.width() - 1), qBound(0, p.y(), m_basePixmap.height() - 1));
}

void CropLabel::mousePressEvent(QMouseEvent *event) {
    if (m_basePixmap.isNull()) {
        return;
    }
    m_dragStartWidget = clampToPixmap(event->pos());
    m_dragging = true;
}

void CropLabel::mouseMoveEvent(QMouseEvent *event) {
    if (!m_dragging) {
        return;
    }
    const QPoint current = clampToPixmap(event->pos());
    const QRect widgetRect = QRect(m_dragStartWidget, current).normalized();

    if (m_basePixmap.width() > 0 && m_basePixmap.height() > 0) {
        const double scaleX = static_cast<double>(m_imageSize.width()) / m_basePixmap.width();
        const double scaleY = static_cast<double>(m_imageSize.height()) / m_basePixmap.height();
        m_cropRect = QRect(static_cast<int>(widgetRect.x() * scaleX), static_cast<int>(widgetRect.y() * scaleY),
                            qMax(1, static_cast<int>(widgetRect.width() * scaleX)),
                            qMax(1, static_cast<int>(widgetRect.height() * scaleY)));
    }
    redraw();
}

void CropLabel::mouseReleaseEvent(QMouseEvent *) {
    if (m_dragging) {
        m_dragging = false;
        // A click without a real drag (near-zero size) means "no crop".
        if (m_cropRect.width() < 4 || m_cropRect.height() < 4) {
            m_cropRect = QRect();
            redraw();
        }
        emit cropChanged();
    }
}

void CropLabel::redraw() {
    if (m_basePixmap.isNull()) {
        return;
    }
    QPixmap overlaid = m_basePixmap;
    if (m_cropRect.isValid() && m_imageSize.width() > 0 && m_imageSize.height() > 0) {
        const double scaleX = static_cast<double>(m_basePixmap.width()) / m_imageSize.width();
        const double scaleY = static_cast<double>(m_basePixmap.height()) / m_imageSize.height();
        const QRect widgetRect(static_cast<int>(m_cropRect.x() * scaleX), static_cast<int>(m_cropRect.y() * scaleY),
                                static_cast<int>(m_cropRect.width() * scaleX),
                                static_cast<int>(m_cropRect.height() * scaleY));

        QPainter painter(&overlaid);
        painter.setPen(QPen(Qt::red, 2));
        painter.setBrush(QColor(255, 0, 0, 40));
        painter.drawRect(widgetRect.adjusted(0, 0, -1, -1));
    }
    setPixmap(overlaid);
}

} // namespace magnify::ui
