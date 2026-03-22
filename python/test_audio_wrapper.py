from python import PyAudioEngine, PyPeerDiscovery


def main():
    engine = PyAudioEngine()
    print(f"Port: {engine.port}")
    print(f"Input devices (first 5): {engine.list_input_devices()[:5]}")

    engine.set_client_id("test-client")
    print(f"Client ID roundtrip: {engine.client_id}")

    disc = PyPeerDiscovery()
    disc.start("test-client", engine.port)
    print(f"Peers: {disc.peers()}")
    disc.stop()


if __name__ == "__main__":
    main()
