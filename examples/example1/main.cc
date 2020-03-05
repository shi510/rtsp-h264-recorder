#include "writer.h"
#include <vr/video/ffmpeg/rtsp_reader.h>
#include <vr/streamer/streamer.h>
#include <vr/recorder/tape.h>
#include <iostream>
#include <memory>
#include <ctime>
#include <thread>
#include <chrono>
#include <regex>

int main(int argc, char* argv[])
{
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
	vr::tape::option opt;
	opt.max_days = 90;
	if(!tp->open(argv[3], opt))
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
					if(tp_iter == tp->end())
					{
						std::cout<<"tape not found."<<std::endl;
					}
					while(tp_iter != tp->end())
					{
						if(stop)
						{
							break;
						}
						auto fi = *tp_iter;
						if(!fi.data.empty())
						{
							streamer->broadcast(fi.data);
						}
						// asssume 30 fps.
						std::this_thread::sleep_for(ms(66));
						++tp_iter;
					}
				}
			);
		}
		else if(cmd == "timeline")
		{
			auto tls = tp->timeline();
			
			for(auto tl : tls)
			{
				std::string to;
				std::string from;
				auto tt = time_t(tl.first / 1000);
				auto ct = ctime(&tt);
				auto len = strlen(ct);
				from.resize(len);
				std::copy(ct, ct + len, from.begin());
				from.erase(from.end()-1);

				tt = time_t(tl.second / 1000);
				ct = ctime(&tt);
				len = strlen(ct);
				to.resize(len);
				std::copy(ct, ct + len, to.begin());
				to.erase(to.end()-1);
				std::cout<<from<<":"<<to<<std::endl;
			}
		}
		else if(cmd == "recent_timeline")
		{
			auto tl = tp->recent_timeline();
			std::string to;
			std::string from;
			auto tt = time_t(tl.first / 1000);
			auto ct = ctime(&tt);
			auto len = strlen(ct);
			from.resize(len);
			std::copy(ct, ct + len, from.begin());
			from.erase(from.end()-1);

			tt = time_t(tl.second / 1000);
			ct = ctime(&tt);
			len = strlen(ct);
			to.resize(len);
			std::copy(ct, ct + len, to.begin());
			to.erase(to.end()-1);
			std::cout<<from<<":"<<to<<std::endl;
		}
		else if(cmd == "delay")
		{
			wt->set_delay(2);
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
