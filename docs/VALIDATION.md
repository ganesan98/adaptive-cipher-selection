# Validation record

Verified on 11 November 2026 using GitHub Actions on Ubuntu.

**[Successful full build-and-test run](https://github.com/ganesan98/adaptive-cipher-selection/actions/runs/42974342703)**  
Tested implementation commit: `f495efe1d136649e7ff2dcdfbade3f67a95e6a5a`.

| Check | Observed result |
|---|---|
| CMake configuration and C++ compilation | Passed for client, server, crypto tests, and model tests |
| AEAD crypto tests | Passed for AES-256-GCM and ChaCha20-Poly1305 |
| Tampering, wrong keys, and payload boundaries | Passed |
| Python/C++ model agreement | All 665 probes passed, including split-adjacent float32 values |
| Rolling metrics, switching confirmation, and replay capacity | Passed |
| CTest | 2 of 2 test suites passed |
| Python key generation and overwrite refusal | Passed |
| Python UDP relay forwarding | Passed on Ubuntu |
| Python unit tests | 2 of 2 passed |
| Actual C++ client/server exchange with both ciphers | Passed |
| Wrong-key communication rejection | Passed |
| Live cipher switching under delayed/lost UDP traffic | Passed; both ciphers received authenticated acknowledgements |
| Datagram replay, tampering, and reflected ACK rejection | Passed |
| Process integration tests | 3 of 3 passed in 18.247 seconds |
| Retraining and loading a newly exported model | Passed; 111-node model and all 665 parity probes |
| Test evidence upload | Passed; CTest log retained as an Actions artifact |

The live switching test sends 260 messages through the impairment relay.
It checks for a switch, authenticated successful exchanges under both ciphers,
nonzero measured deadline loss, and continuous sequence numbers.
This validates switching behavior; it does not establish a performance advantage.

## Model evaluation

The included model was trained on 12,000 simulated rows with 9,000 used for
training and 3,000 held out. Its held-out synthetic policy accuracy is
96.9667%. This measures agreement with the documented preference labels,
not throughput or latency improvement.

Original model generation used Python 3.12.10, NumPy 2.5.4, and scikit-learn 1.9.1.
The successful Ubuntu workflow used Python 3.12.16 and also retrained the model
using the dependencies installed from `requirements.txt`.

## Local environment history

The initial Windows workspace passed the standalone C++ model, metric, replay,
and key-generation checks. It lacked OpenSSL development files and CMake.
UDP receive also timed out in an independent same-process loopback diagnostic.
Those local limitations blocked full validation there; the subsequent Ubuntu
workflow completed the full build and network tests successfully.

A full native Windows build remains unverified. The automatic demo wrapper
has not been separately smoke-tested; its C++ endpoints, relay, and live switching
behavior are exercised by the integration suite.

## Reproduce the checks

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python -m unittest discover -s tests -p 'test_*.py' -v
python tests/integration.py --build build -v
python python/train.py --out build/retrained
./build/policy_test build/retrained/policy.tree build/retrained/parity.csv
```

For a manual demonstration with retained metrics and logs:

```bash
python python/demo.py
```
