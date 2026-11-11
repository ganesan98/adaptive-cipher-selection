#pragma once
#include "types.hpp"
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace acs {
inline void crypto_ok(int result) { require(result == 1, "OpenSSL operation failed"); }
inline Session new_session() {
    Session s{};
    crypto_ok(RAND_bytes(s.data(), static_cast<int>(s.size())));
    return s;
}
inline Key read_key(const std::string& path) {
    std::ifstream file(path);
    std::string hex, extra;
    require(bool(file >> hex) && !(file >> extra) && hex.size() == 64,
            "Key file must contain exactly 64 hexadecimal characters");
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::runtime_error("Invalid hexadecimal key");
    };
    Key key{};
    for (size_t i = 0; i < key.size(); ++i) key[i] = static_cast<unsigned char>(digit(hex[2*i])*16 + digit(hex[2*i+1]));
    return key;
}
inline void put64(Bytes& b, size_t offset, uint64_t value) {
    for (int i = 7; i >= 0; --i) { b[offset+i] = static_cast<unsigned char>(value); value >>= 8; }
}
inline uint64_t get64(const Bytes& b, size_t offset) {
    uint64_t n = 0;
    for (size_t i = 0; i < 8; ++i) n = (n << 8) | b[offset+i];
    return n;
}
struct Packet { Cipher cipher; Kind kind; Session session; uint64_t sequence; Bytes payload; };

// HKDF separates each session, cipher, and traffic direction into a distinct key.
inline Key derive(const Key& master, const Session& session, Cipher cipher, Kind kind) {
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(
        EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr), EVP_PKEY_CTX_free);
    require(bool(ctx), "Cannot allocate HKDF context");
    const unsigned char info[] = {'A','C','S','-','v','1', static_cast<unsigned char>(cipher), static_cast<unsigned char>(kind)};
    crypto_ok(EVP_PKEY_derive_init(ctx.get()));
    crypto_ok(EVP_PKEY_CTX_set_hkdf_md(ctx.get(), EVP_sha256()));
    crypto_ok(EVP_PKEY_CTX_set1_hkdf_salt(ctx.get(), session.data(), static_cast<int>(session.size())));
    crypto_ok(EVP_PKEY_CTX_set1_hkdf_key(ctx.get(), master.data(), static_cast<int>(master.size())));
    crypto_ok(EVP_PKEY_CTX_add1_hkdf_info(ctx.get(), info, sizeof(info)));
    Key key{}; size_t length = key.size();
    crypto_ok(EVP_PKEY_derive(ctx.get(), key.data(), &length));
    return key;
}
inline const EVP_CIPHER* algorithm(Cipher c) {
    require(c == Cipher::aes || c == Cipher::chacha, "Unknown cipher");
    return c == Cipher::aes ? EVP_aes_256_gcm() : EVP_chacha20_poly1305();
}
inline Bytes seal(const Key& master, const Packet& p) {
    require(p.payload.size() <= max_payload, "Payload exceeds 1100 bytes");
    require(p.kind == Kind::data || p.kind == Kind::ack, "Unknown packet kind");
    Bytes out(header_size + p.payload.size() + tag_size);
    out[0]='A'; out[1]='C'; out[2]='S'; out[3]='1'; out[4]=1;
    out[5]=static_cast<unsigned char>(p.cipher); out[6]=static_cast<unsigned char>(p.kind);
    std::copy(p.session.begin(), p.session.end(), out.begin()+8);
    put64(out, 24, p.sequence);
    out[32]=static_cast<unsigned char>(p.payload.size() >> 8);
    out[33]=static_cast<unsigned char>(p.payload.size());
    auto key = derive(master, p.session, p.cipher, p.kind);
    Bytes nonce(12); put64(nonce, 4, p.sequence);
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    require(bool(ctx), "Cannot allocate cipher context");
    crypto_ok(EVP_EncryptInit_ex(ctx.get(), algorithm(p.cipher), nullptr, key.data(), nonce.data()));
    int count=0, written=0;
    crypto_ok(EVP_EncryptUpdate(ctx.get(), nullptr, &count, out.data(), header_size));
    if (!p.payload.empty()) {
        crypto_ok(EVP_EncryptUpdate(ctx.get(), out.data()+header_size, &written, p.payload.data(), static_cast<int>(p.payload.size())));
    }
    crypto_ok(EVP_EncryptFinal_ex(ctx.get(), out.data()+header_size+written, &count));
    crypto_ok(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG, tag_size, out.data()+header_size+p.payload.size()));
    return out;
}
inline Packet open(const Key& master, const Bytes& wire) {
    require(wire.size() >= header_size+tag_size && wire.size() <= header_size+max_payload+tag_size, "Invalid datagram size");
    require(wire[0]=='A' && wire[1]=='C' && wire[2]=='S' && wire[3]=='1' && wire[4]==1, "Invalid protocol/version");
    require(wire[7]==0 && wire[34]==0 && wire[35]==0, "Reserved bytes must be zero");
    const size_t length = (size_t(wire[32]) << 8) | wire[33];
    require(wire.size() == header_size+length+tag_size, "Invalid payload length");
    Packet p{static_cast<Cipher>(wire[5]), static_cast<Kind>(wire[6]), {}, get64(wire,24), {}};
    algorithm(p.cipher);
    require(p.kind == Kind::data || p.kind == Kind::ack, "Unknown packet kind");
    std::copy(wire.begin()+8, wire.begin()+24, p.session.begin());
    auto key = derive(master, p.session, p.cipher, p.kind);
    Bytes nonce(12); put64(nonce,4,p.sequence);
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    require(bool(ctx), "Cannot allocate cipher context");
    crypto_ok(EVP_DecryptInit_ex(ctx.get(), algorithm(p.cipher), nullptr, key.data(), nonce.data()));
    int count=0, written=0;
    crypto_ok(EVP_DecryptUpdate(ctx.get(), nullptr, &count, wire.data(), header_size));
    p.payload.resize(length+16);
    if (length) crypto_ok(EVP_DecryptUpdate(ctx.get(), p.payload.data(), &written, wire.data()+header_size, static_cast<int>(length)));
    std::array<unsigned char,tag_size> tag{};
    std::copy(wire.end()-tag_size,wire.end(),tag.begin());
    crypto_ok(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG, tag_size, tag.data()));
    require(EVP_DecryptFinal_ex(ctx.get(), p.payload.data()+written, &count)==1, "Authentication failed");
    p.payload.resize(length);
    return p;
}
} // namespace acs
