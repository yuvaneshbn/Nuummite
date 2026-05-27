import os
import sys
import shutil
import socket
import time
import traceback
import faulthandler
import logging
import threading
from logging.handlers import RotatingFileHandler
from pathlib import Path
from typing import Dict, List, Set

from PySide6 import QtCore, QtWidgets, QtUiTools, QtGui

# Allow running as a script or module
if __package__ in (None, ""):
    sys.path.append(str(Path(__file__).resolve().parent.parent))

from python import PyAudioEngine, PyPeerDiscovery


ROOT = Path(__file__).resolve().parent.parent  # repository root
UI_DIR = ROOT / "Nuummite" / "ui"

_LOG_FILE: Path | None = None
_FAULTHANDLER_FP = None
_DLL_DIRECTORY_COOKIES: list[object] = []


def _is_writable_dir(path: Path) -> bool:
    try:
        path.mkdir(parents=True, exist_ok=True)
        probe = path / "._nuummite_write_probe"
        probe.write_text("ok", encoding="utf-8")
        probe.unlink(missing_ok=True)
        return True
    except Exception:
        return False


def _bootstrap_log_dir() -> Path:
    """
    Pick a log directory that works for both source runs and frozen apps.

    Prefer next to the launcher `.exe` for frozen runs (if writable), otherwise fall
    back to per-user storage.
    """
    if getattr(sys, "frozen", False):
        exe_dir = Path(sys.executable).resolve().parent
        if _is_writable_dir(exe_dir):
            return exe_dir

    # Per-user fallback (recommended for installs under Program Files).
    return _log_dir()


def _init_logging() -> Path:
    global _LOG_FILE

    log_dir = _bootstrap_log_dir()
    log_dir.mkdir(parents=True, exist_ok=True)
    log_path = log_dir / "Nuummite_debug.log"
    _LOG_FILE = log_path

    root_logger = logging.getLogger()
    root_logger.setLevel(logging.DEBUG)

    # Avoid duplicate handlers if reloaded (e.g., interactive runs).
    for h in list(root_logger.handlers):
        root_logger.removeHandler(h)

    fmt = logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s")

    file_handler = None
    for candidate in (
        log_path,
        log_dir / f"Nuummite_debug_{os.getpid()}.log",
        (Path(os.environ.get("TEMP", str(ROOT))) / f"Nuummite_debug_{os.getpid()}.log").resolve(),
    ):
        try:
            file_handler = RotatingFileHandler(
                str(candidate),
                maxBytes=2 * 1024 * 1024,
                backupCount=3,
                encoding="utf-8",
            )
            _LOG_FILE = candidate
            break
        except OSError:
            continue

    if file_handler is not None:
        file_handler.setLevel(logging.DEBUG)
        file_handler.setFormatter(fmt)
        root_logger.addHandler(file_handler)

    stream_handler = logging.StreamHandler(stream=sys.stderr)
    stream_handler.setLevel(logging.INFO)
    stream_handler.setFormatter(fmt)
    root_logger.addHandler(stream_handler)

    logging.getLogger(__name__).info("Initializing Nuummite bootstrap...")
    logging.getLogger(__name__).info("Log file: %s", log_path)
    logging.getLogger(__name__).info("Frozen: %s (_MEIPASS=%s)", getattr(sys, "frozen", False), getattr(sys, "_MEIPASS", None))
    return log_path


def _install_exception_hooks() -> None:
    """
    Ensure crashes in GUI and background threads are recorded to the log file,
    and show a modal dialog when possible.
    """

    def _show_crash_dialog(message: str, details: str) -> None:
        try:
            app = QtWidgets.QApplication.instance()
            if app is None:
                return
            box = QtWidgets.QMessageBox()
            box.setIcon(QtWidgets.QMessageBox.Icon.Critical)
            box.setWindowTitle("Nuummite crashed")
            box.setText(message)
            if _LOG_FILE is not None:
                box.setInformativeText(f"Log written to:\n{_LOG_FILE}")
            if details:
                box.setDetailedText(details)
            box.exec()
        except Exception:
            pass

    def _log_exception(prefix: str, exc_type, exc_value, exc_tb) -> None:
        tb = "".join(traceback.format_exception(exc_type, exc_value, exc_tb))
        logging.critical("%s\n%s", prefix, tb)
        _show_crash_dialog("A fatal error occurred during execution.", tb)

    def excepthook(exc_type, exc_value, exc_tb):
        if issubclass(exc_type, KeyboardInterrupt):
            return sys.__excepthook__(exc_type, exc_value, exc_tb)
        _log_exception("Unhandled exception:", exc_type, exc_value, exc_tb)
        sys.__excepthook__(exc_type, exc_value, exc_tb)
        raise SystemExit(1)

    sys.excepthook = excepthook

    if hasattr(threading, "excepthook"):
        def thread_excepthook(args):
            _log_exception(f"Unhandled thread exception in {args.thread.name}:", args.exc_type, args.exc_value, args.exc_traceback)
        threading.excepthook = thread_excepthook


def _log_dir() -> Path:
    base = os.environ.get("LOCALAPPDATA") or os.environ.get("TEMP") or str(ROOT)
    return Path(base) / "Nuummite" / "logs"


def _write_startup_log() -> Path:
    # Prefer the same directory as the main bootstrap log (next to the .exe if possible).
    try_dirs = [_bootstrap_log_dir(), _log_dir()]
    log_path: Path | None = None
    for d in try_dirs:
        try:
            d.mkdir(parents=True, exist_ok=True)
            candidate = d / "startup_error.log"
            # Probe writability
            candidate.touch(exist_ok=True)
            log_path = candidate
            break
        except Exception:
            continue
    if log_path is None:
        # Absolute last resort.
        log_path = (Path(os.environ.get("TEMP", str(ROOT))) / "Nuummite_startup_error.log").resolve()
    header = []
    try:
        header.append(f"time={time.strftime('%Y-%m-%d %H:%M:%S')}")
        header.append(f"executable={sys.executable}")
        header.append(f"frozen={getattr(sys, 'frozen', False)}")
        header.append(f"_MEIPASS={getattr(sys, '_MEIPASS', None)}")
        header.append(f"ROOT={ROOT}")
        header.append("")
    except Exception:
        header = []

    try:
        log_path.write_text("\n".join(header) + traceback.format_exc(), encoding="utf-8", errors="replace")
    except Exception:
        # Don't let logging failures hide the original crash.
        pass
    return log_path


def _enable_faulthandler() -> None:
    """
    Ensure fatal native crashes (access violation) still produce a readable log.
    """
    try:
        log_dir = _bootstrap_log_dir()
        log_dir.mkdir(parents=True, exist_ok=True)
        fh_path = log_dir / "faulthandler.log"
        global _FAULTHANDLER_FP
        _FAULTHANDLER_FP = fh_path.open("a", encoding="utf-8", errors="replace")
        _FAULTHANDLER_FP.write(f"\n--- faulthandler enabled @ {time.strftime('%Y-%m-%d %H:%M:%S')} ---\n")
        _FAULTHANDLER_FP.flush()
        faulthandler.enable(file=_FAULTHANDLER_FP, all_threads=True)
        logging.getLogger(__name__).info("Faulthandler log: %s", fh_path)
    except Exception:
        pass


def resource_path(rel: str) -> str:
    """
    Resolve resource paths when running from source or PyInstaller (_MEIPASS).
    """
    rel_path = Path(rel)

    bases: list[Path] = []

    # PyInstaller sets sys._MEIPASS to the bundle's application home directory.
    meipass = getattr(sys, "_MEIPASS", None)
    if meipass:
        bases.append(Path(meipass))

    # In a frozen app, sys.executable points to the onedir .exe. Resources often live in `_internal`.
    if getattr(sys, "frozen", False):
        exe_dir = Path(sys.executable).resolve().parent
        bases.extend([exe_dir / "_internal", exe_dir])

    # Dev/source checkout fallback.
    bases.append(ROOT)

    for base in bases:
        candidate = base / rel_path
        if candidate.exists():
            return str(candidate)

    # Last resort: return the first base join to keep error messages stable.
    base0 = bases[0] if bases else ROOT
    return str(base0 / rel_path)


_THEME_KEY = "ui/theme"  # "dark" | "light"


def _apply_qss(app: QtWidgets.QApplication, qss_path: str) -> None:
    try:
        qss = Path(qss_path).read_text(encoding="utf-8")
    except Exception as e:
        logging.getLogger(__name__).warning("Failed to load stylesheet (%s): %s", qss_path, e)
        return

    placeholders = {
        "__CHECKMARK_SVG__": Path(resource_path("Nuummite/ui/checkmark.svg")).resolve().as_posix(),
        "__THEME_SWITCH_OFF_SVG__": Path(resource_path("Nuummite/ui/switch_off.svg")).resolve().as_posix(),
        "__THEME_SWITCH_ON_SVG__": Path(resource_path("Nuummite/ui/switch_on.svg")).resolve().as_posix(),
    }
    for key, value in placeholders.items():
        qss = qss.replace(key, value)

    app.setStyleSheet(qss)


def get_saved_theme() -> str:
    s = QtCore.QSettings()
    theme = str(s.value(_THEME_KEY, "dark")).strip().lower()
    return "light" if theme == "light" else "dark"


def set_saved_theme(theme: str) -> None:
    theme = "light" if str(theme).strip().lower() == "light" else "dark"
    s = QtCore.QSettings()
    s.setValue(_THEME_KEY, theme)
    s.sync()


def apply_theme(app: QtWidgets.QApplication, theme: str) -> None:
    theme = "light" if str(theme).strip().lower() == "light" else "dark"
    qss_file = "light.qss" if theme == "light" else "dark.qss"
    _apply_qss(app, resource_path(f"Nuummite/ui/{qss_file}"))


def _center_and_activate(widget: QtWidgets.QWidget, *, always_on_top: bool = False) -> None:
    if always_on_top:
        widget.setWindowFlag(QtCore.Qt.WindowStaysOnTopHint, True)
    widget.adjustSize()
    screen = QtWidgets.QApplication.primaryScreen()
    if screen is not None:
        avail = screen.availableGeometry()
        geo = widget.frameGeometry()
        geo.moveCenter(avail.center())
        widget.move(geo.topLeft())
    widget.show()
    widget.raise_()
    widget.activateWindow()


class TickSlider(QtWidgets.QSlider):
    """
    QSS-styled QSlider tends to lose tick marks, so we paint them manually.
    """

    def paintEvent(self, event) -> None:
        super().paintEvent(event)

        if self.orientation() != QtCore.Qt.Orientation.Horizontal:
            return

        tick_pos = self.tickPosition()
        if tick_pos == QtWidgets.QSlider.TickPosition.NoTicks:
            return

        minimum = self.minimum()
        maximum = self.maximum()
        if maximum <= minimum:
            return

        interval = self.tickInterval()
        if interval <= 0:
            interval = self.pageStep()
        if interval <= 0:
            interval = 1

        opt = QtWidgets.QStyleOptionSlider()
        self.initStyleOption(opt)

        handle = self.style().subControlRect(
            QtWidgets.QStyle.ComplexControl.CC_Slider,
            opt,
            QtWidgets.QStyle.SubControl.SC_SliderHandle,
            self,
        )
        handle_w = max(1, handle.width())
        span = float(maximum - minimum)
        usable = max(1.0, float(self.width() - handle_w))
        x0 = handle_w / 2.0

        pen = QtGui.QPen(QtGui.QColor("#7A7A7A"))
        pen.setWidth(1)

        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, False)
        painter.setPen(pen)

        tick_h = 4
        y_top = self.rect().top() + 1
        y_bot = self.rect().bottom() - 1

        def _x_for_value(v: int) -> int:
            t = (float(v - minimum) / span) if span > 0 else 0.0
            return int(round(x0 + t * usable))

        for v in range(minimum, maximum + 1, interval):
            x = _x_for_value(v)
            if tick_pos in (QtWidgets.QSlider.TickPosition.TicksBothSides, QtWidgets.QSlider.TickPosition.TicksAbove):
                painter.drawLine(x, y_top, x, y_top + tick_h)
            if tick_pos in (QtWidgets.QSlider.TickPosition.TicksBothSides, QtWidgets.QSlider.TickPosition.TicksBelow):
                painter.drawLine(x, y_bot, x, y_bot - tick_h)

        painter.end()


class NuummiteUiLoader(QtUiTools.QUiLoader):
    def createWidget(self, className: str, parent=None, name: str = ""):
        if className == "QSlider":
            w = TickSlider(parent)
            w.setObjectName(name)
            return w
        return super().createWidget(className, parent, name)


def load_ui(filename: str, parent=None):
    loader = NuummiteUiLoader()
    resolved = resource_path(f"Nuummite/ui/{filename}")

    # Prefer Python I/O to avoid QFile quirks in some frozen environments.
    widget = None
    io_error: Exception | None = None
    try:
        data = Path(resolved).read_bytes()
        buf = QtCore.QBuffer()
        buf.setData(QtCore.QByteArray(data))
        buf.open(QtCore.QIODevice.OpenModeFlag.ReadOnly)
        widget = loader.load(buf, parent)
        buf.close()
    except Exception as e:
        io_error = e

    if widget is None:
        # Secondary fallback: try QFile open (useful if the path is a Qt resource in future).
        try:
            ui_file = QtCore.QFile(resolved)
            if ui_file.open(QtCore.QFile.OpenModeFlag.ReadOnly):
                widget = loader.load(ui_file, parent)
                ui_file.close()
        except Exception:
            pass

    if widget is None:
        hint = ""
        try:
            if getattr(sys, "frozen", False) and not Path(resolved).exists():
                hint = "\n\nThis looks like an incomplete install. If you are running the packaged app, keep the entire folder (including the `_internal` directory) next to the `.exe`."
        except Exception:
            pass
        details = f"\n\nDetails: {io_error}" if io_error else ""
        raise RuntimeError(f"Cannot open UI file: {resolved}{hint}{details}")

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
        self.settings = QtCore.QSettings()
        self.widget: QtWidgets.QWidget = load_ui("volume_control.ui")

        self.master_label = self.widget.findChild(QtWidgets.QLabel, "masterLabel")
        self.master_slider = self.widget.findChild(QtWidgets.QSlider, "masterSlider")
        self.output_label = self.widget.findChild(QtWidgets.QLabel, "outputLabel")
        self.output_slider = self.widget.findChild(QtWidgets.QSlider, "outputSlider")
        self.gain_label = self.widget.findChild(QtWidgets.QLabel, "gainLabel")
        self.gain_slider = self.widget.findChild(QtWidgets.QSlider, "gainSlider")
        self.ns_label = self.widget.findChild(QtWidgets.QLabel, "noiseSuppressionLabel")
        self.ns_slider = self.widget.findChild(QtWidgets.QSlider, "noiseSuppressionSlider")
        self.aec_delay_label = self.widget.findChild(QtWidgets.QLabel, "aecDelayLabel")
        self.aec_delay_slider = self.widget.findChild(QtWidgets.QSlider, "aecDelaySlider")
        self.input_label = self.widget.findChild(QtWidgets.QLabel, "inputSensitivityLabel")
        self.input_slider = self.widget.findChild(QtWidgets.QSlider, "inputSensitivitySlider")
        self.test_button = self.widget.findChild(QtWidgets.QPushButton, "testMicButton")
        self.test_status = self.widget.findChild(QtWidgets.QLabel, "testStatusLabel")
        self.mic_bar = self.widget.findChild(QtWidgets.QProgressBar, "participantVolumeBar") or \
            self.widget.findChild(QtWidgets.QProgressBar, "micLevelBar")
        self.reset_button = self.widget.findChild(QtWidgets.QPushButton, "restoreDefaultsButton")

        self.autogain_checkbox = self.widget.findChild(QtWidgets.QCheckBox, "autoGainCheckbox")
        self.rnnoise_checkbox = self.widget.findChild(QtWidgets.QCheckBox, "noiseSuppCheckbox")
        self.echo_checkbox = self.widget.findChild(QtWidgets.QCheckBox, "echoCheckbox")

        # Set tick marks in code (avoids QUiLoader enum parsing warnings on some Qt builds).
        ticks = QtWidgets.QSlider.TickPosition.TicksBelow
        for slider, interval in (
            (self.master_slider, 10),
            (self.output_slider, 10),
            (self.gain_slider, 5),
            (self.input_slider, 5),
            (self.ns_slider, 5),
            (self.aec_delay_slider, 20),
        ):
            if slider:
                slider.setTickPosition(ticks)
                slider.setTickInterval(interval)

        self.master_slider.setRange(0, 250)
        if self.output_slider:
            self.output_slider.setRange(0, 250)
        self.gain_slider.setRange(-20, 20)
        self.ns_slider.setRange(0, 100)
        if self.aec_delay_slider:
            self.aec_delay_slider.setRange(0, 500)
        if self.input_slider:
            self.input_slider.setRange(0, 100)

        self.master_slider.valueChanged.connect(self._on_master_changed)
        if self.output_slider:
            self.output_slider.valueChanged.connect(self._on_output_changed)
        self.gain_slider.valueChanged.connect(self._on_gain_changed)
        self.ns_slider.valueChanged.connect(self._on_ns_changed)
        if self.aec_delay_slider:
            self.aec_delay_slider.valueChanged.connect(self._on_aec_delay_changed)
        if self.input_slider:
            self.input_slider.valueChanged.connect(self._on_input_changed)
        self.test_button.clicked.connect(self._test_mic)
        if self.reset_button:
            self.reset_button.clicked.connect(self._reset_defaults)

        if self.autogain_checkbox:
            self.autogain_checkbox.toggled.connect(self._on_autogain_toggled)
        if self.rnnoise_checkbox:
            self.rnnoise_checkbox.toggled.connect(self._on_rnnoise_toggled)
        if self.echo_checkbox:
            self.echo_checkbox.toggled.connect(self._on_echo_toggled)

        self._load_settings_into_ui()
        self._apply_ui_to_engine()
        self._update_value_labels()
        self._sync_feature_controls()
        self.widget.update()  # Force UI refresh

    def _update_value_labels(self):
        # Show current numeric values directly in the labels above each slider.
        if self.master_label:
            self.master_label.setText(f"Master Volume: {self.master_slider.value()}")
        if self.output_label and self.output_slider:
            self.output_label.setText(f"Output Volume: {self.output_slider.value()}")
        if self.gain_label:
            self.gain_label.setText(f"Gain (dB): {self.gain_slider.value()}")
        if self.input_label and self.input_slider:
            self.input_label.setText(f"Input Sensitivity: {self.input_slider.value()}")
        if self.ns_label:
            self.ns_label.setText(f"Noise Suppression: {self.ns_slider.value()}")
        if self.aec_delay_label and self.aec_delay_slider:
            self.aec_delay_label.setText(f"AEC Delay (ms): {self.aec_delay_slider.value()}")

    def _on_master_changed(self, v: int):
        self.audio.set_master_volume(int(v))
        self.settings.setValue("audio/masterVolume", int(v))
        self._update_value_labels()

    def _on_output_changed(self, v: int) -> None:
        self.audio.set_output_volume(int(v))
        self.settings.setValue("audio/outputVolume", int(v))
        self._update_value_labels()

    def _on_gain_changed(self, v: int):
        self.audio.set_gain_db(int(v))
        self.settings.setValue("audio/txGainDb", int(v))
        self._update_value_labels()

    def _on_ns_changed(self, v: int):
        self.audio.set_noise_suppression(int(v))
        self.settings.setValue("audio/noiseSuppAmount", int(v))
        self._update_value_labels()

    def _on_aec_delay_changed(self, v: int) -> None:
        self.audio.set_aec_stream_delay_ms(int(v))
        self.settings.setValue("audio/aecDelayMs", int(v))
        self._update_value_labels()

    def _on_input_changed(self, v: int):
        self.audio.set_mic_sensitivity(int(v))
        self.settings.setValue("audio/micSensitivity", int(v))
        self._update_value_labels()

    def _sync_feature_controls(self) -> None:
        autogain_on = bool(self.autogain_checkbox and self.autogain_checkbox.isChecked())
        rnnoise_on = bool(self.rnnoise_checkbox and self.rnnoise_checkbox.isChecked())
        echo_on = bool(self.echo_checkbox and self.echo_checkbox.isChecked())

        if self.gain_slider:
            self.gain_slider.setEnabled(not autogain_on)
            self.gain_slider.setToolTip(
                "Disabled because Auto Gain is enabled."
                if autogain_on
                else "Manual transmit gain."
            )
        if self.gain_label:
            self.gain_label.setEnabled(not autogain_on)
            base = f"Gain (dB): {self.gain_slider.value() if self.gain_slider else ''}".strip()
            self.gain_label.setText(base + (" (Auto Gain enabled)" if autogain_on else ""))
        if self.ns_slider:
            self.ns_slider.setEnabled(rnnoise_on)
        if self.ns_label:
            self.ns_label.setEnabled(rnnoise_on)
        if self.aec_delay_slider:
            self.aec_delay_slider.setEnabled(echo_on)
        if self.aec_delay_label:
            self.aec_delay_label.setEnabled(echo_on)

    def _on_autogain_toggled(self, checked: bool) -> None:
        self.audio.set_auto_gain(bool(checked))
        self.settings.setValue("audio/autoGainEnabled", bool(checked))
        self._sync_feature_controls()

    def _on_rnnoise_toggled(self, checked: bool) -> None:
        self.audio.set_noise_suppression_enabled(bool(checked))
        self.settings.setValue("audio/noiseSuppEnabled", bool(checked))
        self._sync_feature_controls()

    def _on_echo_toggled(self, checked: bool) -> None:
        self.audio.set_echo_enabled(bool(checked))
        self.settings.setValue("audio/echoEnabled", bool(checked))
        self._sync_feature_controls()

    def _test_mic(self):
        level = self.audio.test_microphone_level(0.8)
        self.setMicLevel(level)
        self.test_status.setText(f"Mic level: {level}%")

    def _reset_defaults(self):
        self.settings.setValue("audio/masterVolume", 100)
        self.settings.setValue("audio/outputVolume", 100)
        self.settings.setValue("audio/txGainDb", 0)
        self.settings.setValue("audio/micSensitivity", 45)
        self.settings.setValue("audio/noiseSuppAmount", 65)
        self.settings.setValue("audio/aecDelayMs", 180)
        self.settings.setValue("audio/autoGainEnabled", False)
        self.settings.setValue("audio/noiseSuppEnabled", False)
        self.settings.setValue("audio/echoEnabled", False)
        self._load_settings_into_ui()
        self._apply_ui_to_engine()
        self._update_value_labels()
        self._sync_feature_controls()
        self.setMicLevel(self.audio.capture_level)

    def _load_settings_into_ui(self) -> None:
        def _set(widget, value):
            if not widget:
                return
            widget.blockSignals(True)
            try:
                if isinstance(widget, QtWidgets.QAbstractButton):
                    widget.setChecked(bool(value))
                elif isinstance(widget, QtWidgets.QSlider):
                    widget.setValue(int(value))
            finally:
                widget.blockSignals(False)

        _set(self.master_slider, self.settings.value("audio/masterVolume", 100, type=int))
        _set(self.output_slider, self.settings.value("audio/outputVolume", 100, type=int))
        _set(self.gain_slider, self.settings.value("audio/txGainDb", 0, type=int))
        _set(self.ns_slider, self.settings.value("audio/noiseSuppAmount", 65, type=int))
        _set(self.input_slider, self.settings.value("audio/micSensitivity", 45, type=int))
        _set(self.aec_delay_slider, self.settings.value("audio/aecDelayMs", 180, type=int))

        _set(self.autogain_checkbox, self.settings.value("audio/autoGainEnabled", False, type=bool))
        _set(self.rnnoise_checkbox, self.settings.value("audio/noiseSuppEnabled", False, type=bool))
        _set(self.echo_checkbox, self.settings.value("audio/echoEnabled", False, type=bool))

    def _apply_ui_to_engine(self) -> None:
        self.audio.set_master_volume(int(self.master_slider.value()))
        if self.output_slider:
            self.audio.set_output_volume(int(self.output_slider.value()))
        self.audio.set_gain_db(int(self.gain_slider.value()))
        self.audio.set_noise_suppression(int(self.ns_slider.value()))
        if self.input_slider:
            self.audio.set_mic_sensitivity(int(self.input_slider.value()))
        if self.aec_delay_slider:
            self.audio.set_aec_stream_delay_ms(int(self.aec_delay_slider.value()))
        self.audio.set_auto_gain(bool(self.autogain_checkbox and self.autogain_checkbox.isChecked()))
        self.audio.set_noise_suppression_enabled(bool(self.rnnoise_checkbox and self.rnnoise_checkbox.isChecked()))
        self.audio.set_echo_enabled(bool(self.echo_checkbox and self.echo_checkbox.isChecked()))

    def setMicLevel(self, level: int):
        if self.mic_bar:
            self.mic_bar.setRange(0, 100)
            self.mic_bar.setValue(max(0, min(100, level)))


class SettingsDialog(QtWidgets.QDialog):
    def __init__(self, audio: PyAudioEngine, server_ip: str, parent=None):
        super().__init__(parent)
        self.audio = audio
        self.server_ip = server_ip
        self.settings = QtCore.QSettings()
        self.form = load_ui("settings_dialog.ui")
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        scroll = QtWidgets.QScrollArea(self)
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QtWidgets.QFrame.Shape.NoFrame)
        scroll.setWidget(self.form)
        layout.addWidget(scroll)

        self.input_combo = self.form.findChild(QtWidgets.QComboBox, "inputDeviceCombo")
        self.output_combo = self.form.findChild(QtWidgets.QComboBox, "outputDeviceCombo")
        self.server_ip_value = self.form.findChild(QtWidgets.QLabel, "serverIpValue")
        self.reconnect_button = self.form.findChild(QtWidgets.QPushButton, "reconnectButton")
        self.save_button = self.form.findChild(QtWidgets.QPushButton, "saveCloseButton")
        self.cancel_button = self.form.findChild(QtWidgets.QPushButton, "cancelButton")
        self.advanced_layout = self.form.findChild(QtWidgets.QVBoxLayout, "advancedAudioLayout")
        self.volume_hint = self.form.findChild(QtWidgets.QLabel, "volumeControlHint")

        # Force refresh of hint and embed panel every time dialog opens
        if self.volume_hint:
            self.volume_hint.hide()  # avoid overlapping with embedded controls

        # Always recreate the volume panel fresh
        self.vol_panel = VolumeControlPanel(audio, self)
        if self.advanced_layout:
            # Clear any previous widgets
            while self.advanced_layout.count():
                item = self.advanced_layout.takeAt(0)
                if item.widget():
                    item.widget().deleteLater()
            self.advanced_layout.addWidget(self.vol_panel.widget)

        self.server_ip_value.setText(server_ip)

        self.reconnect_button.clicked.connect(self._noop_reconnect)
        self.save_button.clicked.connect(self._save_and_close)
        self.cancel_button.clicked.connect(self.reject)
        self.input_combo.currentIndexChanged.connect(self._input_changed)
        self.output_combo.currentIndexChanged.connect(self._output_changed)

        self.setWindowTitle(self.form.windowTitle())
        self.setSizeGripEnabled(True)

        # Apply persisted device choices (best-effort) before populating UI.
        self._apply_saved_devices_to_engine()
        self._populate_devices()

        # Ensure initial size can accommodate embedded controls but remains resizable.
        hint = self.sizeHint()
        self.resize(max(hint.width(), 480), max(hint.height(), 640))

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

    def show_error(self, title: str, message: str, details: str | None = None) -> None:
        box = QtWidgets.QMessageBox(self)
        box.setIcon(QtWidgets.QMessageBox.Icon.Critical)
        box.setWindowTitle(title)
        box.setText(message)
        if details:
            box.setDetailedText(details)
        box.setStandardButtons(QtWidgets.QMessageBox.StandardButton.Ok)
        box.exec()

    def _restore_combo_to_device(self, combo: QtWidgets.QComboBox, device_index: int) -> None:
        combo.blockSignals(True)
        try:
            idx = combo.findData(device_index)
            if idx >= 0:
                combo.setCurrentIndex(idx)
        finally:
            combo.blockSignals(False)

    def _input_changed(self, _):
        data = self.input_combo.currentData()
        if data is not None:
            requested = int(data)
            previous = int(self.audio.input_device_index)
            try:
                ok = bool(self.audio.set_input_device(requested))
            except Exception as e:
                self.show_error(
                    "Input device error",
                    "Something went wrong while applying the selected input device.",
                    f"Device: {self.input_combo.currentText()}\nIndex: {requested}\n\n{e}",
                )
                self._restore_combo_to_device(self.input_combo, previous)
                return

            if not ok:
                self.show_error(
                    "Input device error",
                    "Failed to open the selected input device.",
                    f"Device: {self.input_combo.currentText()}\nIndex: {requested}",
                )
                self._restore_combo_to_device(self.input_combo, previous)
                return

            self.settings.setValue("audio/inputDeviceIndex", requested)

    def _output_changed(self, _):
        data = self.output_combo.currentData()
        if data is not None:
            requested = int(data)
            previous = int(self.audio.output_device_index)
            try:
                ok = bool(self.audio.set_output_device(requested))
            except Exception as e:
                self.show_error(
                    "Output device error",
                    "Something went wrong while applying the selected output device.",
                    f"Device: {self.output_combo.currentText()}\nIndex: {requested}\n\n{e}",
                )
                self._restore_combo_to_device(self.output_combo, previous)
                return

            if not ok:
                self.show_error(
                    "Output device error",
                    "Failed to open the selected output device.",
                    f"Device: {self.output_combo.currentText()}\nIndex: {requested}",
                )
                self._restore_combo_to_device(self.output_combo, previous)
                return

            self.settings.setValue("audio/outputDeviceIndex", requested)

    def _noop_reconnect(self):
        QtWidgets.QMessageBox.information(self, "Reconnect", "Reconnect not required for LAN mesh.")

    def _apply_saved_devices_to_engine(self) -> None:
        def _ival(key: str):
            try:
                v = self.settings.value(key, None)
                if v is None or v == "":
                    return None
                return int(v)
            except Exception:
                return None

        in_idx = _ival("audio/inputDeviceIndex")
        out_idx = _ival("audio/outputDeviceIndex")

        if in_idx is not None:
            try:
                self.audio.set_input_device(in_idx)
            except Exception:
                pass
        if out_idx is not None:
            try:
                self.audio.set_output_device(out_idx)
            except Exception:
                pass

    def _save_and_close(self) -> None:
        # Advanced audio controls persist on change; ensure device selections are persisted too.
        if self.input_combo is not None:
            data = self.input_combo.currentData()
            if data is not None:
                self.settings.setValue("audio/inputDeviceIndex", int(data))
        if self.output_combo is not None:
            data = self.output_combo.currentData()
            if data is not None:
                self.settings.setValue("audio/outputDeviceIndex", int(data))
        self.settings.sync()
        self.accept()


def apply_saved_audio_settings(audio: PyAudioEngine) -> None:
    """
    Apply persisted audio DSP/levels immediately on startup, even before the settings UI is opened.
    """
    s = QtCore.QSettings()

    def _ival(key: str, default: int) -> int:
        try:
            return int(s.value(key, default))
        except Exception:
            return default

    def _bval(key: str, default: bool) -> bool:
        v = s.value(key, default)
        if isinstance(v, bool):
            return v
        if isinstance(v, (int, float)):
            return bool(v)
        if isinstance(v, str):
            return v.strip().lower() in ("1", "true", "yes", "on")
        return bool(v)

    def _opt_ival(key: str):
        v = s.value(key, None)
        if v is None or v == "":
            return None
        try:
            return int(v)
        except Exception:
            return None

    audio.set_master_volume(_ival("audio/masterVolume", 100))
    audio.set_output_volume(_ival("audio/outputVolume", 100))
    audio.set_gain_db(_ival("audio/txGainDb", 0))
    audio.set_mic_sensitivity(_ival("audio/micSensitivity", 45))
    audio.set_noise_suppression(_ival("audio/noiseSuppAmount", 65))
    audio.set_aec_stream_delay_ms(_ival("audio/aecDelayMs", 180))

    audio.set_noise_suppression_enabled(_bval("audio/noiseSuppEnabled", False))
    audio.set_echo_enabled(_bval("audio/echoEnabled", False))
    audio.set_auto_gain(_bval("audio/autoGainEnabled", False))

    in_idx = _opt_ival("audio/inputDeviceIndex")
    out_idx = _opt_ival("audio/outputDeviceIndex")
    if in_idx is not None:
        try:
            audio.set_input_device(in_idx)
        except Exception:
            pass
    if out_idx is not None:
        try:
            audio.set_output_device(out_idx)
        except Exception:
            pass


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
        self.theme_switch = find(QtWidgets.QCheckBox, "themeSwitch")
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

        if self.theme_switch is not None:
            self.theme_switch.blockSignals(True)
            self.theme_switch.setChecked(get_saved_theme() == "dark")
            self._sync_theme_switch_label()
            self.theme_switch.blockSignals(False)
            self.theme_switch.toggled.connect(self._on_theme_toggled)

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
        self.self_muted: bool = False  # keep self mute state across UI refreshes

        self.refresh_participants(False)
        self.set_connected_state(True)

    def _on_theme_toggled(self, checked: bool) -> None:
        theme = "dark" if checked else "light"
        set_saved_theme(theme)
        self._sync_theme_switch_label()
        app = QtWidgets.QApplication.instance()
        if app is not None:
            apply_theme(app, theme)

    def _sync_theme_switch_label(self) -> None:
        if self.theme_switch is None:
            return
        self.theme_switch.setText("Dark" if self.theme_switch.isChecked() else "Light")

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
            row = ParticipantRow(cid, is_self, cid in self.targets,
                                 self.self_muted if is_self else (cid in self.muted),
                                 self)
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
            # Self mute checkbox mirrors the main mute button
            self._set_self_mute(enabled, source="checkbox")
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
        local_ip = get_local_ip()
        for cid in self.targets:
            peer = peers.get(cid)
            if peer:
                ip = peer["ip"]
                port = peer.get("port", self.audio.port())
                # Use loopback for peers on the same host to keep local instances distinct.
                if peer.get("is_local") or ip.startswith("127.") or ip == local_ip:
                    dest_ips.append(f"127.0.0.1:{port}")
                else:
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

    def _set_self_mute(self, muted: bool, source: str):
        """Centralize self-mute state so button and checkbox stay in sync."""
        if self.self_muted == muted:
            pass
        self.self_muted = muted
        self.audio.set_tx_muted(muted)
        self.mute_button.blockSignals(source == "checkbox")
        self.mute_button.setChecked(muted)
        self.mute_button.setText("Unmute Mic" if muted else "Mute Mic")
        self.mute_button.blockSignals(False)
        row = self.rows.get(self.my_id)
        if row:
            row.setMuteChecked(muted)
            row.setMicStatus(not muted)
        self.main_status_bar.showMessage("Microphone muted" if muted else "Microphone unmuted")

    def toggle_self_mute(self, muted: bool):
        # Called from the mute button
        self._set_self_mute(muted, source="button")

    def open_settings(self):
        dlg = SettingsDialog(self.audio, "P2P Mesh", self)
        dlg.exec()

    def update_live_ui(self):
        mic_level = self.audio.capture_level
        self.system_level_bar.setValue(mic_level)
        self.volume_controls.setMicLevel(mic_level)

        sent = getattr(self.audio, "packets_sent", 0)
        recv = getattr(self.audio, "packets_recv", 0)
        dec = getattr(self.audio, "packets_decrypted", 0)

        speaking_state: Dict[str, bool] = {}
        self_state = self.audio.capture_active and not self.audio.tx_muted
        row = self.rows.get(self.my_id)
        if row:
            row.setVolume(mic_level)
            row.setMicStatus(not self.audio.tx_muted)
        speaking_state[self.my_id] = self_state
        self.last_voice_ts[self.my_id] = time.monotonic()
        for cid, row in self.rows.items():
            if cid == self.my_id:
                continue

            raw_level = self.audio.get_peer_peak(cid)
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
        status_lines.append(f"Packets: tx={sent} rx={recv} dec={dec}")
        self.active_speakers_label.setText("\n".join(status_lines) if status_lines else "No clients")

    def closeEvent(self, event):
        self.ui_timer.stop()
        self.auto_refresh_timer.stop()
        self.stop_capture_timer.stop()
        self.audio.shutdown()
        self.discovery.stop()
        event.accept()


def run_app():
    logging.getLogger(__name__).info("Entering run_app()...")
    # Ensure bundled DLLs are discoverable before any native loads
    bases: list[Path] = []

    # PyInstaller sets sys._MEIPASS for onefile builds.
    meipass = getattr(sys, "_MEIPASS", None)
    if meipass:
        bases.append(Path(meipass))

    # For onedir builds, sys.executable is the launcher .exe next to `_internal`.
    if getattr(sys, "frozen", False):
        exe_dir = Path(sys.executable).resolve().parent
        bases.extend([exe_dir / "_internal", exe_dir])

    # Source checkout fallback (also equals `_internal` when running from onedir bundle).
    bases.append(ROOT)

    def _unique_paths(paths: list[Path]) -> list[Path]:
        seen: set[str] = set()
        out: list[Path] = []
        for p in paths:
            key = str(p)
            if key in seen:
                continue
            seen.add(key)
            out.append(p)
        return out

    third_party_suffixes = [
        Path("third_party") / "opus",
        Path("third_party") / "libportaudio",
        Path("third_party") / "libsodium",
        Path("third_party") / "rnnoise",
        Path("third_party") / "webrtc_audio_processing" / "bin",
    ]

    dll_dirs: list[Path] = []
    for base in bases:
        dll_dirs.append(base)
        for suffix in third_party_suffixes:
            dll_dirs.append(base / suffix)
    dll_dirs = _unique_paths(dll_dirs)

    # Prepend to PATH so native LoadLibrary finds them without flags
    existing_path = os.environ.get("PATH", "")
    prepend = ";".join(str(d) for d in dll_dirs if d.exists())
    if prepend:
        os.environ["PATH"] = prepend + ";" + existing_path
    logging.getLogger(__name__).debug("DLL dir candidates: %s", [str(p) for p in dll_dirs])
    for d in dll_dirs:
        if not d.exists():
            continue
        try:
            _DLL_DIRECTORY_COOKIES.append(os.add_dll_directory(str(d)))
        except (AttributeError, OSError):
            pass
    logging.getLogger(__name__).info("Configured %d DLL directories.", len(_DLL_DIRECTORY_COOKIES))

    # Qt needs this set before QApplication is created for some OpenGL configs.
    # PySide6/Qt6 may expose this attribute differently (or not at all).
    share_attr = None
    if hasattr(QtCore.Qt, "AA_ShareOpenGLContexts"):
        share_attr = QtCore.Qt.AA_ShareOpenGLContexts
    elif hasattr(QtCore.Qt, "ApplicationAttribute") and hasattr(QtCore.Qt.ApplicationAttribute, "AA_ShareOpenGLContexts"):
        share_attr = QtCore.Qt.ApplicationAttribute.AA_ShareOpenGLContexts
    if share_attr is not None:
        try:
            QtCore.QCoreApplication.setAttribute(share_attr)
        except Exception:
            pass

    logging.getLogger(__name__).info("Creating QApplication...")
    app = QtWidgets.QApplication(sys.argv)
    app.setStyle("Fusion")
    app.setOrganizationName("Nuummite")
    app.setApplicationName("Nuummite")
    apply_theme(app, get_saved_theme())

    logging.getLogger(__name__).info("Loading startup dialog UI (Popup_message.ui)...")
    dialog = load_ui("Popup_message.ui")
    name_edit = dialog.findChild(QtWidgets.QLineEdit, "nameEdit")
    room_edit = dialog.findChild(QtWidgets.QLineEdit, "manualIpEdit") or \
        dialog.findChild(QtWidgets.QLineEdit, "roomname")
    local_ip_label = dialog.findChild(QtWidgets.QLabel, "localIpLabel")

    if local_ip_label:
        local_ip_label.setText(get_local_ip())

    logging.getLogger(__name__).info("Showing startup dialog...")
    _center_and_activate(dialog, always_on_top=True)
    if dialog.exec() != QtWidgets.QDialog.Accepted:
        logging.getLogger(__name__).info("Startup dialog canceled; exiting.")
        sys.exit(0)

    my_id = name_edit.text().strip() if name_edit else ""
    room_name = room_edit.text().strip() if room_edit else "main"
    room_name = room_name or "main"

    if not my_id:
        QtWidgets.QMessageBox.warning(None, "Error", "Name is required!")
        logging.getLogger(__name__).warning("Name is required; exiting.")
        sys.exit(1)

    room_passphrase, ok = QtWidgets.QInputDialog.getText(
        None,
        "Room Encryption Setup",
        "Enter Private Room Passphrase (Not transmitted):",
        QtWidgets.QLineEdit.EchoMode.Password,
    )
    room_passphrase = (room_passphrase or "").strip()
    if not ok or not room_passphrase:
        logging.getLogger(__name__).info("Passphrase dialog canceled; exiting.")
        sys.exit(0)

    logging.getLogger(__name__).info("Initializing audio/discovery stack...")
    audio = PyAudioEngine()
    apply_saved_audio_settings(audio)
    discovery = PyPeerDiscovery()
    # Derive the symmetric encryption key locally from a private passphrase.
    audio.set_room_secret(room_passphrase)
    discovery.start(my_id, audio.port(), room_name)
    win = MainWindow(my_id, room_name, audio, discovery)
    win.show()
    win.raise_()
    win.activateWindow()
    logging.getLogger(__name__).info("Entering Qt event loop...")
    sys.exit(app.exec())


if __name__ == "__main__":
    _init_logging()
    _install_exception_hooks()
    _enable_faulthandler()
    try:
        run_app()
    except Exception:
        logging.exception("Nuummite crashed during startup.")
        log_path = _write_startup_log()
        try:
            app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv)
            QtWidgets.QMessageBox.critical(
                None,
                "Nuummite failed to start",
                f"Nuummite crashed during startup.\n\nLog written to:\n{log_path}",
            )
            app.processEvents()
        except Exception:
            pass
        raise 
