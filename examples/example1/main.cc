#include "writer.h"
#include <vr/video/ffmpeg/rtsp_reader.h>
#include <vr/streamer/streamer.h>
#include <vr/utility/handy.h>
#include <vr/recorder/tape.h>
#include <iostream>
#include <memory>
#include <ctime>
#include <thread>
#include <chrono>
#include <regex>

int main(int argc, char* argv[])
{
	std::thread writer_thread;
	auto tp = std::make_shared<vr::tape>();
	auto wt = std::make_shared<writer>();
	auto streamer = std::make_shared<vr::streamer>();
	auto rtsp = std::make_shared<vr::rtsp_reader>();
	if(argc < 3)
	{
		std::cout<<argv[0]<<" ";
		std::cout<<"[rtsp url]"<<" ";
		std::cout<<"[server port]";
		std::cout<<std::endl;
		return 1;
	}
	if(!rtsp->connect(argv[1]))
	{
		std::cout<<"Fail to connect :";
		std::cout<<argv[1];
		std::cout<<std::endl;
		return 1;
	}
	
	if(!streamer->open(std::atoi(argv[2])))
	{
		std::cout<<"tape can not open."<<std::endl;
		streamer->close();
		return 1;
	}
	if(!tp->open(argv[3]))
	{
		std::cout<<"tape can not open."<<std::endl;
		tp->close();
		return 1;
	}
	wt->set_tape(tp);
	wt->set_cam_reader(rtsp);
	if(!wt->start())
	{
		std::cout<<"writer can not start."<<std::endl;
		wt->close();
		return 1;
	}
	auto past_criterion = "^start:(\\d{4})-(\\d{2})-(\\d{2})@(\\d{2})-(\\d{2})-(\\d{2})";
	std::regex re_past(past_criterion);
	std::thread view_thread;
	bool stop;
	while(true)
	{
		std::string cmd;
		std::cmatch cm;
		std::cout<<"command: ";
		std::cin>>cmd;
		if(std::regex_match(cmd.c_str(), cm, re_past))
		{
			std::tm t;
			t.tm_year = std::atoi(cm[1].str().c_str()) - 1900;
			t.tm_mon = std::atoi(cm[2].str().c_str()) - 1;
			t.tm_mday = std::atoi(cm[3].str().c_str());
			t.tm_hour = std::atoi(cm[4].str().c_str());
			t.tm_min = std::atoi(cm[5].str().c_str());
			t.tm_sec = std::atoi(cm[6].str().c_str());
			std::cout<<"New past view"<<std::endl;
			stop = true;
			std::cout<<"wait old past view... ";
			if(view_thread.joinable())
			{
				view_thread.join();
			}
			std::cout<<"done."<<std::endl;
			stop = false;
			view_thread = std::thread(
				[&tp, &stop, &streamer, t]() mutable
				{
					using ms = std::chrono::duration<int, std::milli>;
					auto tp_iter = tp->find(mktime(&t));
					while(tp_iter != tp->end())
					{
						if(stop)
						{
							break;
						}
						auto data = *tp_iter;
						if(!data.empty())
						{
							streamer->broadcast(data);
						}
						// asssume 30 fps.
						std::this_thread::sleep_for(ms(33));
						++tp_iter;
					}
				}
			);
		}
		else if(cmd == "stop")
		{
			stop = true;
			if(view_thread.joinable())
			{
				view_thread.join();
			}
			streamer->close();
			break;
		}
	}
	return 0;
}
