import argparse
import math
import multiprocessing as mp
import socket
import threading
import time
from dataclasses import dataclass
from typing import List

from python import PyAudioEngine, PyOpusEncoder


SALT = b"NuummiteLANVoice2026SecureSalt"
NONCE_BYTES = 24
MAC_BYTES = 16


def _load_sodium():
    import ctypes

    candidates = [
        "libsodium.dll",
        r"third_party\libsodium\libsodium.dll",
        r"third_party\libsodium\bin\libsodium-26.dll",
    ]
    dll = None
    for name in candidates:
        try:
            dll = ctypes.WinDLL(name)
            break
        except OSError:
            continue
    if dll is None:
        raise RuntimeError("Failed to load libsodium.dll (check third_party/libsodium)")

    dll.sodium_init.argtypes = []
    dll.sodium_init.restype = ctypes.c_int
    if dll.sodium_init() < 0:
        raise RuntimeError("sodium_init failed")

    dll.crypto_generichash.argtypes = [
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.c_void_p,
        ctypes.c_ulonglong,
        ctypes.c_void_p,
        ctypes.c_size_t,
    ]
    dll.crypto_generichash.restype = ctypes.c_int

    dll.randombytes_buf.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    dll.randombytes_buf.restype = None

    dll.crypto_secretbox_easy.argtypes = [
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_ulonglong,
        ctypes.c_void_p,
        ctypes.c_void_p,
    ]
    dll.crypto_secretbox_easy.restype = ctypes.c_int

    return dll


def _derive_key(sodium, room: str) -> bytes:
    import ctypes

    msg = room.encode("utf-8") + SALT
    out1 = (ctypes.c_ubyte * 32)()
    rc = sodium.crypto_generichash(out1, 32, msg, len(msg), None, 0)
    if rc != 0:
        raise RuntimeError("crypto_generichash (pass 1) failed")

    out2 = (ctypes.c_ubyte * 32)()
    rc = sodium.crypto_generichash(out2, 32, out1, 32, None, 0)
    if rc != 0:
        raise RuntimeError("crypto_generichash (pass 2) failed")

    return bytes(out2)


def _encrypt_secretbox(sodium, key: bytes, plaintext: bytes) -> bytes:
    import ctypes

    nonce = (ctypes.c_ubyte * NONCE_BYTES)()
    sodium.randombytes_buf(nonce, NONCE_BYTES)

    cipher = (ctypes.c_ubyte * (len(plaintext) + MAC_BYTES))()
    rc = sodium.crypto_secretbox_easy(cipher, plaintext, len(plaintext), nonce, key)
    if rc != 0:
        raise RuntimeError("crypto_secretbox_easy failed")

    return bytes(nonce) + bytes(cipher)


def build_client_audio_packet(sodium, key: bytes, sender_id: str, seq: int, timestamp: int, payload: bytes) -> bytes:
    header = f"{sender_id}|{seq}|{timestamp}".encode("utf-8") + b":" + payload
    return _encrypt_secretbox(sodium, key, header)


def tone_frame(frame_size: int, rate: int, freq_hz: float, phase: float, amp: float = 0.2) -> bytes:
    out = bytearray(frame_size * 2)
    for i in range(frame_size):
        s = amp * math.sin(2.0 * math.pi * freq_hz * (i / rate) + phase)
        v = int(max(-1.0, min(1.0, s)) * 32767.0)
        out[2 * i] = v & 0xFF
        out[2 * i + 1] = (v >> 8) & 0xFF
    return bytes(out)


@dataclass(frozen=True)
class ClientSpec:
    client_id: str
    freq_hz: float
    loss_percent: float = 0.0
    slow_every_n: int = 0  # if >0, sleep extra every N packets
    slow_ms: int = 0


def _thread_worker(
    sock: socket.socket,
    addr,
    sodium,
    key: bytes,
    spec: ClientSpec,
    stop_evt: threading.Event,
    *,
    rate: int = 48000,
    frame: int = 960,
) -> None:
    import random

    enc = PyOpusEncoder(rate=rate, channels=1, frame_size=frame, bitrate=24000, complexity=10, fec=True, packet_loss_perc=10, dtx=False)
    seq = 0
    ts = 0
    pkt_count = 0
    sleep_s = frame / rate
    pcm0 = tone_frame(frame, rate, spec.freq_hz, 0.0)

    while not stop_evt.is_set():
        pkt_count += 1

        if spec.loss_percent > 0.0 and (random.random() * 100.0) < spec.loss_percent:
            seq = (seq + 1) & 0xFFFF
            ts = (ts + frame) & 0xFFFFFFFF
            time.sleep(sleep_s)
            continue

        payload = enc.encode(pcm0)
        pkt = build_client_audio_packet(sodium, key, spec.client_id, seq, ts, payload)
        sock.sendto(pkt, addr)

        seq = (seq + 1) & 0xFFFF
        ts = (ts + frame) & 0xFFFFFFFF

        if spec.slow_every_n and (pkt_count % spec.slow_every_n) == 0 and spec.slow_ms > 0:
            time.sleep(spec.slow_ms / 1000.0)

        time.sleep(sleep_s)


def _process_worker(
    host: str,
    port: int,
    room: str,
    clients: List[ClientSpec],
    duration_s: float,
    stop_evt: "mp.Event",
    *,
    rate: int = 48000,
    frame: int = 960,
) -> None:
    import random

    # Import triggers python/__init__.py DLL search-path setup in the child.
    from python import PyOpusEncoder as _PyOpusEncoder  # noqa: F401

    sodium = _load_sodium()
    key = _derive_key(sodium, room)

    addr = (host, port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    encoders = []
    pcm_frames = []
    seqs = []
    tss = []
    pkt_counts = []
    sleep_s = frame / rate
    for spec in clients:
        encoders.append(PyOpusEncoder(rate=rate, channels=1, frame_size=frame, bitrate=24000, complexity=10, fec=True, packet_loss_perc=10, dtx=False))
        pcm_frames.append(tone_frame(frame, rate, spec.freq_hz, 0.0))
        seqs.append(0)
        tss.append(0)
        pkt_counts.append(0)

    deadline = time.time() + duration_s
    while not stop_evt.is_set() and time.time() < deadline:
        for idx, spec in enumerate(clients):
            pkt_counts[idx] += 1

            if spec.loss_percent > 0.0 and (random.random() * 100.0) < spec.loss_percent:
                seqs[idx] = (seqs[idx] + 1) & 0xFFFF
                tss[idx] = (tss[idx] + frame) & 0xFFFFFFFF
                continue

            payload = encoders[idx].encode(pcm_frames[idx])
            pkt = build_client_audio_packet(sodium, key, spec.client_id, seqs[idx], tss[idx], payload)
            sock.sendto(pkt, addr)

            seqs[idx] = (seqs[idx] + 1) & 0xFFFF
            tss[idx] = (tss[idx] + frame) & 0xFFFFFFFF

            if spec.slow_every_n and (pkt_counts[idx] % spec.slow_every_n) == 0 and spec.slow_ms > 0:
                time.sleep(spec.slow_ms / 1000.0)

        time.sleep(sleep_s)

    sock.close()


def run_stress(
    num_clients: int,
    duration_s: float,
    *,
    loss_percent: float = 0.0,
    slow_one: bool = True,
    debug_audio: bool = False,
    processes: int = 0,
) -> None:
    if debug_audio:
        import os

        os.environ["NUUMMITE_AUDIO_DEBUG"] = "1"

    sodium = _load_sodium()

    engine = PyAudioEngine()
    engine.set_client_id("stress-receiver")
    room = "stress-room"
    engine.set_room_secret(room)
    key = _derive_key(sodium, room)

    clients: List[ClientSpec] = []
    base = 220.0
    for i in range(num_clients):
        clients.append(ClientSpec(client_id=f"c{i:04d}", freq_hz=base + (i % 48) * 5.0, loss_percent=loss_percent))
    if slow_one and clients:
        clients[0] = ClientSpec(client_id=clients[0].client_id, freq_hz=clients[0].freq_hz, loss_percent=loss_percent, slow_every_n=10, slow_ms=50)

    engine.set_hear_targets({c.client_id for c in clients})

    addr = ("127.0.0.1", engine.port())
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    if processes and processes > 0:
        sock.close()
        stop_evt = mp.Event()
        chunks: List[List[ClientSpec]] = [[] for _ in range(processes)]
        for i, spec in enumerate(clients):
            chunks[i % processes].append(spec)
        procs = [
            mp.Process(
                target=_process_worker,
                args=("127.0.0.1", engine.port(), room, chunk, duration_s, stop_evt),
                daemon=True,
            )
            for chunk in chunks
            if chunk
        ]
        for p in procs:
            p.start()
    else:
        stop_evt = threading.Event()
        threads = [
            threading.Thread(target=_thread_worker, args=(sock, addr, sodium, key, spec, stop_evt), daemon=True)
            for spec in clients
        ]
        for t in threads:
            t.start()

    t0 = time.time()
    try:
        while time.time() - t0 < duration_s:
            time.sleep(0.25)
    finally:
        stop_evt.set()
        if processes and processes > 0:
            for p in procs:
                p.join(timeout=2.0)
        else:
            for t in threads:
                t.join(timeout=1.0)
            sock.close()
        engine.shutdown()


def main() -> int:
    ap = argparse.ArgumentParser(description="Nuummite UDP client stress sender.")
    ap.add_argument("--clients", type=int, default=50)
    ap.add_argument("--seconds", type=float, default=10.0)
    ap.add_argument("--loss", type=float, default=10.0, help="Packet loss percent per client.")
    ap.add_argument("--no-slow-one", action="store_true", help="Disable one intentionally slow sender.")
    ap.add_argument("--debug-audio", action="store_true", help="Enable NUUMMITE_AUDIO_DEBUG for engine logs.")
    ap.add_argument("--processes", type=int, default=0, help="Use N sender processes instead of threads.")
    args = ap.parse_args()

    run_stress(
        num_clients=args.clients,
        duration_s=args.seconds,
        loss_percent=args.loss,
        slow_one=not args.no_slow_one,
        debug_audio=args.debug_audio,
        processes=args.processes,
    )
    print("UDP client stress run complete.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

