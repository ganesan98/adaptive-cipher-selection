# What the classifier learns

The model is a scikit-learn `DecisionTreeClassifier` with maximum depth 6,
minimum leaf size 20, and random seed 42. The C++ application reads its exported
nodes and traverses them. It does not spawn Python or deserialize pickle files.

The default dataset independently samples:

| Feature | Simulated range |
|---|---|
| Mean round-trip latency | 0–250 ms |
| RTT jitter | 0–90 ms |
| Deadline loss | 0–35% |

These are synthetic feature vectors, not packet captures. They are deliberately
simple and do not reproduce all correlations in real networks. A 75/25 stratified
split separates training and evaluation examples.

The illustrative deployment preference is:

```python
stress = latency_ms / 140 + jitter_ms / 45 + loss_pct / 12
stress += 0.3 * sin(latency_ms / 35) * (jitter_ms / 90)
label = 2 if stress > 2.2 else 1
```

This assumes a preference for ChaCha20 under stress and AES under clean conditions.
It is an explicit assumption, **not an experimentally established performance law**.
The decision tree approximates this policy using training examples. C++ contains
no equivalent hand-written stress formula. Updating labels and retraining changes
the runtime decisions without modifying C++.

The model replaces manual runtime thresholds, but synthetic labels still encode
a human-designed policy. Calling this measured optimal cipher selection would
be inaccurate. A decision tree itself also contains learned thresholds; the
distinction is how they are obtained and maintained.

`training_report.json` records held-out classification metrics and the versions
used to produce the included artifact. `requirements.txt` allows compatible
versions; install the exact recorded NumPy/scikit-learn versions if reproducing
the artifact byte-for-byte matters. Retraining with another release may change
tree structure or metrics.

## Why network-only selection has limits

AES-256-GCM and ChaCha20-Poly1305 have the same payload length and 16-byte tag
overhead in this protocol. Changing ciphers does not recover dropped datagrams.
CPU capabilities, acceleration, implementation, payload size, and system load
can matter more than network conditions. Those factors are not model inputs here.
HKDF and EVP context setup are performed for each packet for code clarity; that
overhead is included in the application and can dominate tiny messages.

## Turning this into a measured policy

1. Define a goal: for example, end-to-end latency under a fixed CPU budget.
2. Run forced `aes` and `chacha` modes on the same target hardware, payload sizes,
   and controlled network traces. Randomize order and repeat trials.
3. Label each observation using measured outcomes and document a tie policy.
4. Split by hardware/run/trace, not adjacent rows from one trace, to avoid leakage.
5. Compare adaptive mode against always-AES, always-ChaCha20, and a documented
   threshold baseline on new traces. Report confidence intervals and failures.

Forced modes (the final argument overrides model selection):

```bash
./build/acs_client 127.0.0.1 9001 demo.key models/policy.tree 300 aes.csv "same payload" aes
./build/acs_client 127.0.0.1 9001 demo.key models/policy.tree 300 chacha.csv "same payload" chacha
```

Restart the relay for each experiment, keep its configuration recorded, and
recognize that a seeded real-time relay is still affected by OS scheduling.
The shipped demo is a mechanism demonstration, not a controlled performance study.

## Export format

First line: `ACS_TREE_V1 <node_count>`.
Each subsequent line: `feature threshold left right label`.

Features 0, 1, 2 mean latency, jitter, loss. Feature -1 means a leaf; its label
is 1 or 2. Child indices must point forward within a maximum of 4095 nodes,
preventing cycles and unbounded traversal. The loader rejects incomplete,
nonfinite, out-of-range, or trailing data. Model files are trusted local
configuration; they are not signed or downloaded over the protocol.

The predictor casts inputs to float32 like scikit-learn's tree implementation.
Parity probes cover held-out examples and values beside split thresholds.
