# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'Popup_message.ui'
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
from PySide6.QtWidgets import (QAbstractButton, QApplication, QDialog, QDialogButtonBox,
    QFormLayout, QLabel, QLineEdit, QSizePolicy,
    QVBoxLayout, QWidget)

class Ui_Dialog(object):
    def setupUi(self, Dialog):
        if not Dialog.objectName():
            Dialog.setObjectName(u"Dialog")
        Dialog.resize(420, 200)
        self.verticalLayout = QVBoxLayout(Dialog)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.formLayout = QFormLayout()
        self.formLayout.setObjectName(u"formLayout")
        self.nameLabel = QLabel(Dialog)
        self.nameLabel.setObjectName(u"nameLabel")

        self.formLayout.setWidget(0, QFormLayout.LabelRole, self.nameLabel)

        self.nameEdit = QLineEdit(Dialog)
        self.nameEdit.setObjectName(u"nameEdit")

        self.formLayout.setWidget(0, QFormLayout.FieldRole, self.nameEdit)

        self.roomLabel = QLabel(Dialog)
        self.roomLabel.setObjectName(u"roomLabel")

        self.formLayout.setWidget(1, QFormLayout.LabelRole, self.roomLabel)

        self.manualIpEdit = QLineEdit(Dialog)
        self.manualIpEdit.setObjectName(u"manualIpEdit")

        self.formLayout.setWidget(1, QFormLayout.FieldRole, self.manualIpEdit)

        self.localIpTextLabel = QLabel(Dialog)
        self.localIpTextLabel.setObjectName(u"localIpTextLabel")

        self.formLayout.setWidget(2, QFormLayout.LabelRole, self.localIpTextLabel)

        self.localIpLabel = QLabel(Dialog)
        self.localIpLabel.setObjectName(u"localIpLabel")

        self.formLayout.setWidget(2, QFormLayout.FieldRole, self.localIpLabel)


        self.verticalLayout.addLayout(self.formLayout)

        self.buttonBox = QDialogButtonBox(Dialog)
        self.buttonBox.setObjectName(u"buttonBox")
        self.buttonBox.setOrientation(Qt.Horizontal)
        self.buttonBox.setStandardButtons(QDialogButtonBox.Cancel|QDialogButtonBox.Ok)

        self.verticalLayout.addWidget(self.buttonBox)


        self.retranslateUi(Dialog)
        self.buttonBox.accepted.connect(Dialog.accept)
        self.buttonBox.rejected.connect(Dialog.reject)

        QMetaObject.connectSlotsByName(Dialog)
    # setupUi

    def retranslateUi(self, Dialog):
        Dialog.setWindowTitle(QCoreApplication.translate("Dialog", u"Join Room", None))
        self.nameLabel.setText(QCoreApplication.translate("Dialog", u"Your name:", None))
        self.nameEdit.setPlaceholderText(QCoreApplication.translate("Dialog", u"e.g. Alice", None))
        self.roomLabel.setText(QCoreApplication.translate("Dialog", u"Default room:", None))
        self.manualIpEdit.setText(QCoreApplication.translate("Dialog", u"main", None))
        self.localIpTextLabel.setText(QCoreApplication.translate("Dialog", u"Local IP:", None))
        self.localIpLabel.setText(QCoreApplication.translate("Dialog", u"0.0.0.0", None))
    # retranslateUi

