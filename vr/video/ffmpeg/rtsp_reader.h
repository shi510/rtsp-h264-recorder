#pragma once
#include "vr/video/cam_reader.h"
#include <string>
#include <vector>

extern "C"
{
// FFMPEG
#include <libavcodec/avcodec.h> 
#include <libavformat/avformat.h> 
#include <libavdevice/avdevice.h>
}

namespace vr
{

class rtsp_reader final : public cam_reader
{
/*
 * public member functions
 */
public:
	bool connect(const std::string url) override;

	bool play() override;

	bool pause() override;

	bool disconnect() override;

	frame read_frame() override;

/*
 * private member functions
 */
private:
	AVFormatContext* create_rtsp_context(std::string url);

	int get_video_index();

/*
 * private member variables
 */
private:
	AVFormatContext* _rtsp_ctx;
	AVPacket _packet;
	int _video_idx;
};

} // end namespace vr
