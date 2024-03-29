#include "vr/recorder/storage.h"
#include "vr/utility/handy.h"
#include <filesystem>
#include <iostream>

namespace vr
{

storage::storage(){}

storage::storage(std::string file_name)
    : fname(file_name)
{
    if(fname.empty() || fname == "")
    {
        std::cerr<<"storage::storage - file name is empty."<<std::endl;
        std::cerr<<"storage file name: "<<fname<<std::endl;
        idxes.clear();
        __timeline.clear();
        return;
    }
    
    for(int i = 0 ; i < __max_events+1 ; i++)
    {
        __timeline.push_back(std::map<uint64_t, uint64_t>());
    }
    
    if(!std::filesystem::exists(fname + ".index")){
        return;
    }
    /*
    if(!repair_if_corrupt(fname))
    {
        idxes.clear();
        __timeline.clear();
        return;
    }
    */
    if(!read_index_file(fname))
    {
        // can not open index file.
    }
}

storage::~storage()
{
    close();
}

void storage::close()
{
    idxes.clear();
    __timeline.clear();
}

bool storage::remove()
{
    bool status = true;
    close();
    {
        if(!std::filesystem::remove(fname + ".data"))
        {
            // std::cerr<<"Fail to remove "<<fname + ".data"<<std::endl;
            status = false;
        }
    }
    {
        if(!std::filesystem::remove(fname + ".index"))
        {
            // std::cerr<<"Fail to remove "<<fname + ".index"<<std::endl;
            status = false;
        }
    }
    return status;
}

std::string storage::name() const
{
    return fname;
}

std::vector<std::pair<uint64_t, uint64_t>> storage::timeline(int index) const
{
    std::vector<std::pair<uint64_t, uint64_t>> tl;

    if(__max_events <= index)
    {
        return tl;
    }
    
    for(auto it : __timeline.at(index))
    {
        tl.push_back(std::make_pair(it.first, it.second));
    }
    return tl;
}

std::pair<uint64_t, uint64_t> storage::recent_timeline(int index) const
{
    if(__max_events <= index) return std::make_pair(0, 0);
    auto timeline = __timeline.at(index);
    if(timeline.size() == 0) return std::make_pair(0, 0);
    auto it = std::prev(timeline.end());
    return std::make_pair(it->first, it->second);
}

bool storage::empty() const
{
    return idxes.empty();
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
    rd.dmtx = &dmtx;
    it.__iter = idxes.end();
    it.__rd = rd;
    return it;
}

bool storage::read_data_file(std::string file)
{
    std::ios::openmode mode = std::ios::binary;
    std::ifstream data_file(file + ".data", mode);
    if(!data_file.is_open() || !data_file.good()){
        std::cerr<<"[VR] storage::read_index_file_from_data() - open "<<file+".data failed"<<std::endl;
        return false;
    }
    
    _LocKey num_frames;
    _LocKey frame_size;
    _LocKey frame_msec;
    uint8_t events;
    
    data_file.seekg(0, std::ios::end);
    auto file_size = static_cast<int64_t>(data_file.tellg());
    if(file_size <= 0){
        return false;
    }
    
    constexpr int hdr_size = 3;
    data_file.seekg(std::ios::beg);
    std::vector<char> hdr(hdr_size);
    if(!data_file.read(hdr.data(), hdr_size)) {
        return false;
    }
    
    if(hdr.at(0) != __magic_code[0] || hdr.at(1) != __magic_code[1])
    {
        std::cout<<"invalid magic code: "<<file<<".index"<<std::endl;
        return false;
    }
    
    if(hdr.at(2) != __version) {
        std::cout<<"not supported version code: "<<file<<".index, version: "<<(int)hdr.at(2)<<std::endl;
        return false;
    }
        
    while(data_file.peek() != EOF) {
        index_info ii;
        ii.loc = data_file.tellg();;

        num_frames = 0;
        if(!data_file.read((char *)&num_frames, sizeof(_LocKey))) {
            std::cout <<"fail to read num_frames"<<std::endl;
            return false;
        }
        for(int i = 0 ; i < num_frames ; i++) {
            if(!data_file.read((char *)&frame_size, sizeof(_LocKey))) {
                std::cout <<"fail to read frame_size"<<std::endl;
                return false;
            }
            if(!data_file.read((char *)&events, sizeof(uint8_t))) {
                std::cout <<"fail to read events"<<std::endl;
                return false;
            }
            if(!data_file.read((char *)&frame_msec, sizeof(_LocKey))) {
                std::cout <<"fail to read frame_msec"<<std::endl;
                return false;
            }
            
            if(i == 0) ii.ts = frame_msec;
            if(i == num_frames-1) ii.ts_end = frame_msec;
            data_file.seekg(frame_size, std::ios::cur);
        }

        _LocKey idx_key = make_index_key(ii.ts / 1000);
        idxes[idx_key] = ii;
        update_timeline(events, std::chrono::milliseconds(ii.ts), std::chrono::milliseconds(ii.ts_end));
    }
    return true;
}

bool storage::read_index_file(std::string file)
{
    std::vector<char> fdata;
    _TsKey last_ts = 0;
    std::ios::openmode mode = std::ios::binary;
    std::ifstream index_file(file + ".index", mode);
    if(!index_file.is_open() || !index_file.good()){
        std::cerr<<"[VR] storage::read_index_file() - index_file.fail()"<<std::endl;
        std::cerr<<'\t'<<"index file rdstate: "<<index_file.rdstate()<<std::endl;
        idxes.clear();
        __timeline.clear();
        return false;
    }
    index_file.seekg(0, std::ios::end);
    auto file_size = static_cast<int64_t>(index_file.tellg());
    if(file_size <= 0){
        return false;
    }
    
    constexpr int hdr_size = 3;
    index_file.seekg(std::ios::beg);
    std::vector<char> hdr(hdr_size);
    if(!index_file.read(hdr.data(), hdr_size)) {
        return false;
    }
    
    if(hdr.at(0) != __magic_code[0] || hdr.at(1) != __magic_code[1])
    {
        std::cout<<"invalid magic code: "<<file<<".index"<<std::endl;
        return false;
    }
    
    if(hdr.at(2) == __version)
    {
        fdata.resize(file_size-hdr_size);
        index_file.seekg(hdr_size, std::ios::beg);
        index_file.read(fdata.data(), file_size);
        index_file.close();
        if((file_size-hdr_size) % (sizeof(_LocKey) + sizeof(uint8_t) + sizeof(_TsKey) + sizeof(_TsKey)) != 0){
            std::cout<<"(file_size-hdr_size) % (sizeof(_LocKey) + sizeof(uint8_t) + sizeof(_TsKey) + sizeof(_TsKey)) : ";
            std::cout<<(file_size-hdr_size) % (sizeof(_LocKey) + sizeof(uint8_t) + sizeof(_TsKey) + sizeof(_TsKey))<<std::endl;
        }
        for(int n = 0; n < file_size-hdr_size; n+=sizeof(int64_t)*3+sizeof(uint8_t))
        {
            auto ptr = reinterpret_cast<int64_t *>(fdata.data() + n);
            index_info ii;
            ii.loc = *reinterpret_cast<int64_t *>(fdata.data() + n);
            ii.events = *reinterpret_cast<uint8_t *>(fdata.data() + n + sizeof(_LocKey));
            ii.ts = *reinterpret_cast<int64_t *>(fdata.data() + n + sizeof(_LocKey) + sizeof(uint8_t));
            ii.ts_end = *reinterpret_cast<int64_t *>(fdata.data() + n + sizeof(_LocKey)*2 + sizeof(uint8_t));
                        
            if(ii.ts < last_ts)
            {
                std::cerr<<"[VR] storage::read_index_file() - ii.ts < last_ts"<<std::endl;
                continue;
            }
            last_ts = ii.ts_end;
            _LocKey idx_key = make_index_key(ii.ts / 1000);
            idxes[idx_key] = ii;
            update_timeline(ii.events, std::chrono::milliseconds(ii.ts), std::chrono::milliseconds(ii.ts_end));
        }
    }
    else
    {
        std::cout<<"not supported version code: "<<file<<".index, version: "<<(int)hdr.at(2)<<std::endl;
    }
    
    return true;
}


bool storage::write(const std::vector<frame_info>& data)
{
    using namespace std::chrono;
    size_t num_frames = data.size();
    if(num_frames < 1)
        return false;
    auto at = data[0].msec;
    auto end = data.back().msec;
    if(!idxes.empty())
    {
        _TsKey last_ftime = std::prev(idxes.end())->second.ts_end;
        if(last_ftime > at.count())
        {
            std::cerr<<"[VR] storage::write() - Fail to write a frame:"<<std::endl;
//            std::cerr<<"File               : "<<fname<<std::endl;
//            std::cerr<<"your frame time    : "<<at.count()<<" ";
//            std::cerr<<utility::to_string(at.count())<<std::endl;
//            std::cerr<<"recorded frame time: "<<last_ftime<<" ";
//            std::cerr<<utility::to_string(last_ftime)<<std::endl;
            return false;
        }
    }
    
    uint8_t events = 0;
    _LocKey data_loc;
    {
        std::unique_lock<std::mutex> lock(dmtx);
        std::string dfile_name = fname + ".data";
        std::ios::openmode mode = std::ios::in | std::ios::out;
        mode |= std::ios::binary | std::ios::app;
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path(dfile_name).parent_path(), ec);
        if(ec.value())
        {
            time_t t = at.count() / 1000;
            std::cout<<"storage::write - fail to create directories"<<std::endl;
            return false;
        }
        
        std::fstream dfile(dfile_name, mode);
        if(!dfile.is_open())
        {
            return false;
        }
        
        dfile.seekp(0, std::ios::end);
        data_loc = static_cast<_LocKey>(dfile.tellp());
        
        // write header
        if(data_loc == 0) {
            dfile.write(__magic_code, sizeof(__magic_code));
            dfile.write(&__version, sizeof(__version));
            data_loc = static_cast<_LocKey>(dfile.tellp());
        }
        
        dfile.write((char *)&num_frames, sizeof(num_frames));
        for(auto& frame : data){
            _LocKey len = frame.data.size();
            uint64_t tl = frame.msec.count();
            dfile.write((char *)&len, sizeof(len));
            dfile.write((char *)&frame.events, sizeof(uint8_t));
            dfile.write((char *)&tl, sizeof(tl));
            dfile.write((char *)frame.data.data(), frame.data.size());
            events |= frame.events;
        }
        dfile.close();
    }
    // write group of picture to data file.
    {
        std::unique_lock<std::mutex> lock(imtx);
        std::fstream ifile;
        _TsKey ts = at.count();
        _TsKey ts_end = end.count();
        _IdxKey idx_key = make_index_key(ts / 1000);
        update_timeline(events, at, end);
        
        std::string ifile_name = fname + ".index";
        std::ios::openmode mode = std::ios::in | std::ios::out;
        mode |= std::ios::binary | std::ios::app;
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path(ifile_name).parent_path(), ec);
        if(ec.value())
        {
            time_t t = at.count() / 1000;
            std::cout<<"storage::write - fail to create directories"<<std::endl;
            return false;
        }
        ifile.open(ifile_name, mode);
        ifile.seekp(0, std::ios::end);
        auto before_loc = ifile.tellp();
        
        // write header
        if(before_loc == 0) {
            ifile.write(__magic_code, sizeof(__magic_code));
            ifile.write(&__version, sizeof(__version));
        }
        
        ifile.write((char *)&data_loc, sizeof(data_loc));
        ifile.write((char *)&events, sizeof(uint8_t));
        ifile.write((char *)&ts, sizeof(ts));
        ifile.write((char *)&ts_end, sizeof(ts_end));
        ifile.close();
        idxes[idx_key] = index_info{data_loc, events, ts, ts_end};
    }
    return true;
}

storage::_IdxKey storage::make_index_key(const std::time_t time) const
{
    std::tm t;
    //auto t = *gmtime(&time);
    auto ptr = gmtime_r(&time, &t);
    if(!ptr){
        std::cerr<<"[VR] storage::make_index_key: gmtime_r(&time, &t)"<<std::endl;
    }
    return t.tm_min * 1e2 + t.tm_sec;
}

void storage::update_timeline(uint8_t events, milliseconds at, milliseconds end)
{
    constexpr uint64_t tolerance = 1500; // ms
    uint64_t at_count = at.count();
    uint64_t end_count = end.count();
    uint64_t evt_end_count = (at+milliseconds(3500)).count();
    
    uint8_t mask = 1;
    for(int index = 0 ; index < __max_events+1 ; index++) {
        uint8_t p_mask = mask;
        if(index != 0)
        {
            mask<<=1;
            if(!(events & p_mask))
            {
                continue;
            }
        }
        
        if(__timeline.at(index).empty())
        {
            __timeline.at(index)[at_count] = (index==0) ? end_count : evt_end_count;
            return;
        }
        auto it = std::prev(__timeline.at(index).end());
        if(at_count > it->second)
        {
            auto diff = at_count - it->second;
            if(diff >= tolerance)
            {
                __timeline.at(index)[at_count] = (index==0) ? end_count : evt_end_count;
            }
            else
            {
                it->second = (index==0) ? end_count : evt_end_count;
            }
        }
        else
        {
            it->second = (index==0) ? end_count : evt_end_count;
        }
    }
}

bool storage::repair_if_corrupt(std::string file_name)
{
    namespace fs = std::filesystem;
    std::ios::openmode mode = std::ios::in | std::ios::out |
        std::ios::binary | std::ios::app;
    if(!fs::exists(file_name + ".data"))
    {
        return true;
    }
    if(!fs::exists(file_name + ".index"))
    {
        return true;
    }
    std::fstream data_file(file_name + ".data", mode);
    std::fstream index_file(file_name + ".index", mode);
    if(!data_file.is_open())
    {
        return false;
    }
    if(!index_file.is_open())
    {
        data_file.close();
        return false;
    }
    index_file.seekg(0, std::ios::end);
    data_file.seekg(0, std::ios::end);
    auto idx_fsize = int64_t(index_file.tellg());
    auto data_fsize = int64_t(data_file.tellg());
    auto idx_chunk_size = int64_t(sizeof(_LocKey) + sizeof(_TsKey) + sizeof(_TsKey));
    auto remainder = idx_fsize % idx_chunk_size;

    if(idx_fsize == 0)
    {
        index_file.close();
        data_file.close();
        fs::resize_file(file_name + ".data", 0);
    }
    // index file is ok.
    else if(remainder == 0)
    {
        //check data corruption
        _LocKey last_loc;
        _TsKey last_ts_start;
        _TsKey last_ts_end;
        index_file.seekg(idx_fsize - idx_chunk_size, std::ios::beg);
        index_file.read(
            reinterpret_cast<char *>(&last_loc),
            sizeof(_LocKey));
        index_file.read(
            reinterpret_cast<char *>(&last_ts_start),
            sizeof(_TsKey));
        index_file.read(
            reinterpret_cast<char *>(&last_ts_end),
            sizeof(_TsKey));
        if(last_loc < 0)
        {
            //std::cout<<file_name + ".index"<<std::endl;
            //std::cout<<'\t'<<"last_loc < 0 - "<<last_loc<<std::endl;
            return false;
        }
        if(last_loc >= data_fsize)
        {
            index_file.close();
            data_file.close();
            fs::resize_file(file_name + ".index", idx_fsize - idx_chunk_size);
            repair_if_corrupt(file_name);
        }
        else
        {
            size_t num_frames;
            data_file.seekg(last_loc, std::ios::beg);
            data_file.read(
                reinterpret_cast<char *>(&num_frames),
                sizeof(size_t));
            uint64_t total_len = sizeof(size_t);
            for(int n = 0; n < num_frames; ++n)
            {
                _LocKey len;
                uint64_t msec;
                data_file.read(
                    reinterpret_cast<char *>(&len),
                    sizeof(_LocKey));
                data_file.read(
                    reinterpret_cast<char *>(&msec),
                    sizeof(_TsKey));
                data_file.read(
                    reinterpret_cast<char *>(&msec),
                    sizeof(_TsKey));
                data_file.seekp(len, std::ios::cur);
                if(data_file.eof())
                {
                    break;
                }
                total_len += sizeof(_LocKey);
                total_len += sizeof(_TsKey);
                total_len += sizeof(_TsKey);
                total_len += len;
            }
            if(data_fsize != last_loc + total_len)
            {
                data_file.close();
                index_file.close();
                // remove contaminated data in data file.
                fs::resize_file(file_name + ".data", last_loc);
                fs::resize_file(file_name + ".index", idx_fsize - idx_chunk_size);
            }
            else{
                // data file is ok.
            }
        }
    }
    // index file is corrupted.
    else
    {
        index_file.close();
        data_file.close();
        fs::resize_file(file_name + ".index", idx_fsize - remainder);
        // reparing index file is done.
        // try to repair again.
        repair_if_corrupt(file_name);
    }
    if(data_file.is_open())
    {
        data_file.close();
    }
    if(index_file.is_open())
    {
        index_file.close();
    }
    return true;
}

std::vector<storage::frame_info> storage::reader::operator()(index_info ii)
{
    std::vector<frame_info> data;
    std::unique_lock<std::mutex> lock(*dmtx);
    
    std::ios::openmode mode = std::ios::in |
        std::ios::out | std::ios::binary | std::ios::app;
    std::fstream dfile(dfname, mode);
    if(!dfile.is_open())
    {
        std::cerr<<"[storage.cc, reader::operator()] ";
        std::cerr<<"Fail to open: "<<dfname<<std::endl;
        return data;
    }
    
    dfile.seekg(0, std::ios::end);
    auto dfile_size = static_cast<long>(dfile.tellg());
    if(dfile_size <= ii.loc)
    {
        std::cerr<<"[storage.cc] dfile_size <= ii.loc"<<std::endl;
        return data;
    }
    size_t num_frames;
    dfile.seekg(ii.loc);
    auto cur_pos = static_cast<long>(dfile.tellg());
    dfile.read(
        reinterpret_cast<char *>(&num_frames),
        sizeof(size_t)
    );
    // read gop.
    for(size_t n = 0; n < num_frames; ++n)
    {
        size_t len;
        uint8_t events;
        uint64_t tl;
        std::vector<uint8_t> fr;
        dfile.read(
            reinterpret_cast<char *>(&len),
            sizeof(size_t)
        );
        dfile.read(
            reinterpret_cast<char *>(&events),
            sizeof(uint8_t)
        );
        dfile.read(
            reinterpret_cast<char *>(&tl),
            sizeof(uint64_t)
        );
        if(len > (dfile_size - cur_pos)){
            return std::vector<frame_info>();
        }
        fr.resize(len);
        dfile.read(
            reinterpret_cast<char *>(fr.data()),
            len
        );
        data.push_back({fr, milliseconds(tl), events});
    }
    
    dfile.close();
    return data;
}

std::vector<storage::frame_info> storage::iterator::operator*()
{
    return __rd(__iter->second);
}

storage::iterator& storage::iterator::operator++()
{
    __iter = std::next(__iter);
    return *this;
}

storage::iterator& storage::iterator::operator--()
{
    __iter = std::prev(__iter);
    return *this;
}

bool storage::iterator::operator==(const this_type& it) const
{
    return __iter == it.__iter;
}

bool storage::iterator::operator!=(const this_type& it) const
{
    return __iter != it.__iter;
}

} // end namespace vr
