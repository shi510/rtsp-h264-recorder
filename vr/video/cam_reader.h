#pragma once
#include <string>
#include <vector>

namespace vr
{

struct frame
{
    bool extra_data;
    std::vector<uint8_t> data;
};

class cam_reader
{
public:
    virtual bool connect(const std::string url) = 0;

    virtual bool play() = 0;

    virtual bool pause() = 0;

    virtual bool disconnect() = 0;

    virtual frame read_frame() = 0;
};

} // end namespace vr

