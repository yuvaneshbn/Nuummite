from __future__ import annotations

from PySide6 import QtCore, QtWidgets

from python import PyAudioEngine
from python.ui.volume_control_ui import Ui_VolumeControlForm


class VolumeControlWidget(QtWidgets.QWidget):
    def __init__(self, parent: QtWidgets.QWidget | None = None):
        super().__init__(parent)
        self.ui = Ui_VolumeControlForm()
        self.ui.setupUi(self)
        self.settings = QtCore.QSettings()
        self._audio: PyAudioEngine | None = None
        self._is_active = True
        self._signals_connected = False

        ticks = QtWidgets.QSlider.TickPosition.TicksBelow
        slider_configs: list[tuple[QtWidgets.QSlider | None, int]] = [
            (self.ui.masterSlider, 10),
            (self.ui.outputSlider, 10),
            (self.ui.gainSlider, 5),
            (self.ui.inputSensitivitySlider, 5),
            (self.ui.noiseSuppressionSlider, 5),
            (self.ui.aecDelaySlider, 20),
        ]
        for slider, interval in slider_configs:
            if slider is not None:
                slider.setTickPosition(ticks)
                slider.setTickInterval(interval)

    def initialize_audio(self, audio: PyAudioEngine):
        self._audio = audio

        if not self._signals_connected:
            self.ui.masterSlider.valueChanged.connect(self._on_master_changed)
            if self.ui.outputSlider:
                self.ui.outputSlider.valueChanged.connect(self._on_output_changed)
            self.ui.gainSlider.valueChanged.connect(self._on_gain_changed)
            self.ui.noiseSuppressionSlider.valueChanged.connect(self._on_ns_changed)
            if self.ui.aecDelaySlider:
                self.ui.aecDelaySlider.valueChanged.connect(self._on_aec_delay_changed)
            if self.ui.inputSensitivitySlider:
                self.ui.inputSensitivitySlider.valueChanged.connect(self._on_input_changed)

            self.ui.autoGainCheckbox.toggled.connect(self._on_autogain_toggled)
            self.ui.noiseSuppCheckbox.toggled.connect(self._on_rnnoise_toggled)
            self.ui.echoCheckbox.toggled.connect(self._on_echo_toggled)

            if self.ui.restoreDefaultsButton:
                self.ui.restoreDefaultsButton.clicked.connect(self.reset_defaults)
            self.ui.testMicButton.clicked.connect(self.test_microphone)
            self._signals_connected = True

        self.load_settings()
        self.update_labels()
        self.sync_feature_controls()

    def refresh_from_settings(self) -> None:
        if not self._audio:
            return
        self.load_settings()
        self._apply_ui_to_audio()
        self.update_labels()
        self.sync_feature_controls()
        self.set_mic_level(getattr(self._audio, "capture_level", 0))

    def load_settings(self) -> None:
        if not self._audio:
            return

        def _safe_load(control, key: str, default):
            control.blockSignals(True)
            val = self.settings.value(key, default)
            if isinstance(control, QtWidgets.QSlider):
                control.setValue(int(val))
            elif isinstance(control, QtWidgets.QAbstractButton):
                control.setChecked(bool(val))
            control.blockSignals(False)

        _safe_load(self.ui.masterSlider, "audio/masterVolume", 100)
        _safe_load(self.ui.outputSlider, "audio/outputVolume", 100)
        _safe_load(self.ui.gainSlider, "audio/txGainDb", 0)
        _safe_load(self.ui.noiseSuppressionSlider, "audio/noiseSuppAmount", 65)
        _safe_load(self.ui.inputSensitivitySlider, "audio/micSensitivity", 45)
        _safe_load(self.ui.aecDelaySlider, "audio/aecDelayMs", 180)
        _safe_load(self.ui.autoGainCheckbox, "audio/autoGainEnabled", False)
        _safe_load(self.ui.noiseSuppCheckbox, "audio/noiseSuppEnabled", False)
        _safe_load(self.ui.echoCheckbox, "audio/echoEnabled", False)

    def _apply_ui_to_audio(self) -> None:
        if not self._audio:
            return

        def _value(control, default: int) -> int:
            if control is None:
                return default
            try:
                return int(control.value())
            except Exception:
                return default

        def _checked(control, default: bool) -> bool:
            if control is None:
                return default
            try:
                return bool(control.isChecked())
            except Exception:
                return default

        try:
            self._audio.set_master_volume(_value(self.ui.masterSlider, 100))
        except Exception:
            pass
        try:
            if self.ui.outputSlider:
                self._audio.set_output_volume(_value(self.ui.outputSlider, 100))
        except Exception:
            pass
        try:
            self._audio.set_gain_db(_value(self.ui.gainSlider, 0))
        except Exception:
            pass
        try:
            self._audio.set_noise_suppression(_value(self.ui.noiseSuppressionSlider, 65))
        except Exception:
            pass
        try:
            if self.ui.inputSensitivitySlider:
                self._audio.set_mic_sensitivity(_value(self.ui.inputSensitivitySlider, 45))
        except Exception:
            pass
        try:
            if self.ui.aecDelaySlider:
                self._audio.set_aec_stream_delay_ms(_value(self.ui.aecDelaySlider, 180))
        except Exception:
            pass
        try:
            self._audio.set_auto_gain(_checked(self.ui.autoGainCheckbox, False))
        except Exception:
            pass
        try:
            self._audio.set_noise_suppression_enabled(_checked(self.ui.noiseSuppCheckbox, False))
        except Exception:
            pass
        try:
            self._audio.set_echo_enabled(_checked(self.ui.echoCheckbox, False))
        except Exception:
            pass

    def sync_feature_controls(self) -> None:
        autogain_on = self.ui.autoGainCheckbox.isChecked()
        rnnoise_on = self.ui.noiseSuppCheckbox.isChecked()
        echo_on = self.ui.echoCheckbox.isChecked()

        gain_enabled = not autogain_on
        ns_enabled = rnnoise_on
        echo_enabled = echo_on

        self.ui.gainSlider.setEnabled(gain_enabled)
        self.ui.gainLabel.setEnabled(gain_enabled)
        self.ui.noiseSuppressionSlider.setEnabled(ns_enabled)
        self.ui.noiseSuppressionLabel.setEnabled(ns_enabled)
        self.ui.aecDelaySlider.setEnabled(echo_enabled)
        self.ui.aecDelayLabel.setEnabled(echo_enabled)

        self.ui.gainSlider.setToolTip(
            "Manual gain is disabled while Auto-Gain is enabled."
            if not gain_enabled
            else "Adjust transmit gain manually."
        )
        self.ui.noiseSuppressionSlider.setToolTip(
            "Enable Noise Suppression to adjust this slider."
            if not ns_enabled
            else "Adjust noise suppression strength."
        )
        self.ui.aecDelaySlider.setToolTip(
            "Enable Echo Cancellation to adjust this slider."
            if not echo_enabled
            else "Adjust the AEC stream delay."
        )

    def update_labels(self) -> None:
        self.ui.masterLabel.setText(f"Master Volume: {self.ui.masterSlider.value()}")
        if self.ui.outputLabel and self.ui.outputSlider:
            self.ui.outputLabel.setText(f"Output Volume: {self.ui.outputSlider.value()}")
        autogain_on = self.ui.autoGainCheckbox.isChecked()
        rnnoise_on = self.ui.noiseSuppCheckbox.isChecked()
        echo_on = self.ui.echoCheckbox.isChecked()
        self.ui.gainLabel.setText(
            f"Gain (dB): {self.ui.gainSlider.value()}"
            + (" (Auto Gain enabled; manual control disabled)" if autogain_on else "")
        )
        if self.ui.inputSensitivityLabel and self.ui.inputSensitivitySlider:
            self.ui.inputSensitivityLabel.setText(f"Input Sensitivity: {self.ui.inputSensitivitySlider.value()}")
        self.ui.noiseSuppressionLabel.setText(
            f"Noise Suppression: {self.ui.noiseSuppressionSlider.value()}"
            + (" (Disabled until checkbox is enabled)" if not rnnoise_on else "")
        )
        self.ui.aecDelayLabel.setText(
            f"AEC Delay: {self.ui.aecDelaySlider.value()} ms"
            + (" (Disabled until checkbox is enabled)" if not echo_on else "")
        )

    def _on_master_changed(self, val: int):
        if self._audio:
            self._audio.set_master_volume(val)
            self.settings.setValue("audio/masterVolume", val)
            self.update_labels()

    def _on_output_changed(self, val: int):
        if self._audio:
            self._audio.set_output_volume(val)
            self.settings.setValue("audio/outputVolume", val)
            self.update_labels()

    def _on_gain_changed(self, val: int):
        if self._audio:
            self._audio.set_gain_db(val)
            self.settings.setValue("audio/txGainDb", val)
            self.update_labels()

    def _on_ns_changed(self, val: int):
        if self._audio:
            self._audio.set_noise_suppression(val)
            self.settings.setValue("audio/noiseSuppAmount", val)
            self.update_labels()

    def _on_aec_delay_changed(self, val: int):
        if self._audio:
            self._audio.set_aec_stream_delay_ms(val)
            self.settings.setValue("audio/aecDelayMs", val)
            self.update_labels()

    def _on_input_changed(self, val: int):
        if self._audio:
            self._audio.set_mic_sensitivity(val)
            self.settings.setValue("audio/micSensitivity", val)
            self.update_labels()

    def _on_autogain_toggled(self, checked: bool):
        if self._audio:
            self._audio.set_auto_gain(checked)
            self.settings.setValue("audio/autoGainEnabled", checked)
            self.sync_feature_controls()
            self.update_labels()

    def _on_rnnoise_toggled(self, checked: bool):
        if self._audio:
            self._audio.set_noise_suppression_enabled(checked)
            self.settings.setValue("audio/noiseSuppEnabled", checked)
            self.sync_feature_controls()
            self.update_labels()

    def _on_echo_toggled(self, checked: bool):
        if self._audio:
            self._audio.set_echo_enabled(checked)
            self.settings.setValue("audio/echoEnabled", checked)
            self.sync_feature_controls()
            self.update_labels()

    def test_microphone(self) -> None:
        if self._audio:
            level = self._audio.test_microphone_level(0.8)
            self.set_mic_level(level)
            self.ui.testStatusLabel.setText(f"Mic level: {level}%")

    def reset_defaults(self) -> None:
        default_settings = [
            ("audio/masterVolume", 100),
            ("audio/outputVolume", 100),
            ("audio/txGainDb", 0),
            ("audio/micSensitivity", 45),
            ("audio/noiseSuppAmount", 65),
            ("audio/aecDelayMs", 180),
            ("audio/autoGainEnabled", False),
            ("audio/noiseSuppEnabled", False),
            ("audio/echoEnabled", False),
        ]
        for key, val in default_settings:
            self.settings.setValue(key, val)
        self.refresh_from_settings()

    def set_mic_level(self, level: int):
        if self._is_active and self.ui.micLevelBar:
            try:
                self.ui.micLevelBar.setValue(max(0, min(100, level)))
            except RuntimeError:
                pass

    def setMicLevel(self, level: int):
        self.set_mic_level(level)

    def closeEvent(self, event):
        self._is_active = False
        super().closeEvent(event)
