#include "ParticipantRowWidget.h"

#include <algorithm>
#include <QCheckBox>

#include "ui_participant_item.h"

ParticipantRowWidget::ParticipantRowWidget(
    const QString& clientId,
    bool isSelf,
    bool talkChecked,
    bool muteChecked,
    QWidget* parent)
    : QWidget(parent),
      clientId_(clientId),
      isSelf_(isSelf),
      ui_(new Ui::ParticipantItemForm) {
    ui_->setupUi(this);

    ui_->participantName->setText(clientId_ + (isSelf_ ? " (me)" : ""));
    ui_->talkCheckbox->setChecked(talkChecked);
    ui_->muteCheckbox->setChecked(muteChecked);
    ui_->participantVolumeBar->setRange(0, 100);
    ui_->participantVolumeBar->setValue(0);
    if (isSelf_) {
        ui_->talkCheckbox->setEnabled(false);
    }

    connect(ui_->talkCheckbox, &QCheckBox::toggled, this, &ParticipantRowWidget::onTalkToggled);
    connect(ui_->muteCheckbox, &QCheckBox::toggled, this, &ParticipantRowWidget::onMuteToggled);
}

ParticipantRowWidget::~ParticipantRowWidget() { delete ui_; }

void ParticipantRowWidget::onTalkToggled(bool checked) { emit talkToggled(clientId_, checked); }

void ParticipantRowWidget::onMuteToggled(bool checked) { emit muteToggled(clientId_, checked); }

void ParticipantRowWidget::setTalkChecked(bool enabled) {
    ui_->talkCheckbox->blockSignals(true);
    ui_->talkCheckbox->setChecked(enabled);
    ui_->talkCheckbox->blockSignals(false);
}

void ParticipantRowWidget::setMuteChecked(bool enabled) {
    ui_->muteCheckbox->blockSignals(true);
    ui_->muteCheckbox->setChecked(enabled);
    ui_->muteCheckbox->blockSignals(false);
}

void ParticipantRowWidget::setVolume(int value) {
    const int clamped = std::max(0, std::min(100, value));
    ui_->participantVolumeBar->setValue(clamped);
}

void ParticipantRowWidget::setMicStatus(bool isOn) { ui_->micStatusLabel->setText(isOn ? "Mic: On" : "Mic: Off"); }
