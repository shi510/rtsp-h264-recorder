#pragma once
#include "vr/recorder/storage.h"
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace vr
{

class tape
{
	typedef uint32_t _StrgKey;
	typedef uint64_t _TimelineKey;

	static constexpr int SYSTEM_BASE_YEAR = 1900;
	static constexpr int BASE_YEAR = 2020;

public:
	struct option
	{
		// keep storages upto max_days.
		int max_days = 90;
		// remove all previous storages.
		bool remove_previous = false;
	};

	class iterator;

	~tape();

	bool open(const std::string dir, option opt);

	void close();

	bool update_option(option opt);

	bool write(std::vector<storage::frame_info> gop, milliseconds at);

	// get all recording timelines.
	std::vector<std::pair<uint64_t, uint64_t>> timeline();

	// get recent recording timelines.
	std::shared_ptr<std::pair<uint64_t, uint64_t>> recent_timeline();

	iterator find(std::time_t at);

	iterator end();

private:
	bool aggregate_index(const std::string dir);

	std::vector<std::pair<uint64_t, uint64_t>> merge_timeline(
		const std::vector<std::pair<uint64_t, uint64_t>>& tls);

	std::shared_ptr<storage> find_storage(const std::time_t time);

	std::shared_ptr<storage> create_storage(const std::time_t time);

	bool remove_oldest_storage();

	_StrgKey make_storage_key(const std::time_t time) const;

	std::string make_file_name(const std::time_t time) const;

	bool remove_all_files();

private:
	struct write_chunk
	{
		std::vector<storage::frame_info> gop;
		milliseconds at;
	};

	/*
	* Key of the map is 
		a value concatenated with year-yday-hour.
	* It has upto 9 position. Year(4), YearDays(3), Hour(2).
	* Note that 0 year is 2020 year, i.e. 1 year is 2021 year.
	* See above BASE_YEAR.
	* Also note that the range of days is 0~365.
	* Value(storage) of the map is storage class.
	* See storage.h.
	*/
	std::map<_StrgKey, std::shared_ptr<storage>> strgs;

	std::map<_TimelineKey, uint32_t> __timelines;

	option __opt;

	// folder path of this tape.
	std::string _root;
	// write buffer.
	std::queue<write_chunk> __wbuf;
	// thread writer to storage.
	std::thread __write_worker;
	// mutex for thread writer.
	std::mutex __wmtx;
	// cv for thread writer.
	std::condition_variable __wcv;
	// termination condition on thread writer.
	bool __stop;
};

class tape::iterator
{
	friend class tape;

	typedef tape::iterator this_type;

	storage::iterator __idx_iter;
	std::map<_StrgKey, std::shared_ptr<storage>>::iterator __iter;
	std::map<_StrgKey, std::shared_ptr<storage>>::iterator __iter_end;
	std::shared_ptr<storage> __strg;
	std::queue<storage::frame_info> __buf;

public:
	storage::frame_info operator*();

	this_type& operator++();

	bool operator==(const this_type& it) const;

	bool operator!=(const this_type& it) const;
};

} // end namespace vr
