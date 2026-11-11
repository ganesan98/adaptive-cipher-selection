from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PythonTests(unittest.TestCase):
    def test_key_generation_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as folder:
            key = Path(folder) / "test.key"
            cmd = [sys.executable, str(ROOT / "python/keygen.py"), str(key)]
            result = subprocess.run(cmd, capture_output=True)
            self.assertEqual(result.returncode, 0)
            original = key.read_text()
            self.assertEqual(len(bytes.fromhex(original)), 32)
            self.assertNotEqual(subprocess.run(cmd, capture_output=True).returncode, 0)
            self.assertEqual(key.read_text(), original)

    def test_proxy_forwards_datagrams_and_returns_replies(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
            server.bind(("127.0.0.1", 0))
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as free:
                free.bind(("127.0.0.1", 0))
                port = free.getsockname()[1]
            server.settimeout(0.2)
            client.settimeout(2)
            proc = subprocess.Popen([sys.executable, str(ROOT / "python/proxy.py"), "--listen", str(port),
                                     "--server", str(server.getsockname()[1]), "--phase-seconds", "100"],
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            try:
                for _ in range(20):
                    client.sendto(b"network-test", ("127.0.0.1", port))
                    try:
                        payload, peer = server.recvfrom(4096)
                        break
                    except socket.timeout:
                        self.assertIsNone(proc.poll())
                else:
                    self.fail("Proxy not ready")
                self.assertEqual(payload, b"network-test")
                server.sendto(b"reply-test", peer)
                self.assertEqual(client.recvfrom(4096)[0], b"reply-test")
            finally:
                proc.terminate()
                proc.wait(timeout=5)


if __name__ == "__main__":
    unittest.main()
