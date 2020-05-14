#include "vr/utility/handy.h"
#include <filesystem>
#include <ctime>
#include <regex>

namespace utility
{

int get_utc_diff()
{
	auto local_t = time(nullptr);
	auto utc_tm = gmtime(&local_t);
	auto utc_t = mktime(utc_tm);
	auto diff_t = local_t-utc_t;
	return gmtime(&diff_t)->tm_hour;
}

std::map<std::string, std::vector<std::string>>
get_matched_file_list(const std::string dir, const std::string regex_str)
{
	std::map<std::string, std::vector<std::string>> list;
	std::regex re(regex_str);
	for(auto& p: std::filesystem::recursive_directory_iterator(dir))
	{
		if(!p.is_directory())
		{
			auto fname = p.path().filename().string();
			std::cmatch cm;
			if(std::regex_match(fname.c_str(), cm, re))
			{
				list[cm[1].str()].push_back(fname);
			}
		}
	}
	return list;
}

bool create_directories(const std::string path, std::error_code& ec)
{
	using namespace std::filesystem;
	create_directories(std::filesystem::path(path), ec);
	if(ec.value())
	{
		return false;
	}
	return true;
}

std::vector<std::string>
remove_files(const std::vector<std::string> files, const std::string root_dir)
{
	std::vector<std::string> fail_list;
	for(auto& fname : files)
	{
		std::filesystem::path fullpath = root_dir;
		fullpath /= fname;
		if(std::filesystem::exists(fullpath))
		{
			bool stat = std::filesystem::remove(fullpath);
			if(!stat)
			{
				fail_list.push_back(fullpath.string());
			}
		}
		else
		{
			fail_list.push_back(fullpath.string());
		}
	}
	return fail_list;
}

} // end namespace utility
