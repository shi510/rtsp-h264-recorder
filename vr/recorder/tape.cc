#include "vr/recorder/tape.h"
#include <filesystem>
#include <sstream>
#include <regex>
#include <iomanip>
#include <iostream>

namespace vr
{
tape::~tape()
{
	close();
}

bool tape::open(const std::string dir)
{
	_root = dir;
	aggregate_index(_root);
	return true;
}

void tape::close()
{
	for(auto it : strgs)
	{
		it.second->close();
	}
}

bool tape::write(std::vector<std::vector<uint8_t>> gop, std::time_t at)
{
	std::shared_ptr<storage> strg = find_storage(at, true);
	return strg->write(gop, at);
}

tape::iterator tape::find(std::time_t at)
{
	iterator it;
	auto strg_key = make_storage_key(at);
	auto strg_it = strgs.find(strg_key);
	if(strg_it == strgs.end())
	{
		return end();
	}
	it.__strg = strg_it->second;
	it.__iter = strg_it;
	it.__iter_end = strgs.end();
	it.__idx_iter = strg_it->second->find(at);
	return ++it;
}

tape::iterator tape::end()
{
	iterator it;
	it.__iter = strgs.end();
	return it;
}

bool tape::aggregate_index(const std::string dir)
{
	auto name_criterion = 
		"^((\\d{4})-(\\d{2})-(\\d{2})@(\\d{2})-(\\d{2})-(\\d{2}))\\.index";
	std::regex re(name_criterion);
	for(auto& p: std::filesystem::recursive_directory_iterator(dir))
	{
		if(!p.is_directory())
		{
			auto fname = p.path().filename().string();
			std::cmatch cm;
			if(std::regex_match(fname.c_str(), cm, re))
			{
				std::tm t{
					.tm_sec = atoi(cm[7].str().c_str()),
					.tm_min = atoi(cm[6].str().c_str()),
					.tm_hour = atoi(cm[5].str().c_str()),
					.tm_mday = atoi(cm[4].str().c_str()),
					.tm_mon = atoi(cm[3].str().c_str()) - 1,
					.tm_year = atoi(cm[2].str().c_str()) - SYSTEM_BASE_YEAR
				};
				auto strg = find_storage(mktime(&t), true);
				if(!strg->read_index_file(p.path().string()))
				{
					std::cout<<"Fail to read: "<<p.path().string()<<std::endl;
				}
			}
		}
	}
	return true;
}

uint32_t tape::make_storage_key(const std::time_t time) const
{
	auto t = *gmtime(&time);
	return (t.tm_year - (tape::BASE_YEAR - SYSTEM_BASE_YEAR)) * 1e5 +
		t.tm_yday * 1e2 +
		t.tm_hour;
}

std::shared_ptr<storage> tape::find_storage(
	const std::time_t time,
	const bool make)
{
	auto strg_key = make_storage_key(time);
	auto strg_it = strgs.find(strg_key);
	if(strg_it != strgs.end())
	{
		return strg_it->second;
	}
	else if(make)
	{
		auto strg = std::make_shared<storage>(make_file_name(time));
		strgs[strg_key] = strg;
		return strg;
	}
	return nullptr;
}

std::string tape::make_file_name(const std::time_t time) const
{
	std::filesystem::path p = _root;
	std::stringstream ss;
	auto t = *gmtime(&time);
	ss<<std::setw(4)<<std::setfill('0')<<t.tm_year + SYSTEM_BASE_YEAR<<"-";
	ss<<std::setw(2)<<std::setfill('0')<<t.tm_mon + 1<<"-";
	ss<<std::setw(2)<<std::setfill('0')<<t.tm_mday<<"@";
	ss<<std::setw(2)<<std::setfill('0')<<t.tm_hour<<"-";
	ss<<std::setw(2)<<std::setfill('0')<<t.tm_min<<"-";
	ss<<std::setw(2)<<std::setfill('0')<<t.tm_sec;
	return (p / ss.str()).string();
}

std::vector<uint8_t> tape::iterator::operator*()
{
	auto data = std::move(__buf.front());
	__buf.pop();
	return data;
}

tape::iterator tape::iterator::operator++()
{
	if(__idx_iter == __strg->end())
	{
		std::cout<<"try new storage ... ";
		__iter = std::next(__iter);
		if(__iter != __iter_end)
		{
			__strg = __iter->second;
			__idx_iter = __strg->begin();
		}
		else
		{
			// Should i do something hear?
		}
	}
	else{
		if(__buf.empty())
		{
			for(auto frame : *__idx_iter)
			{
				__buf.push(std::move(frame));
			}
			++__idx_iter;
		}
	}
	
	return *this;
}

bool tape::iterator::operator==(this_type it)
{
	return __iter == it.__iter;
}

bool tape::iterator::operator!=(this_type it)
{
	return __iter != it.__iter;
}

} // end namespace vr