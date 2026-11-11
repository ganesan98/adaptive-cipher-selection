#include "crypto.hpp"
#include "policy.hpp"
#include "replay.hpp"
#include <iostream>
#include <functional>

void check(bool condition) { acs::require(condition,"Test assertion failed"); }
void rejects(const std::function<void()>& fn) {
    bool caught=false;
    try { fn(); } catch (const std::exception&) { caught=true; }
    check(caught);
}
int main(int argc,char** argv) {
    try {
        check(argc==2);
        acs::Key key{}; key.fill(0x37);
        acs::Session session{}; session.fill(0x62);
        for (auto cipher : {acs::Cipher::aes,acs::Cipher::chacha}) {
            for (size_t length : {size_t(0),size_t(1),acs::max_payload}) {
                acs::Packet p{cipher,acs::Kind::data,session,123,acs::Bytes(length,0xab)};
                auto wire=acs::seal(key,p);
                auto decoded=acs::open(key,wire);
                check(decoded.payload==p.payload && decoded.sequence==123 && decoded.session==session && decoded.cipher==cipher);
                // Every byte of header, ciphertext, and tag must be protected.
                for (size_t i=0;i<wire.size();++i) {
                    auto tampered=wire; tampered[i]^=1;
                    rejects([&]{ acs::open(key,tampered); });
                }
                auto wrong=key; wrong[0]^=1;
                rejects([&]{ acs::open(wrong,wire); });
                wire.pop_back(); rejects([&]{ acs::open(key,wire); });
                p.kind=acs::Kind::ack;
                check(acs::open(key,acs::seal(key,p)).kind==acs::Kind::ack);
                check(acs::derive(key,session,cipher,acs::Kind::ack)!=acs::derive(key,session,cipher,acs::Kind::data));
            }
        }
        rejects([&]{ acs::open(key,{}); });
        rejects([&]{ acs::seal(key,{acs::Cipher::aes,acs::Kind::data,session,0,acs::Bytes(1101)}); });
        acs::ReplayGuard replay;
        check(replay.accept(session,5)); check(!replay.accept(session,5));
        check(!replay.accept(session,4)); check(replay.accept(session,6));
        auto other=session; other[0]^=1; check(replay.accept(other,0));
        acs::Policy policy(argv[1]);
        check(policy.predict({2,1,0})==acs::Cipher::aes);
        check(policy.predict({220,80,30})==acs::Cipher::chacha);
        std::cout << "PASS: both AEAD ciphers, boundary payloads, all-byte tampering, wrong keys, direction keys, replay, model\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
