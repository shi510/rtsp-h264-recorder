#pragma once
#include "vr/recorder/storage.h"
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <queue>

namespace vr
{

class tape
{
	typedef uint32_t _StrgKey;

	static constexpr int SYSTEM_BASE_YEAR = 1900;
	static constexpr int BASE_YEAR = 2020;
	/*
	* Key of the map is 
		a value concatenated with year-yday-hour.
	* It has upto 9 position. Year(4), YearDays(3), Hour(2).
	* Note that 0 year is 2020 year, i.e. 1 year is 2021 year.
	* See above BASE_YEAR.
	* Also note that the range of days is 0~365.
	* Value(storage) of the map is storage structure.
	* See above storage struct.
	*/
	std::map<_StrgKey, std::shared_ptr<storage>> strgs;

	// folder path of this tape.
	std::string _root;

public:
	class iterator;

	~tape();

	bool open(const std::string dir);

	void close();

	bool write(std::vector<std::vector<uint8_t>> gop, std::time_t at);

	iterator find(std::time_t at);

	iterator end();

private:
	bool aggregate_index(const std::string dir);

	std::shared_ptr<storage> find_storage(
		const std::time_t time, const bool make = false);

	_StrgKey make_storage_key(const std::time_t time) const;

	std::string make_file_name(const std::time_t time) const;
};

class tape::iterator
{
	friend class tape;

	typedef tape::iterator this_type;

	storage::iterator __idx_iter;
	std::map<_StrgKey, std::shared_ptr<storage>>::iterator __iter;
	std::map<_StrgKey, std::shared_ptr<storage>>::iterator __iter_end;
	std::shared_ptr<storage> __strg;
	std::queue<std::vector<uint8_t>> __buf;

public:
	std::vector<uint8_t> operator*();

	this_type operator++();

	bool operator==(this_type it);

	bool operator!=(this_type it);
};

} // end namespace vr
