#include "vr/streamer/streamer.h"
#include <iostream>

extern "C"
{

// UNIX NET/SOCKET
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>

};

namespace vr
{

bool streamer::open(int port, int max_queue)
{
	struct sockaddr_in address;
	int opt = 1;
	if((_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	if(setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
		&opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	if(bind(_server_fd, (struct sockaddr *)&address, sizeof(address))<0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	_is_stop = false;
	_acceptor = std::thread(
		[this, &max_queue]()
		{
			struct sockaddr_in address;
			int addrlen = sizeof(address);
			if (listen(_server_fd, max_queue) < 0)
			{
				perror("listen");
				exit(EXIT_FAILURE);
			}
			while(true)
			{
				int new_cli = nonblocking_accept(_server_fd, 5,
					(struct sockaddr *)&address,
					(socklen_t*)&addrlen);
				{
					std::unique_lock<std::mutex> lock(_cli_mtx);
					// _cli_cv.wait(lock, [this](){return _is_stop;});
					if(_is_stop)
					{
						break;
					}
					if(new_cli > 0)
						_cli_list.push_back(new_cli);
				}
			}
		});

	_sender = std::thread(
		[this]()
		{
			while(true)
			{
				std::unique_lock<std::mutex> lock(_sbuf_mtx);
				_sender_cv.wait(lock,
					[this](){return _is_stop || !_sbuf.empty();});
				if(_is_stop)
				{
					break;
				}
				auto data = _sbuf.front();
				_sbuf.pop();
				const int size = data.size();
				for(auto cli : _cli_list)
				{
					int total = 0;
					while(total != size)
					{
						int sent = send(cli, data.data() + total,
							size - total, 0);
						if(sent <= 0)
						{
							break;
						}
						total += sent;
					}
				}

			}
		}
	);
	return true;
}

bool streamer::close()
{
	_is_stop = true;
	_sender_cv.notify_one();
	_sender.join();
	_acceptor.join();
	for(auto cli : _cli_list)
	{
		::close(cli);
	}
	::close(_server_fd);
	return true;
}

void streamer::broadcast(std::vector<uint8_t> data)
{
	std::unique_lock<std::mutex> lock(_sbuf_mtx);
	_sbuf.push(data);
	_sender_cv.notify_one();
}

int streamer::nonblocking_accept(
	int sock,
	int timeout,
	struct sockaddr* addr,
	socklen_t* len)
{
	int ret;
	struct timeval tv;
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);
	tv.tv_sec = (long)timeout;
	tv.tv_usec = 0;

	ret = select(sock+1, &rfds, (fd_set *) 0, (fd_set *) 0, &tv);
	if(ret > 0)
	{
		return accept(sock, addr, len);
	}
	return -1;
}

} // end namespace vr
