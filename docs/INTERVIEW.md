# Explain it without memorizing jargon

## Your 45-second explanation

“I built a C++ UDP client and server that can switch between AES-256-GCM and
ChaCha20-Poly1305 during an application session. The client measures round-trip
latency, jitter, and deadline loss from authenticated replies. A scikit-learn
decision tree trained on simulated conditions selects the cipher. I export the
tree into a small format that C++ can load, so retraining doesn't require changing
the networking code. Each packet carries an authenticated cipher ID, so the
server can decrypt packets even when the algorithm changes. The current policy
is synthetic; it demonstrates adaptation, not a proven performance advantage.”

## Questions you should be able to answer

**Why UDP?** It exposes individual datagrams and packet loss directly. It lets
me design a per-packet protocol, but I must explicitly handle timeouts and replay.

**Is there a UDP connection to preserve?** No. I preserve my application session
ID and sequence stream while changing the cipher. The OS socket stays open.

**Why not plain AES or plain ChaCha20?** Encryption alone does not detect
tampering. GCM and Poly1305 add authentication, and the header is also covered.

**Why not just use `if latency > X`?** The application loads a learned policy,
so its decisions can be retuned from examples without code changes. The shipped
labels still come from an explicit simulated policy, which is a limitation.

**How do you know which cipher is better?** This dataset does not prove that.
I would benchmark both on target hardware and controlled traces, define an
objective, and label training data with measured results.

**How does the receiver learn the new cipher?** Every datagram includes a cipher
ID. It selects the allowed algorithm, then authenticates the whole header and
payload. There is no separate “please switch” message that could get lost.

**What prevents nonce reuse?** A fresh random session creates different derived
keys. Sequence numbers never repeat within a session. Direction and cipher are
also part of key derivation.

**Can someone change the cipher ID?** The header is authenticated, so changing
the ID makes verification fail. Network interference can still affect metrics.

**How do you measure loss?** Missing an authenticated reply within 250 ms. It
is a deadline-loss estimate, not a direct count of router drops.

**What is your biggest missing security feature?** An authenticated handshake
with forward secrecy and replay protection that survives process restarts.

**What should I demo?** Run `python python/demo.py`, open `client.log`, find
`SWITCH`, and compare the preceding metric rows. Then explain the packet layout,
model export, wrong-key test, and replay test.

Before using these claims in an interview, build the full project, run the tests,
observe both cipher transitions, and be comfortable explaining the limitations.
