#include "net.hpp"
#include "policy.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

int main(int argc,char** argv) {
    try {
        if (argc<6 || argc>9) {
            std::cerr << "Usage: acs_client <server-ip> <port> <key-file> <model> <count> [csv] [message] [auto|aes|chacha]\n";
            return 2;
        }
        auto key=acs::read_key(argv[3]);
        acs::Policy policy(argv[4]);
        const int total=acs::number(argv[5],1,1000000);
        std::string message=argc>=8 ? argv[7] : "Hello from Adaptive Cipher!";
        std::string mode=argc>=9 ? argv[8] : "auto";
        acs::require(mode=="auto" || mode=="aes" || mode=="chacha","Invalid mode");
        acs::require(message.size()<=acs::max_payload,"Message too large");
        std::ofstream csv(argc>=7 ? argv[6] : "metrics.csv");
        acs::require(bool(csv),"Cannot create CSV (create parent directory first)");
        csv << "sequence,cipher,latency_ms,jitter_ms,loss_pct,acked,rtt_ms\n";
        acs::Socket socket;
        auto peer=acs::address(argv[1],acs::number(argv[2],1,65535));
        auto session=acs::new_session();
        acs::Window window; acs::Selector selector;
        int received=0,switches=0; acs::Cipher previous=acs::Cipher::aes;
        for (int i=0;i<total;++i) {
            auto metrics=window.metrics();
            auto cipher=i<3 ? acs::Cipher::aes : selector.update(policy.predict(metrics));
            if (mode!="auto") cipher=mode=="aes" ? acs::Cipher::aes : acs::Cipher::chacha;
            if (i && cipher!=previous) { ++switches; std::cout << "SWITCH " << acs::name(previous) << " -> " << acs::name(cipher) << std::endl; }
            previous=cipher;
            acs::Packet p{cipher,acs::Kind::data,session,static_cast<uint64_t>(i),acs::Bytes(message.begin(),message.end())};
            auto wire=acs::seal(key,p);
            auto start=acs::Clock::now(), deadline=start+std::chrono::milliseconds(250);
            socket.send(wire,peer);
            bool acked=false; double rtt=-1;
            while (acs::Clock::now()<deadline) {
                auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-acs::Clock::now()).count();
                acs::Bytes reply; sockaddr_in source{};
                if (!socket.receive(reply,source,static_cast<int>(std::max<int64_t>(1,remaining)))) break;
                if (!acs::same_address(source,peer)) continue;
                try {
                    auto ack=acs::open(key,reply);
                    if (ack.kind!=acs::Kind::ack || ack.session!=session || ack.sequence!=p.sequence
                        || ack.cipher!=cipher || ack.payload!=p.payload) continue;
                    if (acs::Clock::now()>deadline) break;
                    rtt=std::chrono::duration<double,std::milli>(acs::Clock::now()-start).count();
                    acked=true; ++received; break;
                } catch (const std::exception&) { continue; }
            }
            csv << i << ',' << acs::name(cipher) << ',' << metrics.latency << ',' << metrics.jitter << ',' << metrics.loss << ',' << acked << ',' << rtt << '\n';
            window.add(rtt);
            std::cout << "SEND seq=" << i << " cipher=" << acs::name(cipher) << " ack=" << acked << " rtt_ms=" << std::fixed << std::setprecision(2) << rtt << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
        }
        csv.flush(); acs::require(bool(csv),"Failed writing metrics");
        std::cout << "SUMMARY sent=" << total << " acknowledged=" << received << " switches=" << switches << std::endl;
        return received ? 0 : 3;
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
