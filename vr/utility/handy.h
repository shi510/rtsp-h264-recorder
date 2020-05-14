#pragma once
#include <map>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <system_error>
#include <string>
#include <vector>

namespace utility
{

template <typename T1, typename T2>
T1 find_closest_key(const std::map<T1, T2> & data, T1 key)
{
	if(data.size() == 0)
	{
		throw std::out_of_range("Received empty map.");
	}
	auto lower = data.lower_bound(key);

	if (lower == data.end()) // If none found, return the last one.
		return std::prev(lower)->first;

	if (lower == data.begin())
		return lower->first;

	// Check which one is closest.
	auto previous = std::prev(lower);
	if((key - previous->first) < (lower->first - key))
		return previous->first;

	return lower->first;
}

template <typename T>
class blocking_queue
{
public:
	void push(T const& value)
	{
		{
			std::unique_lock<std::mutex> lock(__mutex);
			__queue.push_front(value);
		}
		__condition.notify_one();
	}
	T pop()
	{
		std::unique_lock<std::mutex> lock(__mutex);
		__condition.wait(lock, [this]{ return !__queue.empty(); });
		T rc(std::move(__queue.back()));
		__queue.pop_back();
		return rc;
	}

private:
	std::mutex __mutex;
	std::condition_variable __condition;
	std::deque<T> __queue;
};

std::map<std::string, std::vector<std::string>>
get_matched_file_list(const std::string dir, const std::string regex_str);

bool create_directories(const std::string path, std::error_code& ec);

// returns file name list failed to remove.
std::vector<std::string>
remove_files(const std::vector<std::string> files, const std::string root_dir = "");

int get_utc_diff();

} // namespace utility
