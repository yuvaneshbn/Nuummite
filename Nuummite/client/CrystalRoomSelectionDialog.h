#ifndef CRYSTAL_ROOM_SELECTION_DIALOG_H
#define CRYSTAL_ROOM_SELECTION_DIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class LoadingSpinner;
class PeerDiscovery;

/**
 * @brief Room join/create dialog shown at application startup.
 *
 * Presents a standard Qt dialog (no frameless window or SVG chrome) that lets
 * the user enter their display name, choose or create a room, and supply the
 * room passphrase.  Peer discovery is run in the background so available rooms
 * are populated into the combo box automatically.
 */
class CrystalRoomSelectionDialog final : public QDialog {
    Q_OBJECT

public:
    explicit CrystalRoomSelectionDialog(QWidget* parent = nullptr);
    ~CrystalRoomSelectionDialog() override;

    /** Returns the trimmed display-name entered by the user. */
    QString usernameValue() const;
    /** Returns the trimmed target room name (join or create mode). */
    QString targetRoomValue() const;
    /** Returns the trimmed passphrase entered by the user. */
    QString roomPassphraseValue() const;
    /** Returns true when the user has chosen to create a new room. */
    bool createRoomChecked() const;

private slots:
    void handleModeToggle(bool isCreateMode);
    void handleRoomScan();
    void handleConnectAttempt();

private:
    void buildLayout();
    void initializeStyles();

    // Form fields
    QLineEdit*    userField_         = nullptr;
    QCheckBox*    modeToggleCheckbox_= nullptr;

    // Create-mode widgets
    QWidget*      createModeContainer_ = nullptr;
    QLineEdit*    roomNameField_       = nullptr;

    // Join-mode widgets
    QWidget*      joinModeContainer_ = nullptr;
    QComboBox*    roomSelectorCombo_ = nullptr;
    QPushButton*  scanRoomsButton_   = nullptr;

    // Shared
    QLineEdit*    passphraseField_   = nullptr;
    QPushButton*  proceedButton_     = nullptr;
    QLabel*       scanStatusLabel_   = nullptr;
    LoadingSpinner* transitionLoader_ = nullptr;

    PeerDiscovery* liveDiscoveryEngine_ = nullptr;
};

#endif // CRYSTAL_ROOM_SELECTION_DIALOG_H
