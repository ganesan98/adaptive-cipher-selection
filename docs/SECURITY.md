# Security design and limits

The project uses OpenSSL implementations of AES-256-GCM and ChaCha20-Poly1305,
not home-made cipher primitives. Both provide authenticated encryption.

Implemented safeguards:

- Encrypt and authenticate application payloads; authenticate all header fields.
- Derive traffic keys with HKDF-SHA256, separated by session, cipher, and direction.
- Use cryptographic random session IDs and monotonic sequence-based nonces.
- Verify tags before accepting messages or acknowledging them.
- Authenticate ACKs and bind them to the current request.
- Reject replayed and out-of-order sequence numbers after authentication.
- Validate protocol version, lengths, reserved fields, cipher IDs, and payload bounds.
- Bound session-state memory; fail closed when capacity is reached.
- Do not commit keys; key generation refuses to overwrite an existing file.

Important boundaries:

- **No forward secrecy or key exchange.** A pre-shared key must be delivered
  securely out of band. Anyone with it can impersonate either endpoint.
- **Replay protection is process-local.** After a server restart, old valid
  datagrams can be accepted again with the same master key. Rotate the key.
- **No DoS protection.** Attackers can send invalid packets that consume CPU.
  A key holder can also fill the session table. There is no admission handshake.
- **No formal downgrade negotiation.** Both configured algorithms are allowed.
  Header modification fails authentication, but delaying/dropping traffic can
  influence the classifier's choice. Neither choice is considered “weaker mode.”
- **No reliable delivery.** There are no retransmissions, congestion control,
  flow control, fragmentation, or delivery guarantees.
- **No secure-memory guarantee.** Master/derived keys may remain in process
  memory; crash dumps and endpoint compromise are outside this demo's defenses.
- **No production audit.** Use established DTLS or QUIC implementations for
  production secure datagram communication.

On POSIX, key tools request owner-only file permissions. Windows ACLs depend
on the parent directory. Keep keys in a private user directory. The demo removes
its key on normal exit/exception; an abrupt process kill can leave a key under
`runs/`, which is gitignored.

OpenSSL references:

- https://docs.openssl.org/3.0/man3/EVP_EncryptInit/
- https://docs.openssl.org/3.0/man3/EVP_chacha20/
- https://docs.openssl.org/3.0/man3/EVP_PKEY_CTX_set_hkdf_md/
- https://www.rfc-editor.org/rfc/rfc8439
