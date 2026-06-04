#include "EncryptionDialog.h"
#include "ui_encryption_dialog.h"

EncryptionDialog::EncryptionDialog(QWidget* parent)
    : QDialog(parent),
      ui_(std::make_unique<Ui::EncryptionDialogForm>()) {
    ui_->setupUi(this);
}

EncryptionDialog::~EncryptionDialog() = default;

QString EncryptionDialog::passphrase() const {
    return ui_->passphraseEdit->text().trimmed();
}