#pragma once
#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#endif
#include "crypto.hpp"
#include <chrono>
#include <limits>

namespace acs {
using Clock=std::chrono::steady_clock;
inline int number(const std::string& text, int low, int high) {
    size_t end=0; int n=std::stoi(text,&end);
    require(end==text.size() && n>=low && n<=high, "Numeric argument out of range");
    return n;
}
inline sockaddr_in address(const std::string& ip, int port) {
    sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(static_cast<uint16_t>(port));
    require(inet_pton(AF_INET,ip.c_str(),&a.sin_addr)==1,"Use a numeric IPv4 address");
    return a;
}
inline bool same_address(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_addr.s_addr==b.sin_addr.s_addr && a.sin_port==b.sin_port;
}
class Socket {
#ifdef _WIN32
    SOCKET fd_=INVALID_SOCKET;
#else
    int fd_=-1;
#endif
public:
    Socket() {
#ifdef _WIN32
        WSADATA data{};
        require(WSAStartup(MAKEWORD(2,2),&data)==0,"WSAStartup failed");
        fd_=socket(AF_INET,SOCK_DGRAM,0);
        if (fd_==INVALID_SOCKET) { WSACleanup(); throw std::runtime_error("socket failed"); }
#else
        fd_=socket(AF_INET,SOCK_DGRAM,0);
        require(fd_>=0,"socket failed");
#endif
    }
    Socket(const Socket&)=delete;
    Socket& operator=(const Socket&)=delete;
    ~Socket() {
#ifdef _WIN32
        closesocket(fd_); WSACleanup();
#else
        close(fd_);
#endif
    }
    void bind_to(const sockaddr_in& a) { require(bind(fd_,reinterpret_cast<const sockaddr*>(&a),sizeof(a))==0,"bind failed (port in use?)"); }
    void send(const Bytes& b, const sockaddr_in& a) {
        auto n=sendto(fd_,reinterpret_cast<const char*>(b.data()),static_cast<int>(b.size()),0,reinterpret_cast<const sockaddr*>(&a),sizeof(a));
        require(n==static_cast<decltype(n)>(b.size()),"sendto failed");
    }
    bool receive(Bytes& b, sockaddr_in& from, int timeout_ms) {
        fd_set fds; FD_ZERO(&fds); FD_SET(fd_,&fds);
        timeval tv{timeout_ms/1000,(timeout_ms%1000)*1000};
#ifdef _WIN32
        int ready=select(0,&fds,nullptr,nullptr,&tv);
#else
        int ready=select(fd_+1,&fds,nullptr,nullptr,&tv);
#endif
        require(ready>=0,"select failed");
        if (!ready) return false;
        // Full UDP buffer: oversized input is detected, never silently truncated into a valid packet.
        b.resize(65536);
#ifdef _WIN32
        int len=sizeof(from);
#else
        socklen_t len=sizeof(from);
#endif
        auto n=recvfrom(fd_,reinterpret_cast<char*>(b.data()),static_cast<int>(b.size()),0,reinterpret_cast<sockaddr*>(&from),&len);
        require(n>=0,"recvfrom failed");
        b.resize(static_cast<size_t>(n));
        return true;
    }
};
} // namespace acs
