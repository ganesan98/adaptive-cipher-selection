"""Exercise actual C++ processes over localhost. Run after building."""
import argparse
import csv
from pathlib import Path
import secrets
import socket
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from demo import executable


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Integration(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.folder = Path(self.temp.name)
        self.key = self.folder / "secret.key"
        self.key.write_text(secrets.token_hex(32), encoding="ascii")
        self.port = free_port()
        self.processes = []
        self.logs = []
        self.start([executable(BUILD, "acs_server"), "127.0.0.1", str(self.port), str(self.key)], "server")
        # Readiness is observed from flushed server output, not assumed from a sleep.
        for _ in range(100):
            if "LISTEN" in (self.folder / "server.log").read_text():
                break
            if self.processes[0].poll() is not None:
                self.fail((self.folder / "server.log").read_text())
            time.sleep(0.02)
        else:
            self.fail("Server did not become ready")

    def start(self, args, name):
        log = (self.folder / f"{name}.log").open("w")
        self.logs.append(log)
        proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
        self.processes.append(proc)
        return proc

    def tearDown(self):
        for proc in self.processes:
            if proc.poll() is None:
                proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        for log in self.logs:
            log.close()
        self.temp.cleanup()

    def client(self, port, count, mode="auto", key=None):
        return [executable(BUILD, "acs_client"), "127.0.0.1", str(port), str(key or self.key),
                str(ROOT / "models/policy.tree"), str(count), str(self.folder / "metrics.csv"), "hello", mode]

    def test_both_ciphers_and_wrong_key(self):
        for mode in ["aes", "chacha"]:
            result = subprocess.run(self.client(self.port, 5, mode), capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("acknowledged=5", result.stdout)
        wrong = self.folder / "wrong.key"
        wrong.write_text(secrets.token_hex(32))
        result = subprocess.run(self.client(self.port, 2, key=wrong), capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 3)
        self.assertIn("acknowledged=0", result.stdout)

    def test_replay_tampering_and_ack_reflection(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as relay:
            relay.bind(("127.0.0.1", 0))
            relay.settimeout(2)
            proc = self.start(self.client(relay.getsockname()[1], 1, "chacha"), "client")
            wire, client = relay.recvfrom(65535)
            relay.sendto(wire, ("127.0.0.1", self.port))
            reply, _ = relay.recvfrom(65535)
            relay.sendto(reply, client)
            self.assertEqual(proc.wait(timeout=5), 0)
            # No fresh ACK for a replay, tampered packet, or reflected ACK.
            for bad in [wire, wire[:-1] + bytes([wire[-1] ^ 1]), reply, b"ACS1"]:
                relay.sendto(bad, ("127.0.0.1", self.port))
                relay.settimeout(0.15)
                with self.assertRaises(socket.timeout):
                    relay.recvfrom(65535)

    def test_live_switching(self):
        port = free_port()
        while port == self.port:
            port = free_port()
        proxy = self.start([sys.executable, str(ROOT / "python/proxy.py"), "--listen", str(port),
                           "--server", str(self.port), "--phase-seconds", "3"], "proxy")
        for _ in range(100):
            if "PROXY" in (self.folder / "proxy.log").read_text():
                break
            time.sleep(0.02)
        self.assertIsNone(proxy.poll())
        result = subprocess.run(self.client(port, 260), capture_output=True, text=True, timeout=90)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("SWITCH", result.stdout)
        with (self.folder / "metrics.csv").open(newline="") as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 260)
        self.assertEqual({r["cipher"] for r in rows if r["acked"] == "1"},
                         {"AES-256-GCM", "ChaCha20-Poly1305"})
        self.assertTrue(any(float(r["loss_pct"]) > 0 for r in rows))
        # Sequence numbers are continuous: switching did not restart the client session.
        self.assertEqual([int(r["sequence"]) for r in rows], list(range(260)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    args, rest = parser.parse_known_args()
    BUILD = args.build
    unittest.main(argv=[sys.argv[0], *rest])
