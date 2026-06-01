/********************************************************************************
** Form generated from reading UI file 'settings_dialog.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETTINGS_DIALOG_H
#define UI_SETTINGS_DIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SettingsDialogForm
{
public:
    QVBoxLayout *rootLayout;
    QLabel *headerLabel;
    QGroupBox *audioDevicesGroup;
    QFormLayout *audioDevicesLayout;
    QLabel *inputDeviceLabel;
    QComboBox *inputDeviceCombo;
    QLabel *outputDeviceLabel;
    QComboBox *outputDeviceCombo;
    QGroupBox *advancedAudioGroup;
    QVBoxLayout *advancedAudioLayout;
    QLabel *volumeControlHint;
    QGroupBox *networkGroup;
    QFormLayout *networkLayout;
    QLabel *serverIpTitle;
    QHBoxLayout *serverRow;
    QLabel *serverIpValue;
    QPushButton *reconnectButton;
    QHBoxLayout *footerButtonsLayout;
    QSpacerItem *footerSpacer;
    QPushButton *saveCloseButton;
    QPushButton *cancelButton;

    void setupUi(QWidget *SettingsDialogForm)
    {
        if (SettingsDialogForm->objectName().isEmpty())
            SettingsDialogForm->setObjectName("SettingsDialogForm");
        SettingsDialogForm->resize(480, 640);
        rootLayout = new QVBoxLayout(SettingsDialogForm);
        rootLayout->setSpacing(10);
        rootLayout->setObjectName("rootLayout");
        rootLayout->setContentsMargins(12, 12, 12, 12);
        headerLabel = new QLabel(SettingsDialogForm);
        headerLabel->setObjectName("headerLabel");

        rootLayout->addWidget(headerLabel);

        audioDevicesGroup = new QGroupBox(SettingsDialogForm);
        audioDevicesGroup->setObjectName("audioDevicesGroup");
        audioDevicesLayout = new QFormLayout(audioDevicesGroup);
        audioDevicesLayout->setObjectName("audioDevicesLayout");
        audioDevicesLayout->setHorizontalSpacing(8);
        audioDevicesLayout->setVerticalSpacing(6);
        audioDevicesLayout->setContentsMargins(10, 8, 10, 10);
        inputDeviceLabel = new QLabel(audioDevicesGroup);
        inputDeviceLabel->setObjectName("inputDeviceLabel");

        audioDevicesLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, inputDeviceLabel);

        inputDeviceCombo = new QComboBox(audioDevicesGroup);
        inputDeviceCombo->setObjectName("inputDeviceCombo");

        audioDevicesLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, inputDeviceCombo);

        outputDeviceLabel = new QLabel(audioDevicesGroup);
        outputDeviceLabel->setObjectName("outputDeviceLabel");

        audioDevicesLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, outputDeviceLabel);

        outputDeviceCombo = new QComboBox(audioDevicesGroup);
        outputDeviceCombo->setObjectName("outputDeviceCombo");

        audioDevicesLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, outputDeviceCombo);


        rootLayout->addWidget(audioDevicesGroup);

        advancedAudioGroup = new QGroupBox(SettingsDialogForm);
        advancedAudioGroup->setObjectName("advancedAudioGroup");
        advancedAudioLayout = new QVBoxLayout(advancedAudioGroup);
        advancedAudioLayout->setSpacing(8);
        advancedAudioLayout->setObjectName("advancedAudioLayout");
        advancedAudioLayout->setContentsMargins(10, 8, 10, 10);
        volumeControlHint = new QLabel(advancedAudioGroup);
        volumeControlHint->setObjectName("volumeControlHint");

        advancedAudioLayout->addWidget(volumeControlHint);


        rootLayout->addWidget(advancedAudioGroup);

        networkGroup = new QGroupBox(SettingsDialogForm);
        networkGroup->setObjectName("networkGroup");
        networkLayout = new QFormLayout(networkGroup);
        networkLayout->setObjectName("networkLayout");
        networkLayout->setHorizontalSpacing(8);
        networkLayout->setVerticalSpacing(6);
        networkLayout->setContentsMargins(10, 8, 10, 10);
        serverIpTitle = new QLabel(networkGroup);
        serverIpTitle->setObjectName("serverIpTitle");

        networkLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, serverIpTitle);

        serverRow = new QHBoxLayout();
        serverRow->setObjectName("serverRow");
        serverIpValue = new QLabel(networkGroup);
        serverIpValue->setObjectName("serverIpValue");

        serverRow->addWidget(serverIpValue);

        reconnectButton = new QPushButton(networkGroup);
        reconnectButton->setObjectName("reconnectButton");

        serverRow->addWidget(reconnectButton);


        networkLayout->setLayout(0, QFormLayout::ItemRole::FieldRole, serverRow);


        rootLayout->addWidget(networkGroup);

        footerButtonsLayout = new QHBoxLayout();
        footerButtonsLayout->setObjectName("footerButtonsLayout");
        footerSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        footerButtonsLayout->addItem(footerSpacer);

        saveCloseButton = new QPushButton(SettingsDialogForm);
        saveCloseButton->setObjectName("saveCloseButton");

        footerButtonsLayout->addWidget(saveCloseButton);

        cancelButton = new QPushButton(SettingsDialogForm);
        cancelButton->setObjectName("cancelButton");

        footerButtonsLayout->addWidget(cancelButton);


        rootLayout->addLayout(footerButtonsLayout);


        retranslateUi(SettingsDialogForm);

        QMetaObject::connectSlotsByName(SettingsDialogForm);
    } // setupUi

    void retranslateUi(QWidget *SettingsDialogForm)
    {
        SettingsDialogForm->setWindowTitle(QCoreApplication::translate("SettingsDialogForm", "Settings - Nuummite", nullptr));
        headerLabel->setText(QCoreApplication::translate("SettingsDialogForm", "Settings", nullptr));
        audioDevicesGroup->setTitle(QCoreApplication::translate("SettingsDialogForm", "Audio Devices", nullptr));
        inputDeviceLabel->setText(QCoreApplication::translate("SettingsDialogForm", "Input Device:", nullptr));
        outputDeviceLabel->setText(QCoreApplication::translate("SettingsDialogForm", "Output Device:", nullptr));
        advancedAudioGroup->setTitle(QCoreApplication::translate("SettingsDialogForm", "Advanced Audio", nullptr));
        volumeControlHint->setText(QCoreApplication::translate("SettingsDialogForm", "Embed VolumeControl form here", nullptr));
        networkGroup->setTitle(QCoreApplication::translate("SettingsDialogForm", "Network", nullptr));
        serverIpTitle->setText(QCoreApplication::translate("SettingsDialogForm", "Server IP:", nullptr));
        serverIpValue->setText(QCoreApplication::translate("SettingsDialogForm", "192.168.x.xxx", nullptr));
        reconnectButton->setText(QCoreApplication::translate("SettingsDialogForm", "Reconnect", nullptr));
        saveCloseButton->setText(QCoreApplication::translate("SettingsDialogForm", "Save and Close", nullptr));
        cancelButton->setText(QCoreApplication::translate("SettingsDialogForm", "Cancel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SettingsDialogForm: public Ui_SettingsDialogForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGS_DIALOG_H
