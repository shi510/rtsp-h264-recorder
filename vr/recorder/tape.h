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
#include <functional>

namespace vr
{

class tape
{
public:
    typedef int32_t _StrgKey;
    typedef uint64_t _TimelineKey;

    static constexpr int SYSTEM_BASE_YEAR = 1900;
    static constexpr int BASE_YEAR = 2020;
    static const std::string FILE_NAME_REGEX;

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
    
    option get_option() const;

    bool write(std::vector<storage::frame_info> gop);

    // get all recording timelines.
    std::vector<std::pair<uint64_t, uint64_t>> timeline(int index);

    // get recent recording timelines.
    std::shared_ptr<std::pair<uint64_t, uint64_t>> recent_timeline(int index);

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

    std::vector<std::string> get_old_files(std::string dir, int yday);

    void restrict_option();

private:

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
    std::queue<std::vector<storage::frame_info>> __wbuf;
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

class tape_pool
{
    std::string __root_dir;
    std::map<std::string, std::shared_ptr<vr::tape>> __tps;

public:
    typedef std::function<vr::tape::option(std::string)> opt_calback_fn;

    tape_pool(std::string root_dir, opt_calback_fn fn);

    std::shared_ptr<vr::tape> create(std::string tp_key, vr::tape::option opt);

    std::shared_ptr<vr::tape> find(std::string tp_key);

    ~tape_pool();

    void close();
};


} // end namespace vr

