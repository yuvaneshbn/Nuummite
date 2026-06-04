#ifndef NUUMMITE_ENCRYPTION_DIALOG_H
#define NUUMMITE_ENCRYPTION_DIALOG_H

#include <QDialog>
#include <memory>

namespace Ui {
class EncryptionDialogForm;
}

class EncryptionDialog final : public QDialog {
    Q_OBJECT

public:
    explicit EncryptionDialog(QWidget* parent = nullptr);
    ~EncryptionDialog() override;

    QString passphrase() const;

private:
    std::unique_ptr<Ui::EncryptionDialogForm> ui_;
};

#endif // NUUMMITE_ENCRYPTION_DIALOG_H