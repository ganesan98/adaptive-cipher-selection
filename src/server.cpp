#include "net.hpp"
#include "replay.hpp"
#include <iostream>

int main(int argc,char** argv) {
    try {
        if (argc!=4) { std::cerr << "Usage: acs_server <bind-ip> <port> <key-file>\n"; return 2; }
        auto key=acs::read_key(argv[3]);
        acs::Socket socket;
        socket.bind_to(acs::address(argv[1],acs::number(argv[2],1,65535)));
        acs::ReplayGuard replay;
        std::cout << "LISTEN " << argv[1] << ':' << argv[2] << std::endl;
        for (;;) {
            acs::Bytes wire; sockaddr_in peer{};
            if (!socket.receive(wire,peer,1000)) continue;
            try {
                auto p=acs::open(key,wire);
                if (p.kind!=acs::Kind::data || !replay.accept(p.session,p.sequence)) continue;
                std::cout << "RECV seq=" << p.sequence << " cipher=" << acs::name(p.cipher) << " bytes=" << p.payload.size() << std::endl;
                p.kind=acs::Kind::ack;
                socket.send(acs::seal(key,p),peer);
            } catch (const std::exception&) {
                // Silently discard unauthenticated traffic; do not provide an error oracle.
            }
        }
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
