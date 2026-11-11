"""A local UDP impairment relay. Delays real datagrams; never edits metrics."""
import argparse
import heapq
import random
import select
import socket
import time


def run(listen_port, server_port, seed=42, phase_seconds=6.0):
    rng = random.Random(seed)
    server = ("127.0.0.1", server_port)
    pending = []
    client = None
    started = time.monotonic()
    serial = 0
    previous = -1
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", listen_port))
        print(f"PROXY 127.0.0.1:{listen_port} -> {server_port}", flush=True)
        while True:
            now = time.monotonic()
            phase = int((now - started) / phase_seconds) % 3
            if phase != previous:
                print(f"PROFILE {('clean', 'congested', 'recovery')[phase]}", flush=True)
                previous = phase
            while pending and pending[0][0] <= now:
                _, _, payload, target = heapq.heappop(pending)
                sock.sendto(payload, target)
            wait = min(0.02, max(0, pending[0][0] - now)) if pending else 0.02
            if not select.select([sock], [], [], wait)[0]:
                continue
            payload, source = sock.recvfrom(65535)
            if source == server:
                if client is None:
                    continue
                target = client
            else:
                if client is not None and source != client:
                    continue  # Deliberately a one-client demonstration relay.
                client, target = source, server
            delay, loss = ((0.002, 0.0) if phase != 1 else (rng.uniform(0.035, 0.105), 0.16))
            if rng.random() < loss:
                continue
            serial += 1
            heapq.heappush(pending, (time.monotonic() + delay, serial, payload, target))


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--listen", type=int, default=9001)
    p.add_argument("--server", type=int, default=9000)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--phase-seconds", type=float, default=6)
    args = p.parse_args()
    if not (1 <= args.listen <= 65535 and 1 <= args.server <= 65535 and args.listen != args.server and args.phase_seconds > 0):
        p.error("Use distinct valid ports and a positive phase duration")
    try:
        run(args.listen, args.server, args.seed, args.phase_seconds)
    except KeyboardInterrupt:
        pass
