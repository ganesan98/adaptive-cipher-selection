# Adaptive Cipher
### Networked Communication System 2

**A C++ UDP application that changes its encryption algorithm while it runs.**

C++17 · Python · OpenSSL 3 · scikit-learn · UDP sockets

Imagine sending numbered envelopes to a friend. Each envelope says which lock
protects it. Your friend reads that label, opens the envelope with the right
method, and sends back a protected receipt. You can change locks on the next
envelope without starting the conversation again.

That is this project: a client sends encrypted messages, measures how the
network behaves, and asks a trained classifier which cipher to use next.

| Resume feature | Where it is implemented |
|---|---|
| C++ UDP client and server | `src/client.cpp`, `src/server.cpp`, `include/net.hpp` |
| Live latency, jitter, and packet-loss estimates | Authenticated echo replies and a rolling window in `include/policy.hpp` |
| AES-256 / ChaCha20 switching | AES-256-GCM and ChaCha20-Poly1305 through OpenSSL in `include/crypto.hpp` |
| scikit-learn classifier trained on simulated conditions | `python/train.py`, included trained `models/policy.tree` |
| Retune without editing application logic | Retrain the model file and restart the client; no C++ rebuild |
| Custom header and mid-session transitions | Every packet carries its authenticated cipher ID, session ID, and sequence number |
| Vulnerability-aware design | Authentication, direction-separated keys, replay rejection, size validation, negative tests |

**Scope:** this is a working-design educational prototype, not an audited
transport protocol. The simulation teaches a deployment preference; it does
not establish that congestion makes one cipher faster. See [Security](docs/SECURITY.md)
and [Model](docs/MODEL.md). Check [Validation](docs/VALIDATION.md) for which tests
were actually run when this version was prepared.

## 1. What do I need?

The easiest supported build path is **Ubuntu Linux or Ubuntu in WSL**, with
Python 3.12, a C++ compiler, CMake, and OpenSSL development files.
The C++ source also contains Windows Winsock support; native Windows instructions
are below, but that full build has not yet been verified.

Open a terminal in this project's folder. All commands below run from that folder.

```bash
sudo apt update
sudo apt install -y g++ cmake libssl-dev python3 python3-venv
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
```

What did those commands do?

- `apt install` installs the compiler and OpenSSL headers needed to build C++.
- `venv` makes a separate Python environment for this project.
- `pip install` installs NumPy and scikit-learn for training.
- `cmake` prepares the build, then compiles the client, server, and tests.

The included trained model means you **do not need Python or scikit-learn
running beside the C++ applications**. Python is used for training and demo tools.

## 2. Run the automatic demonstration

```bash
python python/demo.py
```

This starts a server, a network relay, and a client. The relay cycles through
clean, congested, and recovery conditions every six seconds. It delays and drops
real packets. The client measures their effect and selects the cipher.

The run usually takes tens of seconds. Results go into a timestamped `runs/`
folder containing `client.log`, `server.log`, `proxy.log`, and `metrics.csv`.
The temporary key is deleted when the demo ends. Child processes are stopped
even if the run fails or you press Ctrl+C.

Look for lines like these in `client.log` (**illustrative**, not a recorded run):

```text
SWITCH AES-256-GCM -> ChaCha20-Poly1305
SWITCH ChaCha20-Poly1305 -> AES-256-GCM
SUMMARY sent=300 acknowledged=... switches=...
```

Switch timing and packet counts vary with scheduling and loss. A short run may
finish before conditions change; use the default 300 packets. If the program
reports zero switches, inspect the logs instead of claiming a successful switch.

## 3. Run it yourself in separate terminals

First generate a key. It is the shared secret that lets both endpoints encrypt
and decrypt. Never upload it to GitHub.

```bash
python python/keygen.py demo.key
```

Terminal 1 — the receiver:

```bash
./build/acs_server 127.0.0.1 9000 demo.key
```

Terminal 2 — the network simulator:

```bash
python python/proxy.py --listen 9001 --server 9000
```

Terminal 3 — the sender:

```bash
./build/acs_client 127.0.0.1 9001 demo.key models/policy.tree 300 metrics.csv "Hello Ganesan"
```

`127.0.0.1` means this computer. `9000` is the server's port. `9001` is the
relay's port. `300` is the number of messages. `metrics.csv` is the output file.
The server prints receipt metadata, not plaintext messages.

To bypass simulated conditions, send directly to port `9000`. On a clean local
connection, the model will normally stay with AES. **No switch is expected just
because two algorithms are available.**

For two computers on a trusted lab network, securely place the same fresh key
on both, bind the server to its LAN IPv4 address, and point the client to that
address. Use a numeric IPv4 address; DNS names and IPv6 are not implemented.
Allow only the chosen UDP port in your lab firewall. The bundled relay stays
on localhost and supports one client.

Press Ctrl+C to stop the server and relay after manual runs.

## 4. How does it choose a cipher?

The client sends one packet at a time and waits up to **250 ms** for a valid
encrypted reply. A missing or late reply counts as a deadline miss. After each
attempt it updates a window of the most recent 20 attempts:

| Input | Plain-English meaning | Exact definition |
|---|---|---|
| Latency | How long a round trip takes | Mean RTT of acknowledged packets, in ms |
| Jitter | How much that delay changes | Mean absolute difference between successive successful RTTs |
| Loss | How often a reply does not arrive in time | Deadline misses / attempts × 100 |

Latency here is **round-trip**, not one-way delay. Loss cannot distinguish a
dropped request, dropped reply, or late packet. With no successful samples the
latency estimate is 250 ms; jitter is zero until two successful samples exist.

After three initial AES packets, the exported decision tree predicts a cipher
from the **previous** window. Two consecutive predictions requesting a different
cipher cause a switch. This confirmation prevents rapid oscillation; it does
not replace the model with a latency threshold.

The receiver does not need the model. It reads each packet's cipher ID and
authenticates/decrypts that individual packet. The same session and increasing
sequence numbers continue through a switch. UDP itself has no connection to
reconnect; “mid-session” refers to this application session.

## 5. Retrain it

```bash
python python/train.py
```

This generates 12,000 simulated metric rows with seed 42, holds out 25% for
evaluation, trains a decision tree, and exports:

- `models/policy.tree` — portable model loaded by C++.
- `models/training_report.json` — actual held-out evaluation and environment versions.
- `models/parity.csv` — expected predictions for testing Python/C++ agreement.

Restart the client to load the new model. There is no live file watcher and no
application rebuild is required. The included classifier scores about 97% on
its held-out **synthetic preference labels**. That is not a 97% networking
improvement and does not measure encryption strength.

To use your own measured labels:

```bash
python python/train.py --data measurements.csv
```

The CSV must contain `latency_ms,jitter_ms,loss_pct,label`, at least 100 valid
rows, and both classes: `1` for AES-256-GCM, `2` for ChaCha20-Poly1305. See
[Model documentation](docs/MODEL.md) before designing a measurement experiment.

## 6. Test it

```bash
ctest --test-dir build --output-on-failure
python -m unittest discover -s tests -p 'test_*.py' -v
python tests/integration.py --build build -v
```

Tests cover encryption/decryption with both ciphers, every-byte tampering,
wrong keys, malformed sizes, empty and maximum payloads, replay rejection,
model agreement, rolling metrics, switching confirmation, key generation,
relay forwarding, and client/server integration with network impairments.

GitHub Actions is configured to build and run these on Ubuntu after a push.
A configured workflow is not evidence of a successful run; check its result.

## 7. Native Windows build

Install Visual Studio Build Tools with **Desktop development with C++**, CMake,
Python 3.12, and OpenSSL 3 development files using a trusted package manager
such as vcpkg. In a Developer PowerShell, with vcpkg already installed:

```powershell
vcpkg install openssl:x64-windows
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
py -3.12 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python python/demo.py
```

Replace `C:/path/to/vcpkg` with your actual vcpkg folder. Executables are under
`build/Release/`; `demo.py` finds them automatically. Make sure the OpenSSL DLLs
are available beside the executables or on PATH. The full Windows build still
needs verification; the local model-only C++ test was checked with MinGW.

## 8. Troubleshooting

| Problem | What to do |
|---|---|
| CMake cannot find OpenSSL | Install development headers/libraries; the `openssl` command alone is insufficient |
| `bind failed` | Another process has that port; stop it or choose another port |
| No replies | Check the key, IP, port, server log, and UDP firewall rule |
| Model file not found | Run from the project root or supply an absolute model path |
| CSV cannot be created | Create its parent directory first |
| `demo.key` already exists | Reuse it intentionally, or choose a new filename; keygen refuses to overwrite |
| Python installation errors | Use a fresh Python 3.12 virtual environment |
| No cipher change | Run through the impairment relay long enough to encounter congestion |

## 9. Read and explain the code

Start with `src/client.cpp`, then `src/server.cpp`. Read `policy.hpp` to understand
the decision, `crypto.hpp` to understand the envelope, and `net.hpp` for sockets.

- [Protocol byte layout](docs/PROTOCOL.md)
- [Model, labels, and measurement limitations](docs/MODEL.md)
- [Security design and limits](docs/SECURITY.md)
- [Interview walkthrough](docs/INTERVIEW.md)
- [Validation evidence](docs/VALIDATION.md)
- [Publishing to GitHub](docs/PUBLISH.md)

This repository is a new reconstruction from a project description. It does not
recover the deleted repository or its commit history.
