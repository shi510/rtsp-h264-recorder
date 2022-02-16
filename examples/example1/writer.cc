#include "writer.h"

writer::writer()
{
	_stop_working = false;
	_is_delay = false;
}

writer::~writer()
{
	close();
}

bool writer::start()
{
	using group_of_pic = std::vector<vr::storage::frame_info>;
	using namespace std::chrono;
	if(!_tp || !_cr)
	{
		return false;
	}
	_worker = std::thread(
		[this]()
		{
			std::vector<vr::storage::frame_info> gop;
			_cr->play();
			while(true)
			{
				using namespace std::chrono;
				auto fr = _cr->read_frame();
				milliseconds ms_now = duration_cast<milliseconds>(
					system_clock::now().time_since_epoch());
				if(fr.extra_data && gop.size() > 0)
				{
					_tp->write(gop);
					gop = group_of_pic();
					if(_is_delay)
					{
						std::this_thread::sleep_for(seconds(_delay_sec));
						_is_delay = false;
					}
				}
				if(_stop_working)
					break;
				gop.push_back({fr.data, ms_now});
			}
		}
	);
	return true;
}

void writer::set_tape(std::shared_ptr<vr::tape> tp)
{
	_tp = tp;
}

void writer::set_cam_reader(std::shared_ptr<vr::cam_reader> cr)
{
	_cr = cr;
}

void writer::set_delay(int sec)
{
	_is_delay = true;
	_delay_sec = sec;
}

void writer::close()
{
	_stop_working = true;
	_worker.join();
}
