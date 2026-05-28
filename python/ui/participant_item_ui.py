# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'participant_item.ui'
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
from PySide6.QtWidgets import (QApplication, QCheckBox, QHBoxLayout, QLabel,
    QProgressBar, QSizePolicy, QVBoxLayout, QWidget)

class Ui_ParticipantItemForm(object):
    def setupUi(self, ParticipantItemForm):
        if not ParticipantItemForm.objectName():
            ParticipantItemForm.setObjectName(u"ParticipantItemForm")
        ParticipantItemForm.resize(266, 75)
        self.rootLayout = QVBoxLayout(ParticipantItemForm)
        self.rootLayout.setObjectName(u"rootLayout")
        self.rowLayout = QHBoxLayout()
        self.rowLayout.setObjectName(u"rowLayout")
        self.participantName = QLabel(ParticipantItemForm)
        self.participantName.setObjectName(u"participantName")

        self.rowLayout.addWidget(self.participantName)

        self.talkCheckbox = QCheckBox(ParticipantItemForm)
        self.talkCheckbox.setObjectName(u"talkCheckbox")

        self.rowLayout.addWidget(self.talkCheckbox)

        self.muteCheckbox = QCheckBox(ParticipantItemForm)
        self.muteCheckbox.setObjectName(u"muteCheckbox")

        self.rowLayout.addWidget(self.muteCheckbox)

        self.micStatusLabel = QLabel(ParticipantItemForm)
        self.micStatusLabel.setObjectName(u"micStatusLabel")

        self.rowLayout.addWidget(self.micStatusLabel)


        self.rootLayout.addLayout(self.rowLayout)

        self.participantVolumeBar = QProgressBar(ParticipantItemForm)
        self.participantVolumeBar.setObjectName(u"participantVolumeBar")
        self.participantVolumeBar.setTextVisible(False)

        self.rootLayout.addWidget(self.participantVolumeBar)


        self.retranslateUi(ParticipantItemForm)

        QMetaObject.connectSlotsByName(ParticipantItemForm)
    # setupUi

    def retranslateUi(self, ParticipantItemForm):
        self.participantName.setText(QCoreApplication.translate("ParticipantItemForm", u"Client Name", None))
        self.talkCheckbox.setText(QCoreApplication.translate("ParticipantItemForm", u"Talk", None))
        self.muteCheckbox.setText(QCoreApplication.translate("ParticipantItemForm", u"Mute", None))
        self.micStatusLabel.setText(QCoreApplication.translate("ParticipantItemForm", u"Mic: Off", None))
        pass
    # retranslateUi

