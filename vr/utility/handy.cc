#include "vr/utility/handy.h"
#include <ctime>

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

} // end namespace utility