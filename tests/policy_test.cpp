#include "policy.hpp"
#include "replay.hpp"
#include <algorithm>
#include <iostream>
#include <limits>

int main(int argc,char** argv) {
    try {
        acs::require(argc==3,"Usage: policy_test model parity.csv");
        acs::Policy model(argv[1]);
        bool rejected=false;
        try { model.predict({std::numeric_limits<double>::quiet_NaN(),0,0}); }
        catch (const std::exception&) { rejected=true; }
        acs::require(rejected,"Nonfinite metrics accepted");
        rejected=false;
        try { model.predict({1,0,101}); }
        catch (const std::exception&) { rejected=true; }
        acs::require(rejected,"Out-of-range loss accepted");
        std::ifstream csv(argv[2]); std::string line;
        acs::require(bool(std::getline(csv,line)),"Missing parity CSV");
        int rows=0;
        while (std::getline(csv,line)) {
            std::replace(line.begin(),line.end(),',',' ');
            std::istringstream stream(line); acs::Metrics m; int expected;
            acs::require(bool(stream >> m.latency >> m.jitter >> m.loss >> expected),"Bad parity row");
            acs::require(static_cast<int>(model.predict(m))==expected,"C++/sklearn prediction mismatch");
            ++rows;
        }
        acs::require(rows>=500,"Insufficient parity probes");
        acs::Window window; window.add(10); window.add(-1); window.add(30);
        auto m=window.metrics();
        acs::require(std::abs(m.latency-20)<1e-9 && std::abs(m.jitter-20)<1e-9 && std::abs(m.loss-100.0/3)<1e-9,"Metrics calculation failed");
        for (int i=0;i<20;++i) window.add(5);
        m=window.metrics(); acs::require(m.latency==5 && m.loss==0 && m.jitter==0,"Window eviction failed");
        acs::Selector selector;
        acs::require(selector.update(acs::Cipher::chacha)==acs::Cipher::aes,"Hysteresis first vote failed");
        acs::require(selector.update(acs::Cipher::chacha)==acs::Cipher::chacha,"Hysteresis switch failed");
        acs::require(selector.update(acs::Cipher::aes)==acs::Cipher::chacha,"Hysteresis recovery vote failed");
        acs::require(selector.update(acs::Cipher::aes)==acs::Cipher::aes,"Hysteresis recovery failed");
        acs::ReplayGuard replay; acs::Session session{};
        acs::require(replay.accept(session,0) && !replay.accept(session,0) && replay.accept(session,2) && !replay.accept(session,1),"Replay acceptance failed");
        for (unsigned i=1;i<4096;++i) { session[0]=i&255; session[1]=(i>>8)&255; acs::require(replay.accept(session,0),"Replay capacity failed"); }
        session[2]=1; acs::require(!replay.accept(session,0),"Replay cap bypass");
        std::cout << "PASS: " << rows << " sklearn/C++ predictions, rolling metrics, hysteresis, replay and capacity\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
