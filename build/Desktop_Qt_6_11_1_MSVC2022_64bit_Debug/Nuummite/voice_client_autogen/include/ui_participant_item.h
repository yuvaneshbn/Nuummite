/********************************************************************************
** Form generated from reading UI file 'participant_item.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PARTICIPANT_ITEM_H
#define UI_PARTICIPANT_ITEM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ParticipantItemForm
{
public:
    QVBoxLayout *rootLayout;
    QHBoxLayout *rowLayout;
    QLabel *participantName;
    QCheckBox *talkCheckbox;
    QCheckBox *muteCheckbox;
    QLabel *micStatusLabel;
    QProgressBar *participantVolumeBar;

    void setupUi(QWidget *ParticipantItemForm)
    {
        if (ParticipantItemForm->objectName().isEmpty())
            ParticipantItemForm->setObjectName("ParticipantItemForm");
        ParticipantItemForm->resize(266, 75);
        rootLayout = new QVBoxLayout(ParticipantItemForm);
        rootLayout->setObjectName("rootLayout");
        rowLayout = new QHBoxLayout();
        rowLayout->setObjectName("rowLayout");
        participantName = new QLabel(ParticipantItemForm);
        participantName->setObjectName("participantName");

        rowLayout->addWidget(participantName);

        talkCheckbox = new QCheckBox(ParticipantItemForm);
        talkCheckbox->setObjectName("talkCheckbox");

        rowLayout->addWidget(talkCheckbox);

        muteCheckbox = new QCheckBox(ParticipantItemForm);
        muteCheckbox->setObjectName("muteCheckbox");

        rowLayout->addWidget(muteCheckbox);

        micStatusLabel = new QLabel(ParticipantItemForm);
        micStatusLabel->setObjectName("micStatusLabel");

        rowLayout->addWidget(micStatusLabel);


        rootLayout->addLayout(rowLayout);

        participantVolumeBar = new QProgressBar(ParticipantItemForm);
        participantVolumeBar->setObjectName("participantVolumeBar");
        participantVolumeBar->setTextVisible(false);

        rootLayout->addWidget(participantVolumeBar);


        retranslateUi(ParticipantItemForm);

        QMetaObject::connectSlotsByName(ParticipantItemForm);
    } // setupUi

    void retranslateUi(QWidget *ParticipantItemForm)
    {
        participantName->setText(QCoreApplication::translate("ParticipantItemForm", "Client Name", nullptr));
        talkCheckbox->setText(QCoreApplication::translate("ParticipantItemForm", "Talk", nullptr));
        muteCheckbox->setText(QCoreApplication::translate("ParticipantItemForm", "Mute", nullptr));
        micStatusLabel->setText(QCoreApplication::translate("ParticipantItemForm", "Mic: Off", nullptr));
        (void)ParticipantItemForm;
    } // retranslateUi

};

namespace Ui {
    class ParticipantItemForm: public Ui_ParticipantItemForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PARTICIPANT_ITEM_H
