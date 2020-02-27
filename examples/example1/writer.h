#pragma once
#include <vr/recorder/tape.h>
#include <vr/video/cam_reader.h>
#include <vector>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <queue>

class writer
{
	using group_of_pic = std::vector<std::vector<uint8_t>>;

	struct chunk
	{
		// time passed from base time (file name).
		std::time_t time;
		// number of frames at time.
		group_of_pic gop;
	};

	std::shared_ptr<vr::tape> _tp;
	std::shared_ptr<vr::cam_reader> _cr;
	std::mutex _write_mtx;
	std::condition_variable _wbuf_cond;
	std::thread _write_worker;
	std::thread _read_worker;
	std::queue<chunk> _wbuf;
	bool _stop_working;

public:
	writer();

	~writer();

	bool start();

	void set_tape(std::shared_ptr<vr::tape> tp);
	
	void set_cam_reader(std::shared_ptr<vr::cam_reader> cr);

	void write_gop(const group_of_pic gop, const std::time_t time);

	void close();
};

