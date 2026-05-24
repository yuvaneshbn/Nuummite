#ifndef NUUMMITE_PARTICIPANTROWWIDGET_H
#define NUUMMITE_PARTICIPANTROWWIDGET_H

#include <QWidget>

namespace Ui {
class ParticipantItemForm;
}

class ParticipantRowWidget final : public QWidget {
    Q_OBJECT

public:
    ParticipantRowWidget(const QString& clientId, bool isSelf, bool talkChecked, bool muteChecked, QWidget* parent = nullptr);
    ~ParticipantRowWidget() override;

    QString clientId() const { return clientId_; }
    void setTalkChecked(bool enabled);
    void setMuteChecked(bool enabled);
    void setVolume(int value);
    void setMicStatus(bool isOn);

signals:
    void talkToggled(const QString& clientId, bool enabled);
    void muteToggled(const QString& clientId, bool enabled);

private:
    void onTalkToggled(bool checked);
    void onMuteToggled(bool checked);

    QString clientId_;
    bool isSelf_ = false;
    Ui::ParticipantItemForm* ui_ = nullptr;
};

#endif

