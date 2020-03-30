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

bool tape::open(const std::string dir, option opt)
{
	_root = dir;
	__opt = opt;
	if(!aggregate_index(_root))
	{
		return false;
	}
	if(__opt.remove_previous)
	{
		return remove_all_files();
	}
	remove_oldest_storage(true);
	__stop = false;
	__write_worker = std::thread(
		[this]()
		{
			while(true)
			{
				write_chunk chk;
				{
					std::unique_lock<std::mutex> lock(__wmtx);
					__wcv.wait(lock,
						[this](){return __stop || !__wbuf.empty();});
					if(__stop)
					{
						break;
					}
					chk = std::move(__wbuf.front());
					__wbuf.pop();
				}
				auto sec = time_t(chk.at.count() / 1000);
				std::shared_ptr<storage> strg = find_storage(sec);
				if(!strg)
				{
					strg = create_storage(sec);
					if(!remove_oldest_storage())
					{
						// fail to remove oldest storage.
					}
				}
				strg->write(chk.gop, chk.at);
			}
		}
	);
	return true;
}

void tape::close()
{
	__stop = true;
	__wcv.notify_one();
	if(__write_worker.joinable())
	{
		__write_worker.join();
	}
	for(auto it : strgs)
	{
		it.second->close();
	}
}

bool tape::write(std::vector<storage::frame_info> gop, milliseconds at)
{
	std::unique_lock<std::mutex> lock(__wmtx);
	__wbuf.push({gop, at});
	__wcv.notify_one();
	return true;
}

std::vector<std::pair<uint64_t, uint64_t>> tape::timeline()
{
	std::vector<std::pair<uint64_t, uint64_t>> tls;
	for(auto it : strgs)
	{
		auto cur_tl = it.second->timeline();
		for(auto tl : cur_tl)
		{
			tls.push_back(tl);
		}
	}
	return merge_timeline(tls);
	// return tls;
}

std::shared_ptr<std::pair<uint64_t, uint64_t>> tape::recent_timeline()
{
    if(strgs.size() == 0) return nullptr;
	auto it = std::prev(strgs.end());
	return std::make_shared<std::pair<uint64_t, uint64_t>>(it->second->recent_timeline());
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
		"^(\\d{4}-\\d{2}-\\d{2}@\\d{2}-\\d{2}-\\d{2})\\.index";
	std::regex re(name_criterion);
	if(!std::filesystem::exists(dir))
	{
		if(!std::filesystem::create_directories(dir))
		{
			std::cerr<<"Failed to create directory: "<<dir<<std::endl;
			return false;
		}
	}
	for(auto& p: std::filesystem::recursive_directory_iterator(dir))
	{
		if(!p.is_directory())
		{
			auto fname = p.path().filename().string();
			std::cmatch cm;
			if(std::regex_match(fname.c_str(), cm, re))
			{
				std::tm t;
				strptime(cm[1].str().c_str(), "%Y-%m-%d@%H-%M-%S", &t);
				auto strg = create_storage(timegm(&t));
				if(strg->empty())
				{
					std::cerr<<"Fail to read: "<<p.path().string()<<std::endl;
					return false;
				}
			}
		}
	}
	return true;
}

std::vector<std::pair<uint64_t, uint64_t>> tape::merge_timeline(
	const std::vector<std::pair<uint64_t, uint64_t>>& tls)
{
	std::vector<std::pair<uint64_t, uint64_t>> merged;
	if(tls.empty())
	{
		return merged;
	}
	auto first_it = tls.begin();
	auto next_it = std::next(first_it);
	merged.push_back(*first_it);
	if(next_it == tls.end())
	{
		return merged;
	}
	while(next_it != tls.end())
	{
		auto pivot_it = std::prev(merged.end());
		auto junction_diff = next_it->first - (pivot_it->second);
		if(junction_diff <= 1500)
		{
			pivot_it->second = next_it->second;
			++next_it;
		}
		else
		{
			merged.push_back(*next_it);
			++next_it;
		}
	}
	return merged;
}

uint32_t tape::make_storage_key(const std::time_t time) const
{
	auto t = *gmtime(&time);
	return (t.tm_year - (tape::BASE_YEAR - SYSTEM_BASE_YEAR)) * 1e5 +
		t.tm_yday * 1e2 +
		t.tm_hour;
}

std::shared_ptr<storage> tape::find_storage(const std::time_t time)
{
	auto strg_key = make_storage_key(time);
	auto strg_it = strgs.find(strg_key);
	if(strg_it == strgs.end())
	{
		return nullptr;
	}
	return strg_it->second;
}

std::shared_ptr<storage> tape::create_storage(const std::time_t time)
{
	auto strg_key = make_storage_key(time);
	auto strg = std::make_shared<storage>(make_file_name(time));
	if(!strg->empty())
	{
		strgs[strg_key] = strg;
	}
	return strg;
}

bool tape::remove_oldest_storage(bool repeat)
{
	while(!strgs.empty())
	{
		auto oldest_strg_it = strgs.begin();
		auto recent_strg_it = std::prev(strgs.end());
		auto day_diff = recent_strg_it->first - oldest_strg_it->first;
		if(day_diff >= __opt.max_days * 100)
		{
			if(oldest_strg_it->second->remove())
			{
				strgs.erase(oldest_strg_it);
			}
			else
			{
				std::cerr<<"Fail to remove the oldest storage: ";
				std::cerr<<oldest_strg_it->second->name()<<std::endl;
				return false;
			}
		}
		else
			break;
		if(repeat)
			continue;
	}

	return true;
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

bool tape::remove_all_files()
{
	for(auto it : strgs)
	{
		if(!it.second->remove())
		{
			std::cerr<<"Fail to remove "<<it.second->name()<<std::endl;
			return false;
		}
	}
	strgs.clear();
	return true;
}

storage::frame_info tape::iterator::operator*()
{
	auto data = __buf.front();
	return data;
}

tape::iterator& tape::iterator::operator++()
{
	if(!__buf.empty())
	{
		__buf.pop();
	}
	if(__idx_iter == __strg->end())
	{
		__iter = std::next(__iter);
		if(__iter != __iter_end)
		{
			__strg = __iter->second;
			__idx_iter = __strg->begin();
		}
		else
		{
			// __iter is the end of iterator in hear.
			// try get previous iterator.
			// and get std::next again later.
			// the latest index with the latest storage.
			__iter = std::prev(__iter);
			__idx_iter = --__iter->second->end();
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

bool tape::iterator::operator==(const this_type& it) const
{
	return __iter == it.__iter;
}

bool tape::iterator::operator!=(const this_type& it) const
{
	return __iter != it.__iter;
}

} // end namespace vr