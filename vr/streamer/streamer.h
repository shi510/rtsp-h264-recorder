#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

extern "C"
{

// UNIX NET/SOCKET
// #include <unistd.h>
#include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>

};

namespace vr
{

class streamer
{
    bool _is_stop;
    int _server_fd;
    std::vector<int> _cli_list;
    std::queue<std::vector<uint8_t>> _sbuf;
    std::mutex _cli_mtx;
    std::mutex _sbuf_mtx;
    std::thread _acceptor;
    std::thread _sender;
    std::condition_variable _cli_cv;
    std::condition_variable _sender_cv;

public:
    bool open(int port, int max_queue=5);

    bool close();

    void broadcast(std::vector<uint8_t> data);

private:
    int nonblocking_accept(
        int sock,
        int timeout,
        struct sockaddr* addr,
        socklen_t* len);
};

} // end namespace vr

