import socket
import threading
import time
from dataclasses import dataclass
from typing import List

from python import PyAudioEngine


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


@dataclass(frozen=True)
class SenderSpec:
    sender_id: str
    start_seq: int
    every_ms: int
    burst: int


def _sender_thread(sock: socket.socket, addr, sodium, key: bytes, spec: SenderSpec, stop_evt: threading.Event) -> None:
    seq = spec.start_seq
    ts = 0
    payload = b""  # empty => Opus PLC decode path in receiver (decode(nullptr,0))
    sleep_s = spec.every_ms / 1000.0
    while not stop_evt.is_set():
        for _ in range(spec.burst):
            pkt = build_client_audio_packet(sodium, key, spec.sender_id, seq, ts, payload)
            sock.sendto(pkt, addr)
            seq = (seq + 1) & 0xFFFF
            ts = (ts + 960) & 0xFFFFFFFF
        time.sleep(sleep_s)


def main() -> int:
    sodium = _load_sodium()

    engine = PyAudioEngine()
    engine.set_client_id("stress-receiver")
    room = "stress-room"
    engine.set_room_secret(room)
    key = _derive_key(sodium, room)

    senders: List[SenderSpec] = [
        SenderSpec("c_fast_1", 100, every_ms=5, burst=2),
        SenderSpec("c_fast_2", 200, every_ms=5, burst=2),
        SenderSpec("c_slow", 300, every_ms=40, burst=1),
    ]
    engine.set_hear_targets({s.sender_id for s in senders})

    addr = ("127.0.0.1", engine.port())
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    stop_evt = threading.Event()
    threads = [
        threading.Thread(target=_sender_thread, args=(sock, addr, sodium, key, spec, stop_evt), daemon=True)
        for spec in senders
    ]
    for t in threads:
        t.start()

    t0 = time.time()
    try:
        while time.time() - t0 < 5.0:
            time.sleep(0.2)
    finally:
        stop_evt.set()
        for t in threads:
            t.join(timeout=1.0)
        sock.close()
        engine.shutdown()

    print("RX/PLC stress test completed (no hang).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

