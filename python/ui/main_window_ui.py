# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'main_window.ui'
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
from PySide6.QtWidgets import (QApplication, QCheckBox, QComboBox, QFrame,
    QGroupBox, QHBoxLayout, QLabel, QLineEdit,
    QListWidget, QListWidgetItem, QProgressBar, QPushButton,
    QScrollArea, QSizePolicy, QSpacerItem, QStatusBar,
    QVBoxLayout, QWidget)

from python.ui.volume_control_widget import VolumeControlWidget

class Ui_MainWindowForm(object):
    def setupUi(self, MainWindowForm):
        if not MainWindowForm.objectName():
            MainWindowForm.setObjectName(u"MainWindowForm")
        MainWindowForm.resize(810, 481)
        self.rootLayout = QVBoxLayout(MainWindowForm)
        self.rootLayout.setObjectName(u"rootLayout")
        self.topToolbarLayout = QHBoxLayout()
        self.topToolbarLayout.setObjectName(u"topToolbarLayout")
        self.roomLabel = QLabel(MainWindowForm)
        self.roomLabel.setObjectName(u"roomLabel")

        self.topToolbarLayout.addWidget(self.roomLabel)

        self.roomCombo = QComboBox(MainWindowForm)
        self.roomCombo.setObjectName(u"roomCombo")

        self.topToolbarLayout.addWidget(self.roomCombo)

        self.joinLeaveButton = QPushButton(MainWindowForm)
        self.joinLeaveButton.setObjectName(u"joinLeaveButton")

        self.topToolbarLayout.addWidget(self.joinLeaveButton)

        self.refreshButton = QPushButton(MainWindowForm)
        self.refreshButton.setObjectName(u"refreshButton")

        self.topToolbarLayout.addWidget(self.refreshButton)

        self.themeSwitch = QCheckBox(MainWindowForm)
        self.themeSwitch.setObjectName(u"themeSwitch")
        self.themeSwitch.setChecked(True)

        self.topToolbarLayout.addWidget(self.themeSwitch)

        self.topSpacer = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.topToolbarLayout.addItem(self.topSpacer)

        self.connectionIndicator = QLabel(MainWindowForm)
        self.connectionIndicator.setObjectName(u"connectionIndicator")

        self.topToolbarLayout.addWidget(self.connectionIndicator)


        self.rootLayout.addLayout(self.topToolbarLayout)

        self.contentLayout = QHBoxLayout()
        self.contentLayout.setObjectName(u"contentLayout")
        self.participantsGroup = QGroupBox(MainWindowForm)
        self.participantsGroup.setObjectName(u"participantsGroup")
        self.participantsLayout = QVBoxLayout(self.participantsGroup)
        self.participantsLayout.setObjectName(u"participantsLayout")
        self.searchInput = QLineEdit(self.participantsGroup)
        self.searchInput.setObjectName(u"searchInput")

        self.participantsLayout.addWidget(self.searchInput)

        self.participantList = QListWidget(self.participantsGroup)
        self.participantList.setObjectName(u"participantList")

        self.participantsLayout.addWidget(self.participantList)

        self.countLabel = QLabel(self.participantsGroup)
        self.countLabel.setObjectName(u"countLabel")

        self.participantsLayout.addWidget(self.countLabel)


        self.contentLayout.addWidget(self.participantsGroup)

        self.activeSpeakersGroup = QGroupBox(MainWindowForm)
        self.activeSpeakersGroup.setObjectName(u"activeSpeakersGroup")
        self.activeLayout = QVBoxLayout(self.activeSpeakersGroup)
        self.activeLayout.setObjectName(u"activeLayout")
        self.titleLabel = QLabel(self.activeSpeakersGroup)
        self.titleLabel.setObjectName(u"titleLabel")

        self.activeLayout.addWidget(self.titleLabel)

        self.activeSpeakersLabel = QLabel(self.activeSpeakersGroup)
        self.activeSpeakersLabel.setObjectName(u"activeSpeakersLabel")

        self.activeLayout.addWidget(self.activeSpeakersLabel)

        self.speakerLogList = QListWidget(self.activeSpeakersGroup)
        self.speakerLogList.setObjectName(u"speakerLogList")

        self.activeLayout.addWidget(self.speakerLogList)

        self.systemAudioLabel = QLabel(self.activeSpeakersGroup)
        self.systemAudioLabel.setObjectName(u"systemAudioLabel")

        self.activeLayout.addWidget(self.systemAudioLabel)

        self.systemLevelBar = QProgressBar(self.activeSpeakersGroup)
        self.systemLevelBar.setObjectName(u"systemLevelBar")
        self.systemLevelBar.setTextVisible(False)

        self.activeLayout.addWidget(self.systemLevelBar)


        self.contentLayout.addWidget(self.activeSpeakersGroup)

        self.myControlsGroup = QGroupBox(MainWindowForm)
        self.myControlsGroup.setObjectName(u"myControlsGroup")
        self.myControlsLayout = QVBoxLayout(self.myControlsGroup)
        self.myControlsLayout.setObjectName(u"myControlsLayout")
        self.controlsScrollArea = QScrollArea(self.myControlsGroup)
        self.controlsScrollArea.setObjectName(u"controlsScrollArea")
        self.controlsScrollArea.setFrameShape(QFrame.NoFrame)
        self.controlsScrollArea.setWidgetResizable(True)
        self.controlsScrollContent = QWidget()
        self.controlsScrollContent.setObjectName(u"controlsScrollContent")
        self.controlsPlaceholderLayout = QVBoxLayout(self.controlsScrollContent)
        self.controlsPlaceholderLayout.setObjectName(u"controlsPlaceholderLayout")
        self.advancedAudioControl = VolumeControlWidget(self.controlsScrollContent)
        self.advancedAudioControl.setObjectName(u"advancedAudioControl")

        self.controlsPlaceholderLayout.addWidget(self.advancedAudioControl)

        self.controlsScrollArea.setWidget(self.controlsScrollContent)

        self.myControlsLayout.addWidget(self.controlsScrollArea)


        self.contentLayout.addWidget(self.myControlsGroup)


        self.rootLayout.addLayout(self.contentLayout)

        self.bottomToolbarLayout = QHBoxLayout()
        self.bottomToolbarLayout.setObjectName(u"bottomToolbarLayout")
        self.bottomSpacer = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.bottomToolbarLayout.addItem(self.bottomSpacer)

        self.muteButton = QPushButton(MainWindowForm)
        self.muteButton.setObjectName(u"muteButton")

        self.bottomToolbarLayout.addWidget(self.muteButton)

        self.broadcastButton = QPushButton(MainWindowForm)
        self.broadcastButton.setObjectName(u"broadcastButton")

        self.bottomToolbarLayout.addWidget(self.broadcastButton)

        self.settingsButton = QPushButton(MainWindowForm)
        self.settingsButton.setObjectName(u"settingsButton")

        self.bottomToolbarLayout.addWidget(self.settingsButton)


        self.rootLayout.addLayout(self.bottomToolbarLayout)

        self.warningLabel = QLabel(MainWindowForm)
        self.warningLabel.setObjectName(u"warningLabel")

        self.rootLayout.addWidget(self.warningLabel)

        self.mainStatusBar = QStatusBar(MainWindowForm)
        self.mainStatusBar.setObjectName(u"mainStatusBar")

        self.rootLayout.addWidget(self.mainStatusBar)


        self.retranslateUi(MainWindowForm)

        QMetaObject.connectSlotsByName(MainWindowForm)
    # setupUi

    def retranslateUi(self, MainWindowForm):
        MainWindowForm.setWindowTitle(QCoreApplication.translate("MainWindowForm", u"Nuummite - Voice Chat Client", None))
        self.roomLabel.setText(QCoreApplication.translate("MainWindowForm", u"Room:", None))
        self.joinLeaveButton.setText(QCoreApplication.translate("MainWindowForm", u"Leave Room", None))
        self.refreshButton.setText(QCoreApplication.translate("MainWindowForm", u"Refresh List", None))
        self.themeSwitch.setText(QCoreApplication.translate("MainWindowForm", u"Dark", None))
#if QT_CONFIG(tooltip)
        self.themeSwitch.setToolTip(QCoreApplication.translate("MainWindowForm", u"Toggle light/dark theme", None))
#endif // QT_CONFIG(tooltip)
        self.connectionIndicator.setText(QCoreApplication.translate("MainWindowForm", u"Connected", None))
        self.participantsGroup.setTitle(QCoreApplication.translate("MainWindowForm", u"Participants", None))
        self.searchInput.setPlaceholderText(QCoreApplication.translate("MainWindowForm", u"Search by name/ID...", None))
        self.countLabel.setText(QCoreApplication.translate("MainWindowForm", u"0 / 0 shown", None))
        self.activeSpeakersGroup.setTitle(QCoreApplication.translate("MainWindowForm", u"Active Speakers", None))
        self.titleLabel.setText(QCoreApplication.translate("MainWindowForm", u"Nuummite - Voice Connected", None))
        self.activeSpeakersLabel.setText(QCoreApplication.translate("MainWindowForm", u"----------------", None))
        self.systemAudioLabel.setText(QCoreApplication.translate("MainWindowForm", u"System Audio Level", None))
        self.myControlsGroup.setTitle(QCoreApplication.translate("MainWindowForm", u"My Controls", None))
        self.muteButton.setText(QCoreApplication.translate("MainWindowForm", u"Mute Mic", None))
        self.broadcastButton.setText(QCoreApplication.translate("MainWindowForm", u"Broadcast", None))
        self.settingsButton.setText(QCoreApplication.translate("MainWindowForm", u"Settings", None))
        self.warningLabel.setText("")
    # retranslateUi

