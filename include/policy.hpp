#pragma once
#include "types.hpp"
#include <fstream>
#include <cmath>
#include <deque>
#include <sstream>

namespace acs {
struct Metrics { double latency=0, jitter=0, loss=0; };
// Loss = deadline misses / attempts, not a claim of physical link loss.
class Window {
    std::deque<double> samples_;
public:
    void add(double rtt) {
        samples_.push_back(rtt);
        if (samples_.size()>20) samples_.pop_front();
    }
    Metrics metrics() const {
        Metrics m;
        size_t successes=0, differences=0;
        double previous=-1;
        for (double rtt : samples_) {
            if (rtt<0) { m.loss+=1; continue; }
            m.latency+=rtt; ++successes;
            if (previous>=0) { m.jitter+=std::abs(rtt-previous); ++differences; }
            previous=rtt;
        }
        // No successful samples: conservatively use the client's 250 ms deadline.
        m.latency=successes ? m.latency/successes : 250;
        m.jitter=differences ? m.jitter/differences : 0;
        m.loss=samples_.empty() ? 0 : 100*m.loss/samples_.size();
        return m;
    }
};
class Policy {
    struct Node { int feature, left, right, label; double threshold; };
    std::vector<Node> nodes_;
public:
    explicit Policy(const std::string& path) {
        std::ifstream in(path);
        std::string magic; size_t count=0;
        require(bool(in >> magic >> count) && magic=="ACS_TREE_V1" && count>0 && count<=4095, "Invalid model header");
        nodes_.resize(count);
        for (size_t i=0; i<count; ++i) {
            auto& n=nodes_[i];
            require(bool(in >> n.feature >> n.threshold >> n.left >> n.right >> n.label), "Incomplete model");
            require(std::isfinite(n.threshold), "Nonfinite model threshold");
            if (n.feature==-1) require(n.label==1 || n.label==2, "Invalid model class");
            else {
                require(n.feature>=0 && n.feature<3 && n.left>static_cast<int>(i) && n.right>static_cast<int>(i)
                    && n.left<static_cast<int>(count) && n.right<static_cast<int>(count), "Invalid model edges/features");
            }
        }
        std::string extra;
        require(!(in >> extra), "Unexpected trailing model data");
    }
    Cipher predict(const Metrics& m) const {
        require(std::isfinite(m.latency) && std::isfinite(m.jitter) && std::isfinite(m.loss)
            && m.latency>=0 && m.jitter>=0 && m.loss>=0 && m.loss<=100, "Invalid network metrics");
        // sklearn trees cast features to float32 before traversal.
        float features[]={static_cast<float>(m.latency),static_cast<float>(m.jitter),static_cast<float>(m.loss)};
        size_t i=0;
        while (nodes_[i].feature!=-1) {
            const auto& n=nodes_[i];
            i=features[n.feature]<=n.threshold ? n.left : n.right;
        }
        return static_cast<Cipher>(nodes_[i].label);
    }
};
// A two-decision confirmation limits oscillation without choosing the cipher.
class Selector {
    Cipher active_=Cipher::aes, pending_=Cipher::aes;
    int votes_=0;
public:
    Cipher update(Cipher prediction) {
        if (prediction==active_) { votes_=0; pending_=active_; }
        else {
            if (prediction!=pending_) { pending_=prediction; votes_=0; }
            if (++votes_>=2) { active_=pending_; votes_=0; }
        }
        return active_;
    }
};
} // namespace acs
