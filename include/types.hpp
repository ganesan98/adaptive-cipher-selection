#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace acs {
using Bytes = std::vector<unsigned char>;
using Key = std::array<unsigned char, 32>;
using Session = std::array<unsigned char, 16>;
constexpr size_t header_size = 36, tag_size = 16, max_payload = 1100;
enum class Cipher : unsigned char { aes = 1, chacha = 2 };
enum class Kind : unsigned char { data = 1, ack = 2 };
inline const char* name(Cipher c) { return c == Cipher::aes ? "AES-256-GCM" : "ChaCha20-Poly1305"; }
inline void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}
