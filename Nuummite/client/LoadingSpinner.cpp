#include "LoadingSpinner.h"

#include <QPainter>
#include <QPen>
#include <QSizePolicy>
#include <QTimer>

LoadingSpinner::LoadingSpinner(QWidget* parent)
    : QWidget(parent),
      animationTimer_(new QTimer(this)) {
    connect(animationTimer_, &QTimer::timeout, this, &LoadingSpinner::advanceAnimationStep);
    animationTimer_->setInterval(50);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hide();
}

void LoadingSpinner::startAnimation() {
    active_ = true;
    show();
    animationTimer_->start();
    update();
}

void LoadingSpinner::stopAnimation() {
    active_ = false;
    animationTimer_->stop();
    hide();
}

bool LoadingSpinner::isActive() const {
    return active_;
}

void LoadingSpinner::setBaseColor(const QColor& color) {
    color_ = color;
    update();
}

void LoadingSpinner::setLineCount(int lines) {
    lineCount_ = qMax(1, lines);
    currentStep_ %= lineCount_;
    update();
}

void LoadingSpinner::setLineThickness(double thickness) {
    thickness_ = thickness;
    update();
}

void LoadingSpinner::setLineLength(double length) {
    length_ = length;
    update();
}

void LoadingSpinner::setInnerRadius(double radius) {
    innerRadius_ = radius;
    update();
}

void LoadingSpinner::advanceAnimationStep() {
    currentStep_ = (currentStep_ + 1) % lineCount_;
    update();
}

void LoadingSpinner::paintEvent(QPaintEvent* /*event*/) {
    if (!active_) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(width() / 2.0, height() / 2.0);

    const double stepAngle = 360.0 / lineCount_;
    for (int i = 0; i < lineCount_; ++i) {
        QColor drawColor = color_;
        const double decay = ((currentStep_ - i + lineCount_) % lineCount_) / static_cast<double>(lineCount_);
        drawColor.setAlphaF(qMax(decay, minOpacity_));

        painter.setPen(QPen(QBrush(drawColor), thickness_, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(0, static_cast<int>(innerRadius_), 0, static_cast<int>(innerRadius_ + length_));
        painter.rotate(stepAngle);
    }
}

QSize LoadingSpinner::sizeHint() const {
    const int footprint = static_cast<int>((innerRadius_ + length_ + thickness_) * 2.0);
    return QSize(footprint, footprint);
}
