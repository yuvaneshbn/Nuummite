## Stress Testing Checklist (Audio RX/Mix)

### How to run
- Rebuild extension: `python setup.py build_ext --inplace`
- Run stress sender(s): `python python/udp_client_stress.py`
- Use processes (for higher client counts): `python python/udp_client_stress.py --clients 200 --seconds 30 --loss 0 --processes 8`
- Enable debug logs: set `NUUMMITE_AUDIO_DEBUG=1`

### Metrics to watch
- CPU per core: sustained >80% indicates decode/mix overload.
- Mix latency: time from RX to playback should stay ~1–2 frames (20–40ms @ 48k/20ms frames).
- Per-client queue depth: jitter queues growing beyond ~5 frames indicates overload or lock contention.
- Decode errors/drops: count/print failures from Opus decode or dropped out-of-order packets.
- Mixed peak vs. client count: peak should not grow linearly with N; limiter/soft-clip should keep it bounded.
- UDP socket behavior: with non-blocking sockets enabled, occasional send drops (WOULDBLOCK) are acceptable; sustained drops indicate CPU/network overload.
- Windows UDP: we disable `SIO_UDP_CONNRESET` so stray ICMP "Port Unreachable" doesn’t break `recvfrom()` with `WSAECONNRESET`.

### Thresholds (suggested)
- Queue depth > 5 for multiple clients: investigate contention or scheduling.
- Sudden rise in mixed peak with more clients: limiter/soft-clip regression.
- Frequent PLC bursts (`plc_lost_frames`): network loss or timing issues.
