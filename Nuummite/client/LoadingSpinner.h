#ifndef LOADING_SPINNER_H
#define LOADING_SPINNER_H

#include <QColor>
#include <QWidget>

class QTimer;

class LoadingSpinner final : public QWidget {
    Q_OBJECT

public:
    explicit LoadingSpinner(QWidget* parent = nullptr);
    ~LoadingSpinner() override = default;

    void startAnimation();
    void stopAnimation();
    bool isActive() const;

    void setBaseColor(const QColor& color);
    void setLineCount(int lines);
    void setLineThickness(double thickness);
    void setLineLength(double length);
    void setInnerRadius(double radius);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;

private slots:
    void advanceAnimationStep();

private:
    QTimer* animationTimer_ = nullptr;
    int currentStep_ = 0;
    bool active_ = false;

    QColor color_ = QColor(30, 142, 62);
    int lineCount_ = 12;
    double thickness_ = 3.5;
    double length_ = 8.0;
    double innerRadius_ = 8.0;
    double minOpacity_ = 0.15;
};

#endif // LOADING_SPINNER_H
