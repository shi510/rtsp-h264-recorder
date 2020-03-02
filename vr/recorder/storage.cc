#include "vr/recorder/storage.h"
#include "vr/utility/handy.h"
#include <filesystem>
#include <iostream>

namespace vr
{

storage::~storage()
{
	close();
}

void storage::close()
{
	{
		std::unique_lock<std::mutex> lock(dmtx);
		if(dfile.is_open())
		{
			dfile.close();
		}
	}
	{
		std::unique_lock<std::mutex> lock(imtx);
		if(ifile.is_open())
		{
			ifile.close();
		}
	}
	idxes.clear();
}

bool storage::remove()
{
	close();
	{
		if(!std::filesystem::remove(fname + ".data"))
		{
			std::cerr<<"Fail to remove "<<fname + ".data"<<std::endl;
			return false;
		}
	}
	{
		if(!std::filesystem::remove(fname + ".index"))
		{
			std::cerr<<"Fail to remove "<<fname + ".index"<<std::endl;
			return false;
		}
	}
	return true;
}

std::string storage::name() const
{
	return fname;
}

storage::iterator storage::find(std::time_t at)
{
	iterator it;
	reader rd;
	auto idx_key = make_index_key(at);
	auto exact_key = utility::find_closest_key(idxes, idx_key);
	auto found = idxes.find(exact_key);
	if(found == idxes.end())
	{
		return end();
	}
	rd.dfname = fname + ".data";
	rd.dfile = &dfile;
	rd.dmtx = &dmtx;
	it.__iter = found;
	it.__rd = rd;
	return it;
}

storage::iterator storage::begin()
{
	iterator it;
	reader rd;
	rd.dfname = fname + ".data";
	rd.dfile = &dfile;
	rd.dmtx = &dmtx;
	it.__iter = idxes.begin();
	it.__rd = rd;
	return it;
}

storage::iterator storage::end()
{
	iterator it;
	reader rd;
	rd.dfname = fname + ".data";
	rd.dfile = &dfile;
	rd.dmtx = &dmtx;
	it.__iter = idxes.end();
	it.__rd = rd;
	return it;
}

storage::storage(std::string file_name)
{
	fname = file_name;
}

bool storage::read_index_file(std::string file)
{
	std::ios::openmode mode = std::ios::binary;
	std::ifstream index_file(file, mode);
	if(!index_file.is_open())
		return false;
	index_file.seekg(std::ios::beg);
	while(true)
	{
		index_info ii;
		index_file.read(
			reinterpret_cast<char *>(&ii.loc),
			sizeof(_LocKey)
		);
		index_file.read(
			reinterpret_cast<char *>(&ii.ts),
			sizeof(_TsKey)
		);
		if(index_file.eof())
			break;
		_LocKey idx_key = make_index_key(ii.ts);
		if(idxes.find(idx_key) != idxes.end())
		{
			// std::cout<<"Warning: gop already exists at ";
			// std::cout<<idx_key<<std::endl;
		}
		idxes[idx_key] = ii;
	}
	index_file.close();
	return true;
}

bool storage::write(std::vector<std::vector<uint8_t> > data, std::time_t at)
{
	size_t num_frames = data.size();
	_LocKey data_loc;
	{
		std::unique_lock<std::mutex> lock(dmtx);
		if(!dfile.is_open())
		{
			std::string dfile_name = fname + ".data";
			std::ios::openmode mode = std::ios::in | std::ios::out;
			mode |= std::ios::binary | std::ios::app;
			std::filesystem::create_directories(
				std::filesystem::path(dfile_name).parent_path());
			dfile.open(dfile_name, mode);
			if(!dfile.is_open())
			{
				return false;
			}
		}
		dfile.seekp(0, std::ios::end);
		data_loc = static_cast<_LocKey>(dfile.tellp());
		dfile.write(
			reinterpret_cast<char *>(&num_frames),
			sizeof(size_t));
		for(auto frame : data)
		{
			_LocKey len = frame.size();
			dfile.write(
				reinterpret_cast<char *>(&len),
				sizeof(_LocKey));
			dfile.write(
				reinterpret_cast<char *>(frame.data()),
				len);
		}
	}
	// write group of picture to data file.
	{
		std::unique_lock<std::mutex> lock(imtx);
		_IdxKey idx_key = make_index_key(at);
		if(!ifile.is_open())
		{
			std::string ifile_name = fname + ".index";
			std::ios::openmode mode = std::ios::in | std::ios::out;
			mode |= std::ios::binary | std::ios::app;
			std::filesystem::create_directories(
				std::filesystem::path(ifile_name).parent_path());
			ifile.open(ifile_name, mode);
			if(!ifile.is_open())
			{
				return false;
			}
		}
		ifile.seekp(0, std::ios::end);
		ifile.write(
			reinterpret_cast<char *>(&data_loc),
			sizeof(_LocKey));
		ifile.write(
			reinterpret_cast<char *>(&at),
			sizeof(_TsKey));
		idxes[idx_key] = index_info{data_loc, at};
	}
	return true;
}

storage::_IdxKey storage::make_index_key(const std::time_t time) const
{
	auto t = *gmtime(&time);
	return t.tm_min * 1e2 + t.tm_sec;
}

std::vector<std::vector<uint8_t> > storage::reader::operator()(index_info ii)
{
	std::vector<std::vector<uint8_t> > data;
	std::unique_lock<std::mutex> lock(*dmtx);
	if(!dfile->is_open())
	{
		std::ios::openmode mode = std::ios::in | 
			std::ios::out | std::ios::binary | std::ios::app;
		dfile->open(dfname, mode);
		if(!dfile->is_open())
		{
			// std::cout<<"Fail: "<<dfname<<std::endl;
		}
	}
	size_t num_frames;
	dfile->seekg(std::ios::beg + ii.loc);
	dfile->read(
		reinterpret_cast<char *>(&num_frames),
		sizeof(size_t)
	);
	// read gop.
	for(size_t n = 0; n < num_frames; ++n)
	{
		size_t len;
		std::vector<uint8_t> fr;
		dfile->read(
			reinterpret_cast<char *>(&len),
			sizeof(size_t)
		);
		fr.resize(len);
		dfile->read(
			reinterpret_cast<char *>(fr.data()),
			len
		);
		data.push_back(fr);
	}
	return data;
}

std::vector<std::vector<uint8_t> > storage::iterator::operator*()
{
	return __rd(__iter->second);
}

storage::iterator storage::iterator::operator++()
{
	__iter = std::next(__iter);
	return *this;
}

storage::iterator storage::iterator::operator--()
{
	__iter = std::prev(__iter);
	return *this;
}

bool storage::iterator::operator==(this_type it)
{
	return __iter == it.__iter;
}

bool storage::iterator::operator!=(this_type it)
{
	return __iter != it.__iter;
}

} // end namespace vr