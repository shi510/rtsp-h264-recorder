#pragma once
#include <fstream>
#include <mutex>
#include <map>
#include <vector>
#include <chrono>

namespace vr
{
using namespace std::chrono;

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
        // time stamp of last frame of gop.
        _TsKey ts_end;
    };

    // file name excluding extension.
    std::string fname;
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

    std::map<uint64_t, uint64_t> __timeline;

public:
    struct frame_info
    {
        std::vector<uint8_t> data;
        milliseconds msec;
    };

    class iterator;
    
    storage();

    storage(std::string file_name);

    ~storage();

    void close();

    bool remove();
    
    std::string name() const;
    
    std::vector<std::pair<uint64_t, uint64_t>> timeline() const;

    std::pair<uint64_t, uint64_t> recent_timeline() const;

    bool empty() const;

    bool write(const std::vector<frame_info>& data);

    iterator find(std::time_t at);

    iterator begin();

    iterator end();

private:
    _IdxKey make_index_key(const std::time_t time) const;

    bool read_index_file(std::string file);
    bool read_data_file(std::string file);

    void update_timeline(milliseconds at, milliseconds end);

    bool repair_if_corrupt(std::string file_name);
};

class storage::reader
{
    friend class storage;

    std::string dfname;
    std::mutex* dmtx;

public:
    std::vector<frame_info> operator()(index_info ii);
};

class storage::iterator
{
    friend class storage;

    typedef storage::iterator this_type;

    std::map<_IdxKey, index_info>::iterator __iter;

    storage::reader __rd;

public:
    std::vector<frame_info> operator*();

    this_type& operator++();

    this_type& operator--();

    bool operator==(const this_type& it) const;

    bool operator!=(const this_type& it) const;
};

} // end namespace vr

