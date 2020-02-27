#include "writer.h"

writer::writer()
{
	_stop_working = false;
}

writer::~writer()
{
	close();
}

bool writer::start()
{
	if(!_tp || !_cr)
	{
		return false;
	}
	_write_worker = std::thread(
		[this]()
		{
			while(true)
			{
				chunk ck;
				// get a group of picture from buffer.
				{
					std::unique_lock<std::mutex> lock(_write_mtx);
					// wait until the write buffer is filled.
					_wbuf_cond.wait(lock,
						[this]{return _stop_working || !_wbuf.empty();});
					if(_stop_working && _wbuf.size() == 0)
					{
						break;
					}
					ck = _wbuf.front();
					_wbuf.pop();
				}
				_tp->write(ck.gop, ck.time);
			}
		}
	);
	_read_worker = std::thread(
		[this]()
		{
			std::vector<std::vector<uint8_t>> gop;
			_cr->play();
			while(true)
			{
				auto fr = _cr->read_frame();
				if(fr.extra_data && gop.size() > 0)
				{
					std::unique_lock<std::mutex> lock(_write_mtx);
					_wbuf.emplace(chunk{std::time(nullptr), gop});
					_wbuf_cond.notify_one();
					gop = std::vector<std::vector<uint8_t>>();
				}
				if(_stop_working)
				{
					break;
				}
				gop.push_back(fr.data);
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

void writer::close()
{
	_stop_working = true;
	_wbuf_cond.notify_one();
	_read_worker.join();
	_write_worker.join();
}
