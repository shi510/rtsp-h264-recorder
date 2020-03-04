#pragma once
#include <fstream>
#include <mutex>
#include <map>
#include <vector>

namespace vr
{

class storage
{
	typedef int64_t _LocKey;
	typedef int64_t _TsKey;
	typedef uint32_t _IdxKey;

	class reader;

	struct index_info
	{
		// location of group of picture in data file.
		_LocKey loc;
		// time stamp for each group of picture.
		_TsKey ts;
	};

	// file name excluding extension.
	std::string fname;
	// data file stream.
	std::fstream dfile;
	// index file stream.
	std::fstream ifile;
	// mutex for data file stream.
	std::mutex dmtx;
	// mutex for index file stream.
	std::mutex imtx;

	/*
	* Key(_IdxKey) of the map is 
		a value concatenated with minute-second.
	* It has upto 4 position. Minute(2), Second(2).
	* Value of the map is index_info.
	* See above index_info structure.
	*/
	std::map<_IdxKey, index_info> idxes;

public:
	class iterator;

	storage(std::string file_name);

	~storage();

	void close();

	bool remove();
	
	std::string name() const;

	bool empty() const;

	bool write(std::vector<std::vector<uint8_t> > data, std::time_t at);

	iterator find(std::time_t at);

	iterator begin();

	iterator end();

private:
	_IdxKey make_index_key(const std::time_t time) const;

	bool read_index_file(std::string file);
};

class storage::reader
{
	friend class storage;

	std::string dfname;
	std::fstream* dfile;
	std::mutex* dmtx;

public:
	std::vector<std::vector<uint8_t> > operator()(index_info ii);
};

class storage::iterator
{
	friend class storage;

	typedef storage::iterator this_type;

	std::map<_IdxKey, index_info>::iterator __iter;

	storage::reader __rd;

public:
	std::vector<std::vector<uint8_t> > operator*();

	this_type operator++();

	this_type operator--();

	bool operator==(this_type it);

	bool operator!=(this_type it);
};

} // end namespace vr