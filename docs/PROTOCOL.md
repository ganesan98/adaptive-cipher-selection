# Packet protocol — ACS version 1

One datagram contains a 36-byte header, up to 1100 ciphertext bytes, and a
16-byte authentication tag. Maximum size is 1152 bytes, avoiding fragmentation
on common 1500-byte IP paths; smaller path MTUs are not discovered automatically.
All integers use network byte order (big endian). No raw C++ structs are sent.

| Byte offset | Size | Meaning |
|---|---:|---|
| 0 | 4 | ASCII `ACS1` magic |
| 4 | 1 | Version, exactly 1 |
| 5 | 1 | Cipher: 1 AES-256-GCM, 2 ChaCha20-Poly1305 |
| 6 | 1 | Kind: 1 data, 2 authenticated echo acknowledgement |
| 7 | 1 | Reserved, zero |
| 8 | 16 | Random session ID / HKDF salt |
| 24 | 8 | Sequence number |
| 32 | 2 | Ciphertext/plaintext length |
| 34 | 2 | Reserved, zero |
| 36 | variable | Ciphertext |
| 36 + payload length | 16 | AEAD authentication tag |

The entire header is authenticated as additional authenticated data (AAD).
It is visible to an observer but cannot be changed without invalidating the tag.
The receiver validates fixed fields, lengths, and allowed algorithms before
attempting decryption; it releases no plaintext until verification succeeds.

HKDF-SHA256 inputs:

- Input key material: the shared random 32-byte master key.
- Salt: the 16-byte random session ID.
- Info: bytes `ACS-v1`, followed by the one-byte cipher ID and one-byte kind.
- Output: a 32-byte traffic key.

Each AEAD nonce is 12 bytes: four zero bytes followed by the eight-byte sequence
number. Separate traffic keys prevent collisions between directions and ciphers.
The client creates a new random session for each run and never repeats a sequence
within that session. The CLI permits at most one million messages in a run.
Session uniqueness is probabilistic, relying on OpenSSL's cryptographic RNG.

The ACK echoes the payload under the ACK-direction key with the same sequence
and cipher. The client checks its source address, kind, session, sequence, cipher,
payload, authentication tag, and deadline before counting it.

The server tracks the highest accepted sequence for each authenticated session.
Duplicates and lower sequence numbers are rejected. No packets are retransmitted;
an ACK lost after acceptance remains a deadline miss. Reordered packets are also
rejected by this intentionally simple high-water-mark policy. A production
parallel sender would need a sliding replay window.

The server retains at most 4096 sessions and refuses additional sessions rather
than evicting replay state. State is in memory. Restarting the server clears it;
rotate the shared key when restarting if captured historical traffic is a concern.
There is no authenticated handshake or persistent replay database.
