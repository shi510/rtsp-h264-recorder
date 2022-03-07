#include "vr/video/ffmpeg/rtsp_reader.h"

namespace vr
{

bool rtsp_reader::connect(const std::string url)
{
    this->_rtsp_ctx = this->create_rtsp_context(url);
    
    if(!this->_rtsp_ctx)
    {
        return false;
    }
    this->_video_idx = this->get_video_index();
    if(this->_video_idx == -1)
    {
        return false;
    }
    
    return true;
}

bool rtsp_reader::play()
{
    return av_read_play(this->_rtsp_ctx) == 0;
}

bool rtsp_reader::pause()
{
    return av_read_pause(this->_rtsp_ctx) == 0;
}

bool rtsp_reader::disconnect()
{
    return true;
}

frame rtsp_reader::read_frame()
{
    frame fr;
    while(true)
    {
        int nRecvPacket = av_read_frame(this->_rtsp_ctx, &this->_packet);
        auto stream_idx = this->_packet.stream_index;
        if(stream_idx == this->_video_idx)
        {
            fr.data.clear();
            std::vector<uint8_t> ext;
            if (this->_packet.flags == AV_PKT_FLAG_KEY)
            {
                AVStream* in_stream  = this->_rtsp_ctx->streams[stream_idx];
                auto ptr = in_stream->codecpar->extradata;
                auto len = in_stream->codecpar->extradata_size;
                ext = std::vector<uint8_t>(ptr, ptr + len);
                fr.extra_data = true;
            }
            else
            {
                fr.extra_data = false;
            }
            auto ptr = this->_packet.data;
            auto len = this->_packet.size;
            std::vector<uint8_t> data(ptr, ptr + len);
            fr.data.resize(len + ext.size());
            std::copy(
                ext.begin(),
                ext.end(),
                fr.data.begin());
            std::copy(
                data.begin(),
                data.end(),
                fr.data.begin() + ext.size());
            av_packet_unref(&this->_packet);
            break;
        }
    }
    return fr;
}

AVFormatContext* rtsp_reader::create_rtsp_context(std::string url){
    AVFormatContext *rtsp_ctx = avformat_alloc_context();
    AVDictionary *dicts = NULL;
    if(av_dict_set(&dicts, "rtsp_transport", "tcp", 0) != 0)
    {
        printf( "Can't set rtsp_transport tcp\n");
        exit(-1);
    }
    if(avformat_open_input(&rtsp_ctx, url.c_str(), NULL, &dicts) != 0){
        return nullptr;
    }
    if(avformat_find_stream_info(rtsp_ctx, 0) < 0){
        return nullptr;
    }
    return rtsp_ctx;
}

int rtsp_reader::get_video_index()
{
    int idx = -1;
    for(int i = 0; i < this->_rtsp_ctx->nb_streams; i++)
    {
        auto codec_type = this->_rtsp_ctx->streams[i]->codecpar->codec_type;
        if(codec_type == AVMEDIA_TYPE_VIDEO)
        {
            idx = i;
            break;
        }
    }
    return idx;
}

} // end namespace vr

