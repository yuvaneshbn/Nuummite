# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'volume_control.ui'
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
from PySide6.QtWidgets import (QApplication, QCheckBox, QFrame, QHBoxLayout,
    QLabel, QProgressBar, QPushButton, QSizePolicy,
    QSlider, QVBoxLayout, QWidget)

class Ui_VolumeControlForm(object):
    def setupUi(self, VolumeControlForm):
        if not VolumeControlForm.objectName():
            VolumeControlForm.setObjectName(u"VolumeControlForm")
        VolumeControlForm.resize(425, 509)
        self.rootLayout = QVBoxLayout(VolumeControlForm)
        self.rootLayout.setSpacing(12)
        self.rootLayout.setObjectName(u"rootLayout")
        self.rootLayout.setContentsMargins(12, 12, 12, 12)
        self.headerLabel = QLabel(VolumeControlForm)
        self.headerLabel.setObjectName(u"headerLabel")

        self.rootLayout.addWidget(self.headerLabel)

        self.line = QFrame(VolumeControlForm)
        self.line.setObjectName(u"line")
        self.line.setFrameShadow(QFrame.Plain)
        self.line.setFrameShape(QFrame.HLine)

        self.rootLayout.addWidget(self.line)

        self.masterLabel = QLabel(VolumeControlForm)
        self.masterLabel.setObjectName(u"masterLabel")

        self.rootLayout.addWidget(self.masterLabel)

        self.masterSlider = QSlider(VolumeControlForm)
        self.masterSlider.setObjectName(u"masterSlider")
        self.masterSlider.setMinimum(0)
        self.masterSlider.setMaximum(250)
        self.masterSlider.setValue(100)
        self.masterSlider.setOrientation(Qt.Horizontal)

        self.rootLayout.addWidget(self.masterSlider)

        self.outputLabel = QLabel(VolumeControlForm)
        self.outputLabel.setObjectName(u"outputLabel")

        self.rootLayout.addWidget(self.outputLabel)

        self.outputSlider = QSlider(VolumeControlForm)
        self.outputSlider.setObjectName(u"outputSlider")
        self.outputSlider.setMinimum(0)
        self.outputSlider.setMaximum(250)
        self.outputSlider.setValue(100)
        self.outputSlider.setOrientation(Qt.Horizontal)

        self.rootLayout.addWidget(self.outputSlider)

        self.gainLabel = QLabel(VolumeControlForm)
        self.gainLabel.setObjectName(u"gainLabel")

        self.rootLayout.addWidget(self.gainLabel)

        self.gainSlider = QSlider(VolumeControlForm)
        self.gainSlider.setObjectName(u"gainSlider")
        self.gainSlider.setMinimum(-20)
        self.gainSlider.setMaximum(20)
        self.gainSlider.setValue(0)
        self.gainSlider.setOrientation(Qt.Horizontal)

        self.rootLayout.addWidget(self.gainSlider)

        self.inputSensitivityLabel = QLabel(VolumeControlForm)
        self.inputSensitivityLabel.setObjectName(u"inputSensitivityLabel")

        self.rootLayout.addWidget(self.inputSensitivityLabel)

        self.inputSensitivitySlider = QSlider(VolumeControlForm)
        self.inputSensitivitySlider.setObjectName(u"inputSensitivitySlider")
        self.inputSensitivitySlider.setMinimum(0)
        self.inputSensitivitySlider.setMaximum(100)
        self.inputSensitivitySlider.setValue(45)
        self.inputSensitivitySlider.setOrientation(Qt.Horizontal)

        self.rootLayout.addWidget(self.inputSensitivitySlider)

        self.noiseSuppressionLabel = QLabel(VolumeControlForm)
        self.noiseSuppressionLabel.setObjectName(u"noiseSuppressionLabel")

        self.rootLayout.addWidget(self.noiseSuppressionLabel)

        self.noiseSuppressionSlider = QSlider(VolumeControlForm)
        self.noiseSuppressionSlider.setObjectName(u"noiseSuppressionSlider")
        self.noiseSuppressionSlider.setMinimum(0)
        self.noiseSuppressionSlider.setMaximum(100)
        self.noiseSuppressionSlider.setValue(65)
        self.noiseSuppressionSlider.setOrientation(Qt.Horizontal)

        self.rootLayout.addWidget(self.noiseSuppressionSlider)

        self.line_2 = QFrame(VolumeControlForm)
        self.line_2.setObjectName(u"line_2")
        self.line_2.setFrameShadow(QFrame.Plain)
        self.line_2.setFrameShape(QFrame.HLine)

        self.rootLayout.addWidget(self.line_2)

        self.horizontalLayout = QHBoxLayout()
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.autoGainCheckbox = QCheckBox(VolumeControlForm)
        self.autoGainCheckbox.setObjectName(u"autoGainCheckbox")

        self.horizontalLayout.addWidget(self.autoGainCheckbox)

        self.noiseSuppCheckbox = QCheckBox(VolumeControlForm)
        self.noiseSuppCheckbox.setObjectName(u"noiseSuppCheckbox")

        self.horizontalLayout.addWidget(self.noiseSuppCheckbox)

        self.echoCheckbox = QCheckBox(VolumeControlForm)
        self.echoCheckbox.setObjectName(u"echoCheckbox")

        self.horizontalLayout.addWidget(self.echoCheckbox)


        self.rootLayout.addLayout(self.horizontalLayout)

        self.aecDelayLabel = QLabel(VolumeControlForm)
        self.aecDelayLabel.setObjectName(u"aecDelayLabel")

        self.rootLayout.addWidget(self.aecDelayLabel)

        self.aecDelaySlider = QSlider(VolumeControlForm)
        self.aecDelaySlider.setObjectName(u"aecDelaySlider")
        self.aecDelaySlider.setMinimum(0)
        self.aecDelaySlider.setMaximum(500)
        self.aecDelaySlider.setValue(180)
        self.aecDelaySlider.setOrientation(Qt.Horizontal)

        self.rootLayout.addWidget(self.aecDelaySlider)

        self.testMicLayout = QHBoxLayout()
        self.testMicLayout.setObjectName(u"testMicLayout")
        self.testMicButton = QPushButton(VolumeControlForm)
        self.testMicButton.setObjectName(u"testMicButton")

        self.testMicLayout.addWidget(self.testMicButton)

        self.testStatusLabel = QLabel(VolumeControlForm)
        self.testStatusLabel.setObjectName(u"testStatusLabel")
        self.testStatusLabel.setMinimumSize(QSize(100, 0))

        self.testMicLayout.addWidget(self.testStatusLabel)


        self.rootLayout.addLayout(self.testMicLayout)

        self.micLevelBar = QProgressBar(VolumeControlForm)
        self.micLevelBar.setObjectName(u"micLevelBar")
        self.micLevelBar.setTextVisible(False)

        self.rootLayout.addWidget(self.micLevelBar)

        self.restoreDefaultsButton = QPushButton(VolumeControlForm)
        self.restoreDefaultsButton.setObjectName(u"restoreDefaultsButton")

        self.rootLayout.addWidget(self.restoreDefaultsButton)


        self.retranslateUi(VolumeControlForm)

        QMetaObject.connectSlotsByName(VolumeControlForm)
    # setupUi

    def retranslateUi(self, VolumeControlForm):
        self.headerLabel.setText(QCoreApplication.translate("VolumeControlForm", u"Audio Controls", None))
        self.masterLabel.setText(QCoreApplication.translate("VolumeControlForm", u"Master Volume", None))
        self.outputLabel.setText(QCoreApplication.translate("VolumeControlForm", u"Output Volume", None))
        self.gainLabel.setText(QCoreApplication.translate("VolumeControlForm", u"Gain (dB)", None))
        self.inputSensitivityLabel.setText(QCoreApplication.translate("VolumeControlForm", u"Input Sensitivity", None))
        self.noiseSuppressionLabel.setText(QCoreApplication.translate("VolumeControlForm", u"Noise Suppression", None))
        self.autoGainCheckbox.setText(QCoreApplication.translate("VolumeControlForm", u"Auto-Gain", None))
        self.noiseSuppCheckbox.setText(QCoreApplication.translate("VolumeControlForm", u"Noise Suppression", None))
        self.echoCheckbox.setText(QCoreApplication.translate("VolumeControlForm", u"Echo Cancellation", None))
        self.aecDelayLabel.setText(QCoreApplication.translate("VolumeControlForm", u"AEC Delay (ms)", None))
        self.testMicButton.setText(QCoreApplication.translate("VolumeControlForm", u"Test Mic", None))
        self.testStatusLabel.setText("")
        self.restoreDefaultsButton.setText(QCoreApplication.translate("VolumeControlForm", u"Restore Default", None))
        pass
    # retranslateUi

