/********************************************************************************
** Form generated from reading UI file 'Popup_message.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POPUP_MESSAGE_H
#define UI_POPUP_MESSAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_Dialog
{
public:
    QVBoxLayout *verticalLayout;
    QFormLayout *formLayout;
    QLabel *nameLabel;
    QLineEdit *nameEdit;
    QLabel *roomLabel;
    QLineEdit *manualIpEdit;
    QLabel *localIpTextLabel;
    QLabel *localIpLabel;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *Dialog)
    {
        if (Dialog->objectName().isEmpty())
            Dialog->setObjectName("Dialog");
        Dialog->resize(420, 200);
        verticalLayout = new QVBoxLayout(Dialog);
        verticalLayout->setObjectName("verticalLayout");
        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        nameLabel = new QLabel(Dialog);
        nameLabel->setObjectName("nameLabel");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, nameLabel);

        nameEdit = new QLineEdit(Dialog);
        nameEdit->setObjectName("nameEdit");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, nameEdit);

        roomLabel = new QLabel(Dialog);
        roomLabel->setObjectName("roomLabel");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, roomLabel);

        manualIpEdit = new QLineEdit(Dialog);
        manualIpEdit->setObjectName("manualIpEdit");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, manualIpEdit);

        localIpTextLabel = new QLabel(Dialog);
        localIpTextLabel->setObjectName("localIpTextLabel");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, localIpTextLabel);

        localIpLabel = new QLabel(Dialog);
        localIpLabel->setObjectName("localIpLabel");

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, localIpLabel);


        verticalLayout->addLayout(formLayout);

        buttonBox = new QDialogButtonBox(Dialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(Dialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, Dialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, Dialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(Dialog);
    } // setupUi

    void retranslateUi(QDialog *Dialog)
    {
        Dialog->setWindowTitle(QCoreApplication::translate("Dialog", "Join Room", nullptr));
        nameLabel->setText(QCoreApplication::translate("Dialog", "Your name:", nullptr));
        nameEdit->setPlaceholderText(QCoreApplication::translate("Dialog", "e.g. Alice", nullptr));
        roomLabel->setText(QCoreApplication::translate("Dialog", "Default room:", nullptr));
        manualIpEdit->setText(QCoreApplication::translate("Dialog", "main", nullptr));
        localIpTextLabel->setText(QCoreApplication::translate("Dialog", "Local IP:", nullptr));
        localIpLabel->setText(QCoreApplication::translate("Dialog", "0.0.0.0", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Dialog: public Ui_Dialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POPUP_MESSAGE_H
