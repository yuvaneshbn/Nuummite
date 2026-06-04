The Nuummite codebase shows an advanced understanding of low-latency systems programming. The audio transmission pipeline operates on a fixed frame duration of $20\text{ ms}$ at a standard $48\text{ kHz}$ sample rate :$$F = 960\text{ samples}$$$$R = 48000\text{ Hz}$$$$T_{\text{frame}} = \frac{F}{R} = \frac{960}{48000\text{ Hz}} = 0.02\text{ s} = 20\text{ ms}$$At $20\text{ ms}$, the engine balances low network overhead with real-time feedback. The use of a lock-free Single-Producer Single-Consumer (SPSC) ring buffer with std::atomic acquire-release memory order barriers ensures that real-time audio threads are never blocked by network operations or GUI updates, preventing audio glitches.Capturing Thread (MME waveIn Callback)
|-- Push capture frame (960 samples @ 20ms)
|-- Write to lock-free SPSC Ring Buffer (capture_frames_)
v
Transmission Thread (AudioEngine::sendLoop)
|-- Wait on capture semaphore
|-- Pop frame -> RNNoise / WebRTC APM -> Opus Encode
|-- Encrypt -> Send UDP payload

Nuummite: Decentralized LAN P2P Voice Communication Protocol and Windows Client Specification
The Nuummite platform represents a fully decentralized, serverless peer-to-peer (P2P) voice communication architecture engineered specifically for low-latency, full-duplex operation within local area network (LAN) environments.1 By eliminating the requirement for a centralized signaling or media routing server, the system mitigates single-point-of-failure vulnerabilities, minimizes routing latency, and guarantees communication survivability on isolated networks.1 All critical system tasks—including dynamic peer discovery, cryptographic key derivation, multi-stage digital signal processing (DSP), network packet prioritization, and adaptive jitter buffer management—are processed entirely on edge client nodes.1
The primary target platform is Windows 10 and 11 (x64), utilizing a hybrid runtime environment composed of a high-performance, real-time C++ audio core coupled with a graphical user interface (GUI) developed in Qt6.1
Architectural Foundation and Threading Model
The runtime execution environment of the client is split across decoupled thread domains to prevent GUI rendering lags or user interactions from blocking the high-priority, real-time media pipeline.1 This thread isolation is a prerequisite for maintaining steady streaming, avoiding hardware audio driver underruns, and minimizing latency.1

Subsystem Module
Responsible Files
External Dependencies
Execution Thread Domain
Runtime Resolution Mechanism
Graphical User Interface
MainWindow.cpp, SettingsDialog.cpp, VolumeControlPanel.cpp, main.cpp 1
Qt6 (Core, Gui, Widgets, Network, UiTools) 1
Main Thread (Qt Event Loop) 1
Static import library linkage at compile time 1
Peer Discovery Protocol
peer_discovery.cpp, peer_discovery.h 1
Winsock2, IPHlpApi (GetAdaptersAddresses) 1
Dedicated Discovery Thread 1
Static linkage to ws2_32.lib and iphlpapi.lib 1
Core Audio Engine
audio_engine.cpp, audio_engine.h 1
PortAudio, Windows Multimedia (winmm.lib) 1
Hardware Audio Driver Callbacks & Capture Transmission Loop 1
Dynamic dynamic-link library (DLL) resolution (libportaudio.dll) 1
Acoustic Preprocessor
aec_processor.cpp, webrtc_apm.cpp, rnnoise_processor.cpp 1
WebRTC APM, RNNoise 1
Real-time Transmission Thread (sendLoop) 1
Static import linkage (rnnoise.lib, webrtc-audio-processing-1-msvc.lib) 1
Cryptographic Shield
libsodium_wrapper.cpp, audio_packet.cpp 1
Libsodium 1
Main GUI (Key Derivation) & Real-time RX/TX Threads 1
Dynamic dynamic-link library (DLL) resolution (libsodium.dll) 1

Thread COM Apartment Configuration
Because real-time Windows audio drivers (WASAPI) and system APIs require a stable Component Object Model (COM) apartment model, the main application thread explicitly configures a Multithreaded Apartment (MTA) state.1 This configuration is executed on startup before initializing any Qt6 graphic contexts or PortAudio resources 1:

## C++

const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

Setting the thread apartment to MTA ensures that the callback interfaces exposed by WASAPI and direct system-level bindings do not encounter thread serialization delays or message pump re-entrancy bugs common in Single-Threaded Apartments (STA).1
Lock-Free Thread Interconnection
To transfer digitized audio frames between the hardware driver execution context (callback thread) and the processing transmission context (sendLoop thread), a lock-free, single-producer single-consumer circular queue is implemented.1

Hardware Capture Callback Thread             Real-time Transmission Thread (sendLoop)
+----------------------------------+          +-----------------------------------------+
| - Stereo to Mono Downmixing      |          | - Semaphore wait (non-blocking)         |
| - Format scaling & clamping      |          | - Pop Mono PCM capture block            |
| - Lock-free push into SPSC ring  | -------> | - Preprocess (WebRTC APM + RNNoise)     |
| - Signal semaphore               |          | - Compress via Opus & encrypt packet    |
+----------------------------------+          +-----------------------------------------+

The queue utilizes atomic memory order constraints to prevent compiler reordering and processor cache latency 1:

## C++

template <typename T, size_t Capacity>
class LockFreeSpscRingBuffer {
// SPSC ring buffer using cacheline-aligned atomic heads and tails
alignas(64) std::atomic<size_t> head_{0};
alignas(64) std::atomic<size_t> tail_{0};
};

The application of the alignas(64) constraint to the atomic read and write pointers prevents false sharing on modern CPU architectures.1 By forcing the head and tail indices onto separate processor cache lines (typically $64$ bytes), cache invalidation cycles are minimized, allowing the hardware driver to offload audio buffers with sub-microsecond latency.1
Thread Synchronization via Atomic Snapshots
When the GUI thread requires updates regarding peer status (such as signal levels, audio peaks, or connection state), taking locks on the real-time processing threads can introduce micro-stuttering or buffer dropouts.1 To bypass this lock contention, the audio engine implements a double-buffered snapshot mechanism.1 Two peer-tracking vectors (stream_snapshot_buffers_) are allocated.1 The active buffer index is tracked using an atomic pointer 1:

## C++

std::atomic<uint8_t> stream_snapshot_active_{0};
std::array<std::atomic<uint32_t>, 2> stream_snapshot_readers_{{0u, 0u}};
std::array<std::vector<StreamState*>, 2> stream_snapshot_buffers_{};

When the GUI thread triggers a list refresh, it updates the inactive buffer segment.1 It then performs an atomic swap of the active index.1 The real-time thread accesses the active snapshot via a spin-wait guard containing processor hints to minimize CPU instruction cycles 1:

## C++

while (stream_snapshot_readers_[write].load(std::memory_order_acquire)!= 0) {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
_mm_pause(); // Emits hardware instruction to reduce core power and latency
#endif
std::this_thread::yield();
}

Dynamic System Initialization and Runtime Bootstrap Sequence
The initialization protocol of the client is organized into discrete operational phases to verify external runtimes, initialize network stacks, and establish local encryption contexts.1

+---------------------------------------------------------------------------------+
| Phase 1: Initialize System APIs (WSAStartup, CoInitializeEx, Load Libraries)    |
+-------------------------------------------------+-------------------------------+
|
v
+-------------------------------------------------+-------------------------------+
| Phase 2: Render Authentication Dialog (Input Client Name & Private Room ID)     |
+-------------------------------------------------+-------------------------------+
|
v
+-------------------------------------------------+-------------------------------+
| Phase 3: Symmetric Key Derivation (Argon2id Stretch / Fallback BLAKE2b Hash)     |
+-------------------------------------------------+-------------------------------+
|
v
+-------------------------------------------------+-------------------------------+
| Phase 4: Sockets & Transport Bindings (SO_EXCLUSIVEADDRUSE, SIO_UDP_CONNRESET)  |
+-------------------------------------------------+-------------------------------+
|
v
+-------------------------------------------------+-------------------------------+
| Phase 5: Thread Spawning & Port Discovery Broadcast Group Membership Joining     |
+---------------------------------------------------------------------------------+

Phase 1: Core System Initialization
On application entry, the Windows Sockets (Winsock v2.2) assembly is loaded via a dedicated wrapper instance (WinSockInit), ensuring the underlying network drivers are ready.1 Simultaneously, COM interfaces are established.1 The system then resolves the locations of external DLL dependencies.1 The application checks directory handles sequentially, resolving the executable root, packaging roots (_MEIPASS), and system folders using LOAD_LIBRARY_SEARCH_APPLICATION_DIR and LOAD_LIBRARY_SEARCH_SYSTEM32 system flags to resist library injection 1:

## C++

HMODULE SecureLoadPortAudioLibraryW(const std::wstring& full_path) {
DWORD flags = LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS;
if (has_dir) flags |= LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR;
HMODULE h_module = LoadLibraryExW(full_path.c_str(), nullptr, flags);
if (!h_module) h_module = LoadLibraryW(full_path.c_str());
return h_module;
}

Phase 2: Graphic Presentation Setup
The user interface launches a Modal Dialog box requesting client identity credentials, a targeted room designation identifier, and a private cryptographic passphrase.1 The dialog measures local interface configurations to present the host's active local IP address, verifying physical network adapter connectivity.1
Phase 3: Cryptographic Key Derivation (KDF)
The user-supplied passphrase is converted into a key via Libsodium's password hashing and key derivation APIs.1 The default path calls crypto_pwhash (Argon2id), configuring high memory bounds and processing complexity to safeguard the key against local and GPU hardware brute-force attacks.1 The derivation uses a fixed salt value 1:

## C++

static const unsigned char salt = {
'N','u','u','m','m','i','t','e','V','o','i','c','e','K','D','F'
};

If the system environment lacks native Argon2id exports, the KDF falls back to a BLAKE2b hashing procedure (crypto_generichash) using the domain prefix Nuummite:secretbox:key:v1 to safely stretch the passphrase.1
Phase 4: Non-Blocking Socket Allocation
The network transport layer allocates an ephemeral UDP socket (recv_sock_).1 System security is hardened by applying SO_EXCLUSIVEADDRUSE to prevent local port-hijacking.1 Additionally, the Winsock SIO_UDP_CONNRESET behavior is disabled.1 This prevents ICMP Port Unreachable packets from disrupting socket operations when peers abruptly drop off the network 1:

## C++

BOOL new_behavior = FALSE;
DWORD bytes_returned = 0;
WSAIoctl(sock, SIO_UDP_CONNRESET, &new_behavior, sizeof(new_behavior), nullptr, 0, &bytes_returned, nullptr, nullptr);

Phase 5: Processing Thread Generation
Once the sockets are bound, the system spawns independent listening threads.1 The AudioEngine::listenLoop thread monitors incoming media payloads on the ephemeral port.1 Simultaneously, the PeerDiscovery thread initiates peer detection broadcasts on local subnets.1

Operational Phase
Executed Windows/Sodium APIs
Passed Parameter Flags
Produced Handle Result
Target System State
Winsock & COM Init
WSAStartup, CoInitializeEx 1

## Coinit_multithreaded 1

WSADATA structure 1
System network socket interface and COM STA-to-MTA transitioned 1
Dynamic DLL Mapping
LoadLibraryExW, GetProcAddress 1

## Load_library_search_system32 1

HMODULE pointers for PortAudio & Sodium 1
Run-time pointers resolved; dynamic linkages bound safely 1
Key Derivation
crypto_pwhash / crypto_generichash 1
Argon2id default interactive memory limits 1
$256$-bit atomic thread-safe symmetric key 1
Symmetric encryption key established across localized context 1
Transport Binding
socket, setsockopt, WSAIoctl 1

## So_exclusiveaddruse, sio_udp_connreset 1

Non-blocking SOCKET recv_sock_ 1
Local interface socket configured; ICMP error handling disabled 1
Thread Spawning
std::thread, bind 1
System-level priority structures
Multi-thread execution handles
Peer discovery loops and packet-receiver engines active 1

Peer Discovery Protocol and LAN Signaling Specification
The client implements a decentralized, zero-configuration peer discovery protocol operating over UDP port $50000$.1 It manages localized subnet advertising, peer validation, and thread-safe UI updates.1

(GetAdaptersAddresses on IPv4 Only)
|
v

(Port 50000 / IP 239.255.0.1)
|
v
+---------------+---------------+
|                               |
v                               v

"VOICE_PEER:ID:Room:Port"         (Non-blocking 250ms Poll)
|                               |
|                               v
|                    [Peer Validation Filter]
|                   - Prune Stale (> 3500ms)
|                   - Check Local Interface
|                   - Remap Loopback (127.0.0.1)
|                               |
+---------------+---------------+
|
v

(O(1) Lock-free atomic swap)

Local Network Discovery and Multicast Configuration
Upon execution of the signaling thread, a local system interface evaluation is performed.1 By calling GetAdaptersAddresses, the system identifies active IPv4 interfaces.1 The discovery socket is bound to the discovery port using SO_REUSEADDR 1:

## C++

const int reuse = 1;
setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

This enables multiple instances of the application to execute on a single physical workstation, resolving port bind collisions.1 The discovery socket then joins the standardized IPv4 multicast group address 239.255.0.1 and enables multicast loopback to support localized multi-client testing 1:

## C++

ip_mreq mreq{};
inet_pton(AF_INET, "239.255.0.1", &mreq.imr_multiaddr);
mreq.imr_interface.s_addr = htonl(INADDR_ANY);
setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));

Subnet Beacons and Parse Validation
Every $1000\text{ ms}$, the discovery loop formats and transmits an unencrypted UDP beacon string to the multicast group 1:

$$\text{BeaconString} = \text{"VOICE\_PEER:"} \mathbin{\Vert} \text{client\_id} \mathbin{\Vert} \text{":"} \mathbin{\Vert} \text{room\_name} \mathbin{\Vert} \text{":"} \mathbin{\Vert} \text{audio\_port}$$
The receive path polls the multicast socket using non-blocking select() calls with a $250\text{ ms}$ timeout.1 When data is detected, it is extracted into a stack-allocated buffer 1:

## C++

char buffer = {0};
int recv_len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr*>(&src), &src_len);

Stack allocation avoids heap fragmentation in long-running runtime sessions.1 The received packet is validated by checking for the VOICE_PEER: prefix and matching the room string with the host's active room configuration.1
To support multiple instances running on the same host, the source IP of the sender is checked against local network interfaces using GetAdaptersAddresses.1 If the packet originates from the host itself, the source IP is remapped to loopback ($127.0.0.1$) and flagged as local (is_local = true), routing the audio streams correctly.1
If a peer does not send a beacon for $3500\text{ ms}$, the discovery engine prunes the entry from its tracking database.1 This automatic cleanup maintains an accurate view of active participants in the room.1
Real-Time Audio Transmission (TX) Pipeline
The transmission pipeline digitizes local microphone signals, applies noise reduction and echo cancellation, compresses the audio, encrypts the stream, and sends the packets over the P2P network.1

+---------------------------+
|  Physical Mic / Hardware  |
+-------------+-------------+
| (Stereo)
v
+-------------+-------------+
| Stereo-to-Mono Downmixing |
+-------------+-------------+
| (Mono, 48 kHz, 20ms)
v
+-------------+-------------+
|  Lock-Free Ring Buffer    |
+-------------+-------------+
| (TX Thread pop)
v
+-------------+-------------+
|       WebRTC APM          | <--- (AEC, HPF, AGC)
+-------------+-------------+
|
v
+-------------+-------------+
|  RNNoise Denoise Filter   |
+-------------+-------------+
|
v
+-------------+-------------+
|  Software Gain & VAD/Sens |
+-------------+-------------+
| (Active speech block)
v
+-------------+-------------+
|  Opus Voice Compression   |
+-------------+-------------+
| (Opus payload)
v
+-------------+-------------+
| Libsodium XSalsa20-Poly1305|
+-------------+-------------+
| (Encrypted Packet)
v
+-------------+-------------+
| UDP Socket TX (DSCP EF)   |
+---------------------------+

Phase 1: Capture and Stereo-to-Mono Downmixing
The system captures audio at $48\text{ kHz}$ with a $20\text{ ms}$ frame size ($960$ samples).1 When dual-channel input is detected, the audio thread downmixes the interleaved stereo signal to mono before processing 1:

$$\text{MixedSample}[i] = \text{clamp}\left( \frac{\text{Left}[i] + \text{Right}[i]}{2}, -32768, 32767 \right)$$
The mono samples are pushed into a lock-free single-producer single-consumer (SPSC) ring buffer.1
Phase 2: WebRTC Acoustic Echo Cancellation (AEC)
The transmission thread retrieves the raw frame and forwards it to the WebRTC Audio Processing Module (APM).1 The high-pass filter within the WebRTC APM attenuates low-frequency noise (sub-$100\text{ Hz}$ room rumble).1
AEC coordinates with the playback rendering pipeline using a far-end echo reference loop 1:

## C++

if (echo_enabled_.load()) {
std::unique_lock<std::mutex> lock(echo_mutex_);
aec_->set_stream_delay_ms(aec_stream_delay_ms_.load());
aec_->process_capture(frame);
}

Phase 3: RNNoise Neural Noise Suppression
Because RNNoise is trained on $10\text{ ms}$ blocks ($480$ samples at $48\text{ kHz}$), the pipeline splits the $20\text{ ms}$ frame into two parts, processes them sequentially, and applies an interpolative dry/wet blend based on the user's settings 1:

$$\text{WetSample}[i] = (1 - \alpha) \cdot \text{Original}[i] + \alpha \cdot \text{Denoised}[i]$$
Here, $\alpha$ represents the blending ratio (configured from $0.0$ to $1.0$).1
Phase 4: Software Gain and Peak-Level Measurement
If WebRTC AGC is disabled, manual gain scaling is applied 1:

$$\text{ScaledSample} = \text{RawSample} \cdot \left( 10^{\frac{\text{tx\_gain\_db}}{20}} \cdot \frac{\text{mic\_sensitivity}}{50} \right)$$
The peak absolute sample value is calculated to update the GUI's input level meter.1
Phase 5: Voice Activity Detection (VAD) and Hangover Protection
The transmission thread determines if active speech is present to avoid flooding the local network with silence.1 If WebRTC VAD is inactive, the engine falls back to evaluating peak values against a dynamic threshold based on the user's microphone sensitivity 1:

$$\text{VAD\_Threshold} = \text{clamp}(260 - (\text{sensitivity} \cdot 2), 60, 220)$$
If the peak value exceeds this limit, voice activity is marked as active and the system resets a hangover frame counter ($kVoiceHangoverFrames = 18$).1 The hangover mechanism ensures that brief pauses between words (up to $360\text{ms}$) do not cause choppy audio cuts during transmission.1
Phase 6: Opus Encoding and Encryption
Active frames are compressed using the Opus codec (VBR mode, VOIP application type, complexity level $10$, and in-band Forward Error Correction [FEC] enabled).1 The resulting Opus payload is sent to the encryption layer, which wraps it into a secure packet.1
The plaintext packet layout contains a metadata header (sender ID, sequence number, and timestamp) concatenated with the Opus payload.1 The system uses Libsodium's secretbox_easy (XSalsa20-Poly1305) to encrypt the block.1 A $24$-byte cryptographically secure random nonce is generated for each packet to ensure that identical payloads yield completely different ciphertexts, preventing replay and pattern analysis attacks.1 The random nonce is prepended directly to the ciphertext 1:

$$\text{Packet} = \text{Nonce (24 bytes)} \mathbin{\Vert} \text{Ciphertext}$$
Phase 7: Network Serialization and QoS Mapping
The final packet is sent to all registered peer endpoints using the UDP socket.1 To prioritize voice traffic over standard LAN data, QoS markings are applied to the IP header.1 The socket's Type of Service (TOS) field is updated to DSCP Expedited Forwarding (EF, value $46$, which maps to TOS octet 0xB8), signaling local routers to minimize latency and packet drops for these streams.1
Real-Time Audio Receive (RX) and Mixing Pipeline
The receiving pipeline processes incoming packets, decrypts the payloads, manages the jitter buffer, mixes the streams, and routes the final audio to the playback device.1

+---------------------------+
|   Incoming UDP Packet     |
+-------------+-------------+
|
v
+-------------+-------------+
|  Decryption Verification  |
+-------------+-------------+
| (Decrypted Plaintext)
v
+-------------+-------------+
|   Jitter Buffer Sorting   | <--- Adaptive Sizing & Loss Window
+-------------+-------------+
|
v
+-------------+-------------+
|   Opus Decoding & PLC     | <--- Triggers PLC if frame lost
+-------------+-------------+
|
v
+-------------+-------------+
| Lock-Free Stream Snapshot |
+-------------+-------------+
|  Mono PCM Mixing Matrix   |
+-------------+-------------+
|
v
+-------------+-------------+
| Normalized Soft Clipping  | <--- Clamps sum post pre-scale
+-------------+-------------+
|
v
+-------------+-------------+
|   WebRTC APM Echo Loop    |
+-------------+-------------+
|
v
+-------------+-------------+
| Master/Output Gain Scaler |
+-------------+-------------+
|
v
+-------------+-------------+
| Mono-to-Stereo Duplicator |
+-------------+-------------+
|
v
+-------------+-------------+
| PortAudio WASAPI Output   |
+---------------------------+

Packet Reception, Security, and Stream Sorting
The listenLoop socket listener monitors the ephemeral UDP receive port via non-blocking WSAPoll() configurations with $100\text{ ms}$ sleep limits.1 When data is detected, it is extracted and processed 1:

## C++

int recv_len = recvfrom(recv_sock_, buffer, 4096, 0, &src, &src_len);

The incoming buffer is split.1 The first $24$ bytes are treated as the random nonce, and the remaining bytes are processed by Libsodium's symmetric decryption engine 1:

## C++

bool decrypt_ok = SodiumWrapper::decrypt(cipher, cipher_len, nonce, 24, plain);

If decryption fails, the packet is discarded immediately.1 If successful, the plaintext header is parsed.1 The system extracts the sender's identifier, sequence key, and timestamp, and locates the corresponding StreamState buffer 1:

## C++

if (view.rfind("MIXED|", 0) == 0) {... } // Handles specialized mixing routing

The parsed audio payload is placed in the peer's jitter buffer to await processing.1
Dynamic Jitter Buffer Management
Each peer stream is managed by an independent JitterBuffer instance designed to smooth out packet arrival timing variations over the local network.1

Operational State
Monitored Trigger Condition
Core Engine Action
System Impact
Buffering
Target slot count not yet reached 1
Holds playout; queues frames 1
Introduces initial delay to build safety margin 1
Normal Playout
Requested frame matches expected sequence 1
Pops payload; decodes normally 1
Delivers low-latency, glitch-free audio 1
Loss Event
Requested frame missing from the queue 1
Calls Opus decoder with null pointer 1
Activates Packet Loss Concealment (PLC) 1
Sequence Reset
Sequence drift exceeds window boundaries 1
Clears buffer; resynchronizes sequence 1
Quickly recovers from extended network drops 1
Target Sizing
Sliding loss window tracking ($50$ packets) 1
Adjusts target size (between $2$ and $6$) 1
Dynamically balances latency against packet loss 1

The jitter buffer allocates a circular array of $64$ slots.1 When a packet arrives, its sequence position is evaluated.1 If it is late (falling outside the current tracking window), it is discarded.1
During playout, the hardware thread requests the expected sequence frame.1 If missing, it calls the Opus decoder with nullptr to trigger Packet Loss Concealment (PLC), which reconstructs the missing audio segment.1
The system tracks packet loss over a $50$-packet window.1 If the loss rate exceeds $10\%$, the target buffer size is increased (up to $6$ frames).1 If the loss rate drops below $2\%$, the target is reduced (down to $2$ frames) to minimize latency.1
Synchronized Stream Mixing and Soft-Clipping
The hardware playback callback runs the mixing loop, combining decoded streams from all active peers.1 Peer streams are read from the active snapshot vector 1:

## C++

StreamSnapshotGuard guard(this);
for (auto* st : *guard.snapshot) {
std::unique_lock<std::mutex> lock(st->mutex, std::try_to_lock);
if (!lock.owns_lock()) continue; // Skip if locked to prevent blockages
// Decodes the peer payload into a temporary PCM buffer
st->decoder->decode_into(view.data, view.len, pcm_frame, 960);
for (int i = 0; i < 960; ++i) mix_accum_[i] += pcm_frame[i];
}

To prevent digital clipping when combining multiple high-amplitude streams, the system calculates the overall peak of the combined frame 1:

$$\text{peak} = \max_{0 \le i < 960} |\text{MixAccumulator}[i]|$$
If the peak level exceeds $28,000$, a scaling factor is applied to keep the signal within a safe range 1:

$$S_f = \frac{28000.0}{\text{peak}}$$

$$\text{ScaledAccumulator}[i] = \frac{\text{MixAccumulator}[i] \cdot S_f}{32768.0}$$
The normalized signal is run through opus_pcm_soft_clip to round off any remaining transients, and is then scaled back to a $16$-bit signed integer format 1:

$$\text{OutputPCM}[i] = \text{clamp}(\text{Clipped}[i] \cdot 32767.0, -32768.0, 32767.0)$$
Finally, the mixed stream is processed by the WebRTC APM echo canceller and adjusted for volume 1:

$$\text{FinalSample}[i] = \text{OutputPCM}[i] \cdot (\text{master\_volume} \cdot \text{output\_volume})$$
The mono output is duplicated to both channels to ensure correct stereo playback without driver underruns.1
Fallback Audio Driver Architecture
To handle situations where the primary PortAudio/WASAPI interface is unavailable or fails to initialize, the audio engine implements a fallback driver subsystem.1

+---------------------------+
|  Initialize Audio Engine  |
+-------------+-------------+
|
v
+-------------+-------------+
|  Attempt PortAudio/WASAPI |
+-------------+-------------+
|
├──> Succeeded: Stream active
|
└──> Failed / Rejected
|
v
+-------------+-------------+
| Fallback to waveIn Driver |
| - Open WAVE_MAPPER        |
| - Configure 16-bit Mono   |
| - Prepare 4 WAVEHDR slots |
| - Requeue via Callback    |
+---------------------------+

If the primary driver fails, the engine falls back to the classic Windows Multimedia waveIn system.1 The fallback pipeline is configured for mono recording at $48\text{ kHz}$ with $16$-bit resolution 1:

## C++

WAVEFORMATEX format{};
format.wFormatTag = WAVE_FORMAT_PCM;
format.nChannels = 1;
format.nSamplesPerSec = 48000;
format.wBitsPerSample = 16;

The driver allocates four sequential storage blocks.1 Each buffer contains space for exactly $960$ samples ($1920$ bytes) and is registered using independent WAVEHDR structures to prevent driver starvation and ensure continuous recording 1:

## C++

mr = waveInPrepareHeader(handle, &header, sizeof(header));
mr = waveInAddBuffer(handle, &header, sizeof(header));

When a buffer fills, the driver triggers the registered callback waveInCallback.1 The engine processes the captured frame through the standard preprocessing pipeline, and then requeues the buffer to keep the recording cycle running smoothly.1
Build System and Packaging Specifications
The Nuummite project utilizes a modular CMake build system to compile the C++ binaries and bundle all required assets and runtime dependencies on Windows.1
Compiler Prerequisites
Compiler Platform: Visual Studio 2022 (MSVC v143 build toolset) 1
Build Generator: CMake 3.20+ combined with the Ninja build system 1
SDK Integrations: Qt 6.5+ (configured for MSVC x64) 1
Direct Library Dependencies: Opus Codec, Libsodium, RNNoise, and WebRTC APM 1
Compilation Pipeline Execution
The build environment is set up by executing the platform-specific toolchain variables from a visual studio command prompt 1:

## Dos

:: Load Visual Studio build environment variables
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

:: Configure workspace targets via CMake
cmake -S. -B.\build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.5.3/msvc2022_64

:: Compile application binaries
cmake --build.\build-msvc

The CMake configuration organizes compiling into two primary targets: the static library voice_shared (containing the common components, packet parser, and sodium wrappers) and the main client executable voice_client.1
To ensure the compiled binary can locate and load its dependencies, post-build copy rules are defined in CMakeLists.txt 1:

CMake
# CMake commands to automatically deploy required assets and dynamic libraries
add_custom_command(TARGET voice_client POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory
${CMAKE_SOURCE_DIR}/Nuummite/ui
$<TARGET_FILE_DIR:voice_client>/ui
)

The system requires several libraries to be present in the executable's folder at runtime to prevent missing DLL errors 1:

Dependency Binary / Asset
Original Directory
Destination Folder
Deployment Purpose
opus.dll
third_party/opus
Target Output Dir
Opus codec decompression library 1
libportaudio.dll
third_party/libportaudio
Target Output Dir
Low-level hardware audio driver 1
libsodium.dll / libsodium-26.dll
third_party/libsodium
Target Output Dir
Symmetric encryption and hashing algorithms 1
rnnoise.dll
third_party/rnnoise
Target Output Dir
Recurrent neural noise processing library 1
webrtc-audio-processing-1-3.dll
third_party/webrtc_audio_processing/bin
Target Output Dir
WebRTC acoustic echo cancellation engine 1
webrtc-audio-coding-1-3.dll
third_party/webrtc_audio_processing/bin
Target Output Dir
WebRTC core audio processing utility 1
technical-support.ico
Nuummite
Target Output Dir
Main graphical window display icon 1


This ensures that when the application unpacks itself into the temporary directory _MEIPASS at runtime, the audio engine can securely locate and load all required dependencies.1
Diagnostic Verification and Troubleshooting Protocols
The Nuummite codebase includes a command-line utility, audio_flow_test.cpp, designed to verify the performance and stability of the audio processing and network pipelines.1

## C++

// Verification modes within audio_flow_test.cpp
const bool receiver_started = receiver.start({}, false, true); // Playback and mixing only
const bool sender_started = sender.start({receiver_endpoint}, false, false); // Packet generation only

Local Loopback Verification Mode
The diagnostic utility simulates multi-party audio flows over local loopback using a synthetic signal generator to test both transmission and reception 1:

## Dos

:: Run loopback verification test for 4 seconds
.\build-msvc\bin\audio_flow_test.exe --pair --seconds 4 --room test-room --sender-id alpha --receiver-id beta

During the test, the sender node synthesizes a continuous sine wave and injects it into the capture queue.1 The receiver node processes the stream, manages the jitter buffer, decrypts the packets, and measures output signal metrics.1 If the test completes successfully, it confirms the stability of the core engine 1:

t=0.2s tx_active=1 tx_level=36 tx_packets=10 rx_packets=10 rx_decrypted=10 peer_peak=12000 mixed_peak=12000
two-instance audio test result: AUDIO FLOW VERIFIED

Troubleshooting Common Errors

+------------------------------------------+
|           Diagnostic Checklist           |
+--------------------+---------------------+
|
+-------------------------+-------------------------+
|                                                   |
v                                                   v
[Network Errors]
- Check UDP 50000 (Discovery)                       - Check default recording devices
- Check UDP 50002 (Media flow)                      - Adjust Sensitivity (MicSens)
- Allow firewall exceptions                         - Verify input levels in settings
- Verify multicast bindings                         - Enable AEC/RNNoise suppression

Fault Category
Observed Symptom
Underlying Root Cause
Resolution Protocol
Dynamic Load failure
Application crashes with a missing DLL error on launch
The required runtime DLLs are missing from the executable directory 1
Verify that opus.dll, libportaudio.dll, and libsodium.dll are copied to the same directory as the executable.1
Device Missing
PortAudio fails to initialize or listing returns empty
No active default input or output audio device is configured on the host 1
Check the Windows Sound Control Panel to ensure audio hardware is connected and enabled. Confirm microphone access is allowed in Windows Privacy settings.1
Discovery Failure
Peer discovery fails; the participant list remains empty
Local network firewalls are blocking UDP multicast traffic on port $50000$ 1
Run the following PowerShell command to add a firewall exception rule for discovery traffic 1:

New-NetFirewallRule -DisplayName "Nuummite Discovery" -Direction Inbound -Action Allow -Protocol UDP -LocalPort 50000 -Profile Private 1
No Audio Playout
Connected to a room and peers are discovered, but no voice can be heard
Local network firewalls are blocking media traffic on port $50002$ 1
Run the following PowerShell command to add a firewall exception rule for audio stream traffic 1:

New-NetFirewallRule -DisplayName "Nuummite Audio" -Direction Inbound -Action Allow -Protocol UDP -LocalPort 50002 -Profile Private 1
VAD Starvation
Microphone level is captured but no network packets are sent
The voice activity detection threshold is set too high for the incoming signal 1
Adjust the microphone sensitivity slider in the settings panel to a lower value (such as $20$ or $30$) to reduce the VAD threshold.