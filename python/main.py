import os
import sys
import shutil
import socket
import time
from pathlib import Path
from typing import Dict, List, Set

from PySide6 import QtCore, QtWidgets, QtUiTools

# Allow running as a script or module
if __package__ in (None, ""):
    sys.path.append(str(Path(__file__).resolve().parent.parent))

from python import PyAudioEngine, PyPeerDiscovery


ROOT = Path(__file__).resolve().parent.parent  # repository root
UI_DIR = ROOT / "Nuummite" / "ui"


def resource_path(rel: str) -> str:
    """
    Resolve resource paths when running from source or PyInstaller (_MEIPASS).
    """
    base = Path(getattr(sys, "_MEIPASS", ROOT))
    return str(base / rel)


def load_ui(filename: str, parent=None):
    loader = QtUiTools.QUiLoader()
    ui_file = QtCore.QFile(resource_path(f"Nuummite/ui/{filename}"))
    # PySide6 renamed open mode enum; QFile.ReadOnly is absent. Use OpenModeFlag.
    if not ui_file.open(QtCore.QFile.OpenModeFlag.ReadOnly):
        raise RuntimeError(f"Cannot open UI file: {ui_file.fileName()}")
    widget = loader.load(ui_file, parent)
    ui_file.close()
    if widget is None:
        raise RuntimeError(f"Failed to load UI from {filename}")
    return widget


def get_local_ip() -> str:
    """
    Best-effort local IPv4 detection without external traffic.
    """
    ip = "127.0.0.1"
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
    except OSError:
        pass
    return ip


class ParticipantRow(QtCore.QObject):
    talkToggled = QtCore.Signal(str, bool)
    muteToggled = QtCore.Signal(str, bool)

    def __init__(self, client_id: str, is_self: bool, talk_checked: bool, mute_checked: bool, parent=None):
        super().__init__(parent)
        self.client_id = client_id
        self.is_self = is_self
        self.widget = load_ui("participant_item.ui")

        self.name_label: QtWidgets.QLabel = self.widget.findChild(QtWidgets.QLabel, "participantName")
        self.talk_checkbox: QtWidgets.QCheckBox = self.widget.findChild(QtWidgets.QCheckBox, "talkCheckbox")
        self.mute_checkbox: QtWidgets.QCheckBox = self.widget.findChild(QtWidgets.QCheckBox, "muteCheckbox")
        self.mic_status_label: QtWidgets.QLabel = self.widget.findChild(QtWidgets.QLabel, "micStatusLabel")
        self.volume_bar: QtWidgets.QProgressBar = self.widget.findChild(QtWidgets.QProgressBar, "participantVolumeBar")

        self.name_label.setText(client_id + (" (me)" if is_self else ""))
        self.talk_checkbox.setChecked(talk_checked)
        self.mute_checkbox.setChecked(mute_checked)
        self.volume_bar.setRange(0, 100)
        self.volume_bar.setValue(0)
        if is_self:
            self.talk_checkbox.setEnabled(False)  # self always sending controlled via mute/broadcast

        self.talk_checkbox.toggled.connect(self._on_talk)
        self.mute_checkbox.toggled.connect(self._on_mute)

    def _on_talk(self, checked: bool):
        self.talkToggled.emit(self.client_id, checked)

    def _on_mute(self, checked: bool):
        self.muteToggled.emit(self.client_id, checked)

    def setTalkChecked(self, enabled: bool):
        self.talk_checkbox.blockSignals(True)
        self.talk_checkbox.setChecked(enabled)
        self.talk_checkbox.blockSignals(False)

    def setMuteChecked(self, enabled: bool):
        self.mute_checkbox.blockSignals(True)
        self.mute_checkbox.setChecked(enabled)
        self.mute_checkbox.blockSignals(False)

    def setVolume(self, value: int):
        self.volume_bar.setValue(max(0, min(100, value)))

    def setMicStatus(self, is_on: bool):
        self.mic_status_label.setText("Mic: On" if is_on else "Mic: Off")


class VolumeControlPanel(QtCore.QObject):
    def __init__(self, audio: PyAudioEngine, parent=None):
        super().__init__(parent)
        self.audio = audio
        self.widget: QtWidgets.QWidget = load_ui("volume_control.ui")

        self.master_slider = self.widget.findChild(QtWidgets.QSlider, "masterSlider")
        self.gain_slider = self.widget.findChild(QtWidgets.QSlider, "gainSlider")
        self.ns_label = self.widget.findChild(QtWidgets.QLabel, "noiseSuppressionLabel")
        self.ns_slider = self.widget.findChild(QtWidgets.QSlider, "noiseSuppressionSlider")
        self.input_label = self.widget.findChild(QtWidgets.QLabel, "inputSensitivityLabel")
        self.input_slider = self.widget.findChild(QtWidgets.QSlider, "inputSensitivitySlider")
        self.test_button = self.widget.findChild(QtWidgets.QPushButton, "testMicButton")
        self.test_status = self.widget.findChild(QtWidgets.QLabel, "testStatusLabel")
        self.mic_bar = self.widget.findChild(QtWidgets.QProgressBar, "participantVolumeBar") or \
            self.widget.findChild(QtWidgets.QProgressBar, "micLevelBar")
        self.reset_button = self.widget.findChild(QtWidgets.QPushButton, "restoreDefaultsButton")

        self.master_slider.setRange(0, 100)
        self.master_slider.setValue(100)
        self.gain_slider.setRange(-20, 20)
        self.gain_slider.setValue(0)
        self.ns_slider.setRange(0, 100)
        self.ns_slider.setValue(70)
        if self.input_slider:
            self.input_slider.setRange(0, 100)
            self.input_slider.setValue(50)

        self.master_slider.valueChanged.connect(lambda v: self.audio.set_master_volume(int(v)))
        self.gain_slider.valueChanged.connect(lambda v: self.audio.set_gain_db(int(v)))
        self.ns_slider.valueChanged.connect(lambda v: self.audio.set_noise_suppression(int(v)))
        if self.input_slider:
            self.input_slider.valueChanged.connect(lambda v: self.audio.set_mic_sensitivity(int(v)))
        self.test_button.clicked.connect(self._test_mic)
        if self.reset_button:
            self.reset_button.clicked.connect(self._reset_defaults)

    def _test_mic(self):
        level = self.audio.test_microphone_level(0.8)
        self.setMicLevel(level)
        self.test_status.setText(f"Mic level: {level}%")

    def _reset_defaults(self):
        self.master_slider.setValue(100)
        self.gain_slider.setValue(0)
        if self.input_slider:
            self.input_slider.setValue(50)
        self.ns_slider.setValue(70)
        self.setMicLevel(self.audio.capture_level)

    def setMicLevel(self, level: int):
        if self.mic_bar:
            self.mic_bar.setRange(0, 100)
            self.mic_bar.setValue(max(0, min(100, level)))


class SettingsDialog(QtWidgets.QDialog):
    def __init__(self, audio: PyAudioEngine, server_ip: str, parent=None):
        super().__init__(parent)
        self.audio = audio
        self.server_ip = server_ip
        self.form = load_ui("settings_dialog.ui", self)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.form)

        self.input_combo = self.form.findChild(QtWidgets.QComboBox, "inputDeviceCombo")
        self.output_combo = self.form.findChild(QtWidgets.QComboBox, "outputDeviceCombo")
        self.server_ip_value = self.form.findChild(QtWidgets.QLabel, "serverIpValue")
        self.reconnect_button = self.form.findChild(QtWidgets.QPushButton, "reconnectButton")
        self.save_button = self.form.findChild(QtWidgets.QPushButton, "saveCloseButton")
        self.cancel_button = self.form.findChild(QtWidgets.QPushButton, "cancelButton")
        self.advanced_layout = self.form.findChild(QtWidgets.QVBoxLayout, "advancedAudioLayout")
        self.volume_hint = self.form.findChild(QtWidgets.QLabel, "volumeControlHint")

        self.vol_panel = VolumeControlPanel(audio, self)
        if self.volume_hint:
            self.volume_hint.setParent(None)
        if self.advanced_layout:
            self.advanced_layout.addWidget(self.vol_panel.widget)

        self.server_ip_value.setText(server_ip)

        self.reconnect_button.clicked.connect(self._noop_reconnect)
        self.save_button.clicked.connect(self.accept)
        self.cancel_button.clicked.connect(self.reject)
        self.input_combo.currentIndexChanged.connect(self._input_changed)
        self.output_combo.currentIndexChanged.connect(self._output_changed)

        self._populate_devices()

    def _populate_devices(self):
        self.input_combo.blockSignals(True)
        self.output_combo.blockSignals(True)
        self.input_combo.clear()
        self.output_combo.clear()
        for entry in self.audio.list_input_devices():
            self.input_combo.addItem(entry["name"], entry["index"])
        for entry in self.audio.list_output_devices():
            self.output_combo.addItem(entry["name"], entry["index"])
        # set current selections
        idx = self.input_combo.findData(self.audio.input_device_index)
        if idx >= 0:
            self.input_combo.setCurrentIndex(idx)
        idx = self.output_combo.findData(self.audio.output_device_index)
        if idx >= 0:
            self.output_combo.setCurrentIndex(idx)
        self.input_combo.blockSignals(False)
        self.output_combo.blockSignals(False)

    def _input_changed(self, _):
        data = self.input_combo.currentData()
        if data is not None:
            self.audio.set_input_device(int(data))

    def _output_changed(self, _):
        data = self.output_combo.currentData()
        if data is not None:
            self.audio.set_output_device(int(data))

    def _noop_reconnect(self):
        QtWidgets.QMessageBox.information(self, "Reconnect", "Reconnect not required for LAN mesh.")


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, my_id: str, room_name: str, audio: PyAudioEngine, discovery: PyPeerDiscovery):
        super().__init__()
        self.my_id = my_id
        self.current_room = room_name or "main"
        self.audio = audio
        self.discovery = discovery

        self.audio.set_client_id(my_id)

        self.root = load_ui("main_window.ui")
        self.setCentralWidget(self.root)
        self.setWindowTitle(self.root.windowTitle())

        find = self.root.findChild
        self.room_combo = find(QtWidgets.QComboBox, "roomCombo")
        self.join_leave_button = find(QtWidgets.QPushButton, "joinLeaveButton")
        self.refresh_button = find(QtWidgets.QPushButton, "refreshButton")
        self.connection_indicator = find(QtWidgets.QLabel, "connectionIndicator")
        self.search_input = find(QtWidgets.QLineEdit, "searchInput")
        self.participant_list = find(QtWidgets.QListWidget, "participantList")
        self.count_label = find(QtWidgets.QLabel, "countLabel")
        self.active_speakers_label = find(QtWidgets.QLabel, "activeSpeakersLabel")
        self.speaker_log_list = find(QtWidgets.QListWidget, "speakerLogList")
        self.system_level_bar = find(QtWidgets.QProgressBar, "systemLevelBar")
        self.controls_layout = find(QtWidgets.QVBoxLayout, "controlsPlaceholderLayout")
        if self.controls_layout is None:
            group = find(QtWidgets.QWidget, "myControlsGroup")
            if group:
                self.controls_layout = group.layout()
        self.controls_hint = find(QtWidgets.QLabel, "controlsHint")
        self.mute_button = find(QtWidgets.QPushButton, "muteButton")
        self.broadcast_button = find(QtWidgets.QPushButton, "broadcastButton")
        self.settings_button = find(QtWidgets.QPushButton, "settingsButton")
        self.warning_label = find(QtWidgets.QLabel, "warningLabel")
        self.main_status_bar = find(QtWidgets.QStatusBar, "mainStatusBar")
        if self.main_status_bar is None:
            self.main_status_bar = QtWidgets.QStatusBar(self)
            self.setStatusBar(self.main_status_bar)

        self.volume_controls = VolumeControlPanel(audio, self)
        if self.controls_hint:
            self.controls_hint.setParent(None)
        if self.controls_layout:
            self.controls_layout.addWidget(self.volume_controls.widget)

        self.room_combo.clear()
        self.room_combo.addItem(self.current_room)
        self.room_combo.setEnabled(False)

        self.join_leave_button.clicked.connect(self.close)
        self.refresh_button.clicked.connect(lambda: self.refresh_participants(False))
        self.search_input.textChanged.connect(self.apply_search_filter)
        self.mute_button.setCheckable(True)
        self.mute_button.toggled.connect(self.toggle_self_mute)
        self.broadcast_button.setCheckable(True)
        self.broadcast_button.toggled.connect(self.toggle_broadcast)
        self.settings_button.clicked.connect(self.open_settings)

        self.stop_capture_timer = QtCore.QTimer(self)
        self.stop_capture_timer.setSingleShot(True)
        self.stop_capture_timer.setInterval(1200)
        self.stop_capture_timer.timeout.connect(self.stop_capture_if_idle)

        self.ui_timer = QtCore.QTimer(self)
        self.ui_timer.setInterval(200)
        self.ui_timer.timeout.connect(self.update_live_ui)
        self.ui_timer.start()

        self.auto_refresh_timer = QtCore.QTimer(self)
        self.auto_refresh_timer.setInterval(1500)
        self.auto_refresh_timer.timeout.connect(lambda: self.refresh_participants(True))
        self.auto_refresh_timer.start()

        self.system_level_bar.setRange(0, 100)
        self.system_level_bar.setValue(0)

        self.main_status_bar.showMessage(f"Client {my_id} in room '{self.current_room}' (P2P mesh)")
        self.connected = True
        self.targets: Set[str] = set()
        self.muted: Set[str] = set()
        self.hear_targets: Set[str] = set()
        self.rows: Dict[str, ParticipantRow] = {}
        self.speaker_state: Dict[str, bool] = {}
        self.last_voice_ts: Dict[str, float] = {}

        self.refresh_participants(False)
        self.set_connected_state(True)

    # ----- UI wiring -----
    def set_connected_state(self, connected: bool, detail: str = ""):
        self.connected = connected
        if connected:
            self.connection_indicator.setText("Connected")
            self.connection_indicator.setStyleSheet("color:#1E8E3E; font-weight:bold;")
            if detail:
                self.main_status_bar.showMessage(detail)
            self.warning_label.setText("")
        else:
            self.connection_indicator.setText("Disconnected")
            self.connection_indicator.setStyleSheet("color:#C62828; font-weight:bold;")
            msg = detail or "No peers reachable"
            self.warning_label.setText(msg)
            self.main_status_bar.showMessage(msg)

    def refresh_participants(self, silent: bool):
        peers = self.discovery.peers()
        participants = [p["id"] for p in peers]
        if self.my_id not in participants:
            participants.append(self.my_id)
        participants = sorted(set(participants), key=lambda x: (not x.isdigit(), x))

        if not silent:
            self.set_connected_state(True, "Participant list refreshed")
        elif not self.connected:
            self.set_connected_state(True, "Connection restored")

        self.targets &= set(participants)
        self.muted &= set(participants)

        self.participant_list.clear()
        for cid in list(self.rows.values()):
            cid.widget.deleteLater()
        self.rows.clear()

        for cid in participants:
            is_self = cid == self.my_id
            row = ParticipantRow(cid, is_self, cid in self.targets, cid in self.muted, self)
            row.talkToggled.connect(self._on_talk_toggled)
            row.muteToggled.connect(self._on_mute_toggled)

            item = QtWidgets.QListWidgetItem()
            item.setSizeHint(row.widget.sizeHint())
            self.participant_list.addItem(item)
            self.participant_list.setItemWidget(item, row.widget)
            self.rows[cid] = row

        self.recompute_hear_targets()
        self.update_local_targets()
        self.apply_search_filter()

    def _on_talk_toggled(self, client_id: str, enabled: bool):
        if client_id == self.my_id:
            return
        if enabled:
            self.targets.add(client_id)
        else:
            self.targets.discard(client_id)
        self.update_local_targets()

    def _on_mute_toggled(self, client_id: str, enabled: bool):
        if client_id == self.my_id:
            return
        if enabled:
            self.muted.add(client_id)
        else:
            self.muted.discard(client_id)
        self.recompute_hear_targets()
        self.update_local_targets()

    def apply_search_filter(self):
        query = self.search_input.text().strip().lower()
        shown = 0
        total = self.participant_list.count()
        for i in range(total):
            item = self.participant_list.item(i)
            widget = self.participant_list.itemWidget(item)
            name_label = widget.findChild(QtWidgets.QLabel, "participantName")
            text = name_label.text().lower() if name_label else ""
            visible = (not query) or (query in text)
            item.setHidden(not visible)
            if visible:
                shown += 1
        self.count_label.setText(f"{shown} / {total} shown")

    def recompute_hear_targets(self):
        self.hear_targets = {cid for cid in self.rows if cid != self.my_id and cid not in self.muted}

    def update_local_targets(self):
        dest_ips: List[str] = []
        peers = {p["id"]: p for p in self.discovery.peers()}
        for cid in self.targets:
            peer = peers.get(cid)
            if peer:
                ip = peer["ip"]
                port = peer.get("port", self.audio.port())
                dest_ips.append(f"{ip}:{port}")
        if dest_ips:
            if self.stop_capture_timer.isActive():
                self.stop_capture_timer.stop()
            if not self.audio.is_running:
                self.audio.start(dest_ips)
            else:
                self.audio.update_destinations(dest_ips)
        elif self.audio.is_running and not self.stop_capture_timer.isActive():
            self.stop_capture_timer.start()

        self.audio.set_hear_targets(self.hear_targets)
        self.sync_broadcast_button()

    def stop_capture_if_idle(self):
        if not self.targets and self.audio.is_running:
            self.audio.stop()

    def sync_broadcast_button(self):
        all_targets = {cid for cid in self.rows if cid != self.my_id}
        is_broadcast = bool(all_targets) and self.targets == all_targets
        self.broadcast_button.blockSignals(True)
        self.broadcast_button.setChecked(is_broadcast)
        self.broadcast_button.setText("Broadcast On" if is_broadcast else "Broadcast Off")
        self.broadcast_button.blockSignals(False)

    def toggle_broadcast(self, enabled: bool):
        all_targets = {cid for cid in self.rows if cid != self.my_id}
        if enabled and not all_targets:
            self.broadcast_button.blockSignals(True)
            self.broadcast_button.setChecked(False)
            self.broadcast_button.setText("Broadcast Off")
            self.broadcast_button.blockSignals(False)
            return
        self.targets = set(all_targets) if enabled else set()
        for cid, row in self.rows.items():
            if cid == self.my_id:
                continue
            row.setTalkChecked(cid in self.targets)
        self.update_local_targets()

    def toggle_self_mute(self, muted: bool):
        self.audio.set_tx_muted(muted)
        self.mute_button.setText("Unmute Mic" if muted else "Mute Mic")
        self.main_status_bar.showMessage("Microphone muted" if muted else "Microphone unmuted")
        row = self.rows.get(self.my_id)
        if row:
            row.setMicStatus(not muted)

    def open_settings(self):
        dlg = SettingsDialog(self.audio, "P2P Mesh", self)
        dlg.exec()

    def update_live_ui(self):
        mic_level = self.audio.capture_level
        self.system_level_bar.setValue(mic_level)
        self.volume_controls.setMicLevel(mic_level)

        speaking_state: Dict[str, bool] = {}
        self_state = self.audio.capture_active and not self.audio.tx_muted
        row = self.rows.get(self.my_id)
        if row:
            row.setVolume(mic_level)
            row.setMicStatus(not self.audio.tx_muted)
        speaking_state[self.my_id] = self_state
        self.last_voice_ts[self.my_id] = time.monotonic()

        raw_level = self.audio.mixed_peak
        for cid, row in self.rows.items():
            if cid == self.my_id:
                continue
            level = min(100, int((raw_level * 100) / 32767)) if raw_level else 0
            now = time.monotonic()
            is_active_instant = level >= 2
            if is_active_instant:
                self.last_voice_ts[cid] = now
            last_ts = self.last_voice_ts.get(cid, 0.0)
            # hangover to avoid UI flicker: keep "Mic: On" for 0.8s after last activity
            is_active = (now - last_ts) < 0.8
            row.setVolume(level)
            row.setMicStatus(is_active)
            prev = self.speaker_state.get(cid, False)
            if is_active != prev:
                timestamp = QtCore.QTime.currentTime().toString("HH:mm:ss")
                msg = f"[{timestamp}] Client {cid} {'speaking' if is_active else 'stopped'}"
                self.speaker_log_list.addItem(msg)
            self.speaker_state[cid] = is_active
            speaking_state[cid] = is_active

        status_lines = [f"Client {cid} - {'talking' if state else 'listening'}"
                        for cid, state in speaking_state.items()]
        self.active_speakers_label.setText("\n".join(status_lines) if status_lines else "No clients")

    def closeEvent(self, event):
        self.ui_timer.stop()
        self.auto_refresh_timer.stop()
        self.stop_capture_timer.stop()
        self.audio.shutdown()
        self.discovery.stop()
        event.accept()


def run_app():
    # Ensure bundled DLLs are discoverable before any native loads
    dll_dirs = [
        ROOT / "third_party" / "opus",
        ROOT / "third_party" / "libportaudio",
        ROOT / "third_party" / "libsodium",
        ROOT / "third_party" / "rnnoise",
        ROOT / "third_party" / "win_webrtc" / "bin",
    ]
    # Prepend to PATH so native LoadLibrary finds them without flags
    existing_path = os.environ.get("PATH", "")
    prepend = ";".join(str(d) for d in dll_dirs if d.exists())
    if prepend:
        os.environ["PATH"] = prepend + ";" + existing_path
    for d in dll_dirs:
        if d.exists():
            os.add_dll_directory(str(d))
    # Qt needs this set before QApplication is created for some OpenGL configs
    QtCore.QCoreApplication.setAttribute(QtCore.Qt.AA_ShareOpenGLContexts)

    app = QtWidgets.QApplication(sys.argv)

    dialog = load_ui("Popup_message.ui")
    name_edit = dialog.findChild(QtWidgets.QLineEdit, "nameEdit")
    room_edit = dialog.findChild(QtWidgets.QLineEdit, "manualIpEdit") or \
        dialog.findChild(QtWidgets.QLineEdit, "roomname")
    local_ip_label = dialog.findChild(QtWidgets.QLabel, "localIpLabel")

    if local_ip_label:
        local_ip_label.setText(get_local_ip())

    if dialog.exec() != QtWidgets.QDialog.Accepted:
        sys.exit(0)

    my_id = name_edit.text().strip() if name_edit else ""
    room_name = room_edit.text().strip() if room_edit else "main"
    room_name = room_name or "main"

    if not my_id:
        QtWidgets.QMessageBox.warning(None, "Error", "Name is required!")
        sys.exit(1)

    audio = PyAudioEngine()
    discovery = PyPeerDiscovery()
    # Derive per-room encryption key so only peers in the same room can decrypt audio
    audio.set_room_secret(room_name)
    discovery.start(my_id, audio.port(), room_name)
    win = MainWindow(my_id, room_name, audio, discovery)
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    run_app()
