/********************************************************************************
** Form generated from reading UI file 'encryption_dialog.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ENCRYPTION_DIALOG_H
#define UI_ENCRYPTION_DIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_EncryptionDialogForm
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *headerLabel;
    QLabel *promptLabel;
    QLineEdit *passphraseEdit;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *EncryptionDialogForm)
    {
        if (EncryptionDialogForm->objectName().isEmpty())
            EncryptionDialogForm->setObjectName("EncryptionDialogForm");
        EncryptionDialogForm->resize(450, 160);
        verticalLayout = new QVBoxLayout(EncryptionDialogForm);
        verticalLayout->setObjectName("verticalLayout");
        headerLabel = new QLabel(EncryptionDialogForm);
        headerLabel->setObjectName("headerLabel");
        QFont font;
        font.setPointSize(10);
        font.setBold(true);
        headerLabel->setFont(font);

        verticalLayout->addWidget(headerLabel);

        promptLabel = new QLabel(EncryptionDialogForm);
        promptLabel->setObjectName("promptLabel");
        promptLabel->setWordWrap(true);

        verticalLayout->addWidget(promptLabel);

        passphraseEdit = new QLineEdit(EncryptionDialogForm);
        passphraseEdit->setObjectName("passphraseEdit");
        passphraseEdit->setEchoMode(QLineEdit::Password);
        passphraseEdit->setClearButtonEnabled(true);

        verticalLayout->addWidget(passphraseEdit);

        buttonBox = new QDialogButtonBox(EncryptionDialogForm);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(EncryptionDialogForm);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, EncryptionDialogForm, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, EncryptionDialogForm, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(EncryptionDialogForm);
    } // setupUi

    void retranslateUi(QDialog *EncryptionDialogForm)
    {
        EncryptionDialogForm->setWindowTitle(QCoreApplication::translate("EncryptionDialogForm", "Room Private Key Verification", nullptr));
        headerLabel->setText(QCoreApplication::translate("EncryptionDialogForm", "Enter Private Room Passphrase", nullptr));
        promptLabel->setText(QCoreApplication::translate("EncryptionDialogForm", "The passphrase is used locally to derive your key and is not transmitted.", nullptr));
        passphraseEdit->setPlaceholderText(QCoreApplication::translate("EncryptionDialogForm", "Enter passphrase...", nullptr));
    } // retranslateUi

};

namespace Ui {
    class EncryptionDialogForm: public Ui_EncryptionDialogForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ENCRYPTION_DIALOG_H
