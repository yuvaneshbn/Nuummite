# audio_wrapper.pyx
# distutils: language = c++
# cython: language_level=3
# cython: embedsignature=True

from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp.unordered_set cimport unordered_set
from libcpp cimport bool
from libc.stdint cimport uint16_t, int16_t

cdef extern from "audio/audio_engine.h" namespace "":
    cdef struct AudioDeviceInfo:
        int index
        string name

    cdef cppclass AudioEngine:
        AudioEngine() except +
        int port() const
        vector[AudioDeviceInfo] listInputDevices() const
        vector[AudioDeviceInfo] listOutputDevices() const
        bool setInputDevice(int)
        bool setOutputDevice(int)
        int inputDeviceIndex() const
        int outputDeviceIndex() const
        void setClientId(const string&)
        void setRoomSecret(const string&)
        void setMasterVolume(int)
        void setOutputVolume(int)
        void setGainDb(int)
        void setMicSensitivity(int)
        void setNoiseSuppression(int)
        void setNoiseSuppressionEnabled(bool)
        void setAutoGain(bool)
        void setEchoEnabled(bool)
        void setAecStreamDelayMs(int)
        bool echoAvailable() const
        bool echoEnabled() const
        void setTxMuted(bool)
        bool isTxMuted() const
        int testMicrophoneLevel(double duration_sec)
        bool start(const vector[string]&)
        bool updateDestinations(const vector[string]&)
        void stop()
        void shutdown()
        bool isRunning() const
        int captureLevel() const
        bool captureActive() const
        float mixedPeak() const
        void setHearTargets(const unordered_set[string]&)
        void renderOutput(int16_t* out, int sample_count)

cdef extern from "p2p/peer_discovery.h" namespace "":
    cdef struct PeerInfo:
        string id
        string ip
        uint16_t port
        string room
        bool is_local

    cdef cppclass PeerDiscovery:
        PeerDiscovery() except +
        void start(const string&, uint16_t, const string&)
        void stop()
        vector[PeerInfo] peers() const
        string currentRoom() const

cdef class PyAudioEngine:
    cdef AudioEngine* thisptr

    def __cinit__(self):
        self.thisptr = new AudioEngine()

    def __dealloc__(self):
        del self.thisptr

    def port(self):
        return self.thisptr.port()

    def list_input_devices(self):
        cdef vector[AudioDeviceInfo] devs = self.thisptr.listInputDevices()
        result = []
        for i in range(devs.size()):
            d = devs[i]
            result.append({"name": d.name.decode("utf-8", errors="replace"), "index": d.index})
        return result

    def list_output_devices(self):
        cdef vector[AudioDeviceInfo] devs = self.thisptr.listOutputDevices()
        result = []
        for i in range(devs.size()):
            d = devs[i]
            result.append({"name": d.name.decode("utf-8", errors="replace"), "index": d.index})
        return result

    def set_input_device(self, int idx):
        return self.thisptr.setInputDevice(idx)

    def set_output_device(self, int idx):
        return self.thisptr.setOutputDevice(idx)

    @property
    def input_device_index(self):
        return self.thisptr.inputDeviceIndex()

    @property
    def output_device_index(self):
        return self.thisptr.outputDeviceIndex()

    def set_client_id(self, str client_id):
        self.thisptr.setClientId(client_id.encode("utf-8"))

    def set_room_secret(self, str secret):
        self.thisptr.setRoomSecret(secret.encode("utf-8"))

    def set_master_volume(self, int value):
        self.thisptr.setMasterVolume(value)

    def set_output_volume(self, int value):
        self.thisptr.setOutputVolume(value)

    def set_gain_db(self, int value):
        self.thisptr.setGainDb(value)

    def set_mic_sensitivity(self, int value):
        self.thisptr.setMicSensitivity(value)

    def set_noise_suppression(self, int value):
        self.thisptr.setNoiseSuppression(value)

    def set_noise_suppression_enabled(self, bool enabled):
        self.thisptr.setNoiseSuppressionEnabled(enabled)

    def set_auto_gain(self, bool enabled):
        self.thisptr.setAutoGain(enabled)

    def set_echo_enabled(self, bool enabled):
        self.thisptr.setEchoEnabled(enabled)

    def set_aec_stream_delay_ms(self, int delay_ms):
        self.thisptr.setAecStreamDelayMs(delay_ms)

    def echo_available(self):
        return self.thisptr.echoAvailable()

    def echo_enabled(self):
        return self.thisptr.echoEnabled()

    def set_tx_muted(self, bool enabled):
        self.thisptr.setTxMuted(enabled)

    @property
    def tx_muted(self):
        return self.thisptr.isTxMuted()

    def test_microphone_level(self, double duration_sec=1.0):
        return self.thisptr.testMicrophoneLevel(duration_sec)

    def start(self, list destinations):
        cdef vector[string] cpp_dest
        for dest in destinations:
            cpp_dest.push_back(dest.encode("utf-8"))
        return self.thisptr.start(cpp_dest)

    def update_destinations(self, list destinations):
        cdef vector[string] cpp_dest
        for dest in destinations:
            cpp_dest.push_back(dest.encode("utf-8"))
        return self.thisptr.updateDestinations(cpp_dest)

    def stop(self):
        self.thisptr.stop()

    def shutdown(self):
        self.thisptr.shutdown()

    @property
    def is_running(self):
        return self.thisptr.isRunning()

    @property
    def capture_level(self):
        return self.thisptr.captureLevel()

    @property
    def capture_active(self):
        return self.thisptr.captureActive()

    @property
    def mixed_peak(self):
        return self.thisptr.mixedPeak()

    def set_hear_targets(self, set targets):
        cdef unordered_set[string] cpp_set
        for t in targets:
            if isinstance(t, str):
                cpp_set.insert(t.encode("utf-8"))
        self.thisptr.setHearTargets(cpp_set)

cdef class PyPeerDiscovery:
    cdef PeerDiscovery* thisptr

    def __cinit__(self):
        self.thisptr = new PeerDiscovery()

    def __dealloc__(self):
        del self.thisptr

    def start(self, str my_id, uint16_t audio_port, str room_name="main"):
        self.thisptr.start(my_id.encode("utf-8"), audio_port, room_name.encode("utf-8"))

    def stop(self):
        self.thisptr.stop()

    def peers(self):
        cdef vector[PeerInfo] ps = self.thisptr.peers()
        result = []
        for i in range(ps.size()):
            p = ps[i]
            result.append({
                "id": p.id.decode("utf-8", errors="replace"),
                "ip": p.ip.decode("utf-8", errors="replace"),
                "port": p.port,
                "room": p.room.decode("utf-8", errors="replace"),
                "is_local": True if p.is_local else False,
            })
        return result

    def current_room(self):
        return self.thisptr.currentRoom().decode("utf-8", errors="replace")
