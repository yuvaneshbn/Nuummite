# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'settings_dialog.ui'
##
## Created by: Qt User Interface Compiler version 6.6.3
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QApplication, QComboBox, QFormLayout, QGroupBox,
    QHBoxLayout, QLabel, QPushButton, QSizePolicy,
    QSpacerItem, QVBoxLayout, QWidget)

from python.ui.volume_control_widget import VolumeControlWidget

class Ui_SettingsDialogForm(object):
    def setupUi(self, SettingsDialogForm):
        if not SettingsDialogForm.objectName():
            SettingsDialogForm.setObjectName(u"SettingsDialogForm")
        SettingsDialogForm.resize(480, 640)
        self.rootLayout = QVBoxLayout(SettingsDialogForm)
        self.rootLayout.setSpacing(10)
        self.rootLayout.setObjectName(u"rootLayout")
        self.rootLayout.setContentsMargins(12, 12, 12, 12)
        self.headerLabel = QLabel(SettingsDialogForm)
        self.headerLabel.setObjectName(u"headerLabel")

        self.rootLayout.addWidget(self.headerLabel)

        self.audioDevicesGroup = QGroupBox(SettingsDialogForm)
        self.audioDevicesGroup.setObjectName(u"audioDevicesGroup")
        self.audioDevicesLayout = QFormLayout(self.audioDevicesGroup)
        self.audioDevicesLayout.setObjectName(u"audioDevicesLayout")
        self.audioDevicesLayout.setHorizontalSpacing(8)
        self.audioDevicesLayout.setVerticalSpacing(6)
        self.audioDevicesLayout.setContentsMargins(10, 8, 10, 10)
        self.inputDeviceLabel = QLabel(self.audioDevicesGroup)
        self.inputDeviceLabel.setObjectName(u"inputDeviceLabel")

        self.audioDevicesLayout.setWidget(0, QFormLayout.LabelRole, self.inputDeviceLabel)

        self.inputDeviceCombo = QComboBox(self.audioDevicesGroup)
        self.inputDeviceCombo.setObjectName(u"inputDeviceCombo")

        self.audioDevicesLayout.setWidget(0, QFormLayout.FieldRole, self.inputDeviceCombo)

        self.outputDeviceLabel = QLabel(self.audioDevicesGroup)
        self.outputDeviceLabel.setObjectName(u"outputDeviceLabel")

        self.audioDevicesLayout.setWidget(1, QFormLayout.LabelRole, self.outputDeviceLabel)

        self.outputDeviceCombo = QComboBox(self.audioDevicesGroup)
        self.outputDeviceCombo.setObjectName(u"outputDeviceCombo")

        self.audioDevicesLayout.setWidget(1, QFormLayout.FieldRole, self.outputDeviceCombo)


        self.rootLayout.addWidget(self.audioDevicesGroup)

        self.advancedAudioGroup = QGroupBox(SettingsDialogForm)
        self.advancedAudioGroup.setObjectName(u"advancedAudioGroup")
        self.advancedAudioLayout = QVBoxLayout(self.advancedAudioGroup)
        self.advancedAudioLayout.setSpacing(8)
        self.advancedAudioLayout.setObjectName(u"advancedAudioLayout")
        self.advancedAudioLayout.setContentsMargins(10, 8, 10, 10)
        self.advancedAudioControl = VolumeControlWidget(self.advancedAudioGroup)
        self.advancedAudioControl.setObjectName(u"advancedAudioControl")

        self.advancedAudioLayout.addWidget(self.advancedAudioControl)


        self.rootLayout.addWidget(self.advancedAudioGroup)

        self.networkGroup = QGroupBox(SettingsDialogForm)
        self.networkGroup.setObjectName(u"networkGroup")
        self.networkLayout = QFormLayout(self.networkGroup)
        self.networkLayout.setObjectName(u"networkLayout")
        self.networkLayout.setHorizontalSpacing(8)
        self.networkLayout.setVerticalSpacing(6)
        self.networkLayout.setContentsMargins(10, 8, 10, 10)
        self.serverIpTitle = QLabel(self.networkGroup)
        self.serverIpTitle.setObjectName(u"serverIpTitle")

        self.networkLayout.setWidget(0, QFormLayout.LabelRole, self.serverIpTitle)

        self.serverRow = QHBoxLayout()
        self.serverRow.setObjectName(u"serverRow")
        self.serverIpValue = QLabel(self.networkGroup)
        self.serverIpValue.setObjectName(u"serverIpValue")

        self.serverRow.addWidget(self.serverIpValue)

        self.reconnectButton = QPushButton(self.networkGroup)
        self.reconnectButton.setObjectName(u"reconnectButton")

        self.serverRow.addWidget(self.reconnectButton)


        self.networkLayout.setLayout(0, QFormLayout.FieldRole, self.serverRow)


        self.rootLayout.addWidget(self.networkGroup)

        self.footerButtonsLayout = QHBoxLayout()
        self.footerButtonsLayout.setObjectName(u"footerButtonsLayout")
        self.footerSpacer = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.footerButtonsLayout.addItem(self.footerSpacer)

        self.saveCloseButton = QPushButton(SettingsDialogForm)
        self.saveCloseButton.setObjectName(u"saveCloseButton")

        self.footerButtonsLayout.addWidget(self.saveCloseButton)

        self.cancelButton = QPushButton(SettingsDialogForm)
        self.cancelButton.setObjectName(u"cancelButton")

        self.footerButtonsLayout.addWidget(self.cancelButton)


        self.rootLayout.addLayout(self.footerButtonsLayout)


        self.retranslateUi(SettingsDialogForm)

        QMetaObject.connectSlotsByName(SettingsDialogForm)
    # setupUi

    def retranslateUi(self, SettingsDialogForm):
        SettingsDialogForm.setWindowTitle(QCoreApplication.translate("SettingsDialogForm", u"Settings - Nuummite", None))
        self.headerLabel.setText(QCoreApplication.translate("SettingsDialogForm", u"Settings", None))
        self.audioDevicesGroup.setTitle(QCoreApplication.translate("SettingsDialogForm", u"Audio Devices", None))
        self.inputDeviceLabel.setText(QCoreApplication.translate("SettingsDialogForm", u"Input Device:", None))
        self.outputDeviceLabel.setText(QCoreApplication.translate("SettingsDialogForm", u"Output Device:", None))
        self.advancedAudioGroup.setTitle(QCoreApplication.translate("SettingsDialogForm", u"Advanced Audio", None))
        self.networkGroup.setTitle(QCoreApplication.translate("SettingsDialogForm", u"Network", None))
        self.serverIpTitle.setText(QCoreApplication.translate("SettingsDialogForm", u"Server IP:", None))
        self.serverIpValue.setText(QCoreApplication.translate("SettingsDialogForm", u"192.168.x.xxx", None))
        self.reconnectButton.setText(QCoreApplication.translate("SettingsDialogForm", u"Reconnect", None))
        self.saveCloseButton.setText(QCoreApplication.translate("SettingsDialogForm", u"Save and Close", None))
        self.cancelButton.setText(QCoreApplication.translate("SettingsDialogForm", u"Cancel", None))
    # retranslateUi

