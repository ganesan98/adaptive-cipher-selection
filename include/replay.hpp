#pragma once
#include "types.hpp"
#include <map>
namespace acs {
class ReplayGuard {
    std::map<Session,uint64_t> highest_;
public:
    // Call only after authentication. No eviction: old sessions cannot become fresh again.
    bool accept(const Session& session, uint64_t sequence) {
        auto it=highest_.find(session);
        if (it!=highest_.end()) {
            if (sequence<=it->second) return false;
            it->second=sequence; return true;
        }
        if (highest_.size()>=4096) return false;
        highest_.emplace(session,sequence); return true;
    }
};
}
