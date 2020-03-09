#pragma once
#include <vr/recorder/tape.h>
#include <vr/video/cam_reader.h>
#include <vector>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <queue>

class writer
{
	using group_of_pic = std::vector<vr::storage::frame_info>;

	struct chunk
	{
		// gop start tiem (utc milliseconds)
		std::chrono::milliseconds time;
		// number of frames at time.
		group_of_pic gop;
	};

	std::shared_ptr<vr::tape> _tp;
	std::shared_ptr<vr::cam_reader> _cr;
	std::thread _worker;
	bool _stop_working;
	bool _is_delay;
	int _delay_sec;

public:
	writer();

	~writer();

	bool start();

	void set_tape(std::shared_ptr<vr::tape> tp);

	void set_cam_reader(std::shared_ptr<vr::cam_reader> cr);

	void write_gop(const group_of_pic gop, const std::time_t time);

	void set_delay(int sec);

	void close();
};

