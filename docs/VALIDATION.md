# Validation record

Prepared on 11 November 2026. These are observed results, not assumed CI results.

| Check | Result |
|---|---|
| Python source compilation | Passed for all Python and test modules |
| Training | Passed: 12,000 rows, 9,000 training / 3,000 held out, 111 tree nodes |
| Held-out synthetic policy accuracy | 96.9667%; not a performance gain |
| Standalone C++ model test | Passed using MinGW GCC 16.1.0, C++17, warning flags |
| Python/C++ prediction agreement | All 665 probes passed, including split-adjacent float32 inputs |
| Rolling RTT, jitter, and loss calculation | Passed |
| Rolling window eviction and two-vote switching | Passed |
| Replay sequence rejection and session capacity | Passed |
| Key generation and overwrite refusal | Passed |
| Python UDP relay test | Failed locally: receive timed out |
| Independent UDP loopback diagnostic | Timed out even between two sockets in one process, outside project code |
| Full C++ client/server build | Not run: OpenSSL development headers/libraries and CMake unavailable locally |
| AEAD/tampering C++ tests | Written; not executed locally because full build is blocked |
| C++ socket integration and live switching test | Written; not executed locally |
| GitHub Actions | Configured, not yet run |

The independent loopback diagnostic supports an environment-level networking
restriction; it does not itself prove the project's UDP code correct. The
relay test remains a real test, not a skipped check or a fabricated pass.

Training used Python 3.12.10, NumPy 2.5.4, and scikit-learn 1.9.1. Full build
and encrypted traffic behavior need validation on a machine with the listed
dependencies and working localhost UDP, or through the included Ubuntu CI.

## Required before calling the whole project verified

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python -m unittest discover -s tests -p 'test_*.py' -v
python tests/integration.py --build build -v
python python/demo.py
```

Inspect the observed switch messages and metrics. Replace this record with the
actual new results after running them; do not turn pending tests into claims.
