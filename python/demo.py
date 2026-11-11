"""Start server, impairment relay, and client; clean up children on exit."""
import argparse
import csv
import os
from pathlib import Path
import secrets
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def executable(build, name):
    filename = name + (".exe" if os.name == "nt" else "")
    for folder in [build, build / "Release", build / "Debug"]:
        path = folder / filename
        if path.is_file():
            return str(path.resolve())
    raise FileNotFoundError(f"Build {filename} first (searched {build})")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", type=Path, default=ROOT / "build")
    p.add_argument("--count", type=int, default=300)
    p.add_argument("--port", type=int, default=19000)
    args = p.parse_args()
    if not 1024 <= args.port < 65535 or not 1 <= args.count <= 1000000:
        p.error("Invalid port/count")
    server_bin = executable(args.build, "acs_server")
    client_bin = executable(args.build, "acs_client")
    run = ROOT / "runs" / (time.strftime("%Y%m%d-%H%M%S") + "-" + secrets.token_hex(3))
    run.mkdir(parents=True)
    key = run / "demo.key"
    fd = os.open(key, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w") as stream:
        stream.write(secrets.token_hex(32) + "\n")
    children = []
    try:
        with (run / "server.log").open("w") as server_log, (run / "proxy.log").open("w") as proxy_log:
            children.append(subprocess.Popen([server_bin, "127.0.0.1", str(args.port), str(key)], stdout=server_log, stderr=subprocess.STDOUT))
            children.append(subprocess.Popen([sys.executable, str(ROOT / "python/proxy.py"), "--listen", str(args.port + 1), "--server", str(args.port)], stdout=proxy_log, stderr=subprocess.STDOUT))
            time.sleep(0.5)
            if any(child.poll() is not None for child in children):
                raise RuntimeError(f"Server or proxy failed; inspect {run}")
            with (run / "client.log").open("w") as log:
                result = subprocess.run([client_bin, "127.0.0.1", str(args.port + 1), str(key), str(ROOT / "models/policy.tree"), str(args.count), str(run / "metrics.csv")], stdout=log, stderr=subprocess.STDOUT, timeout=args.count * 0.35 + 10)
            if result.returncode:
                raise RuntimeError(f"Client failed ({result.returncode}); inspect {run}")
    finally:
        for child in children:
            child.terminate()
        for child in children:
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
        key.unlink(missing_ok=True)
    with (run / "metrics.csv").open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    switches = sum(a["cipher"] != b["cipher"] for a, b in zip(rows, rows[1:]))
    print(f"Sent {len(rows)} | acknowledged {sum(int(r['acked']) for r in rows)} | switches {switches}")
    print(f"Logs and measurements: {run}")
    if not switches:
        print("No switch observed. Use --count 300 or inspect the profile/model; do not claim a transition from this run.")


if __name__ == "__main__":
    main()
