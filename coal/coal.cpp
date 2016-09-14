#include <iostream>
#include "coal.h"
using namespace std;

#define COAL_PI 3.141592653589793

Coal :: Coal()
{
    coal::init();
}

Coal :: ~Coal()
{
    coal::deinit();
}

namespace coal {

    //boost::lockfree::spsc_queue<std::vector<float>> g_Buffers;
    //std::atomic<int> g_Ready = ATOMIC_FLAG_INIT;
    int g_Init = 0;

    void init()
    {
        if(g_Init==0)
            Pa_Initialize();
        ++g_Init;
    }

    void deinit()
    {
        if(g_Init==1)
            Pa_Terminate();
        --g_Init;
    }
    
    Buffer :: Buffer(std::string fn)
    {
        SF_INFO info;
        memset(&info, 0, sizeof(info));
        SNDFILE* sndfile = sf_open(fn.c_str(), SFM_READ, &info);

        assert(!sf_error(sndfile));

        assert(info.frames > 0 && info.channels > 0);
        
        channels = info.channels;
        rate = info.samplerate;
        
        buffer.resize(info.frames * info.channels);
        
        int len = 0;
        int r = 0;
        while((r = sf_read_float(sndfile, (float*)&buffer[len], buffer.size() - len))){
            len += r;
        }
        
        assert(!sf_error(sndfile));

        sf_close(sndfile);
    }

    Stream :: Stream(std::string fn)
        //buffers(buffer_capacity)
    {
        buffers.set_capacity(buffer_capacity);
        SF_INFO info;
        memset(&info, 0, sizeof(info));
        m_pFile = sf_open(fn.c_str(), SFM_READ, &info);
        assert(!sf_error(m_pFile));

        channels = info.channels;
        rate = info.samplerate;
        sz = info.frames;
        assert(info.frames > 0 && info.channels > 0);
        
        update();
    }

    Stream :: ~Stream()
    {
        sf_close(m_pFile);
        //ov_clear(&m_Stream);
    }

    void Stream :: seek(float pos)
    {
        t = pos;
        //len = 0;
        //len_in_buffer = 0;
        // TODO: seek in file
    }

    void Stream :: update()
    {
        if(ended)
            return;
        
        while(buffers.size() < buffer_capacity)
        {
            std::vector<float> buf;
            buf.resize(buffer_size);
            int r = sf_read_float(m_pFile, (float*)&buf[0], buf.size());
            if(r == 0)
                ended = true;
            buffers.push_back(buf);
            assert(not buffers.empty());
        }
    }

    void Source :: add(std::shared_ptr<Buffer> buf)
    {
        buffers.emplace_back(buf);
        buffers[buffers.size()-1].enabled = true;
    }

    void Source :: add(std::shared_ptr<Stream> strm)
    {
        streams.emplace_back(strm);
        streams[streams.size()-1].enabled = true;
    }

    void Source :: update(Space* space, vector<float>& buf)
    {
        auto size = buffers.size() + streams.size();
        auto done_count = 0;
        
        for(auto& b: buffers){
            if(!b.enabled)
                continue;
            if(b.ended){
                ++done_count;
                continue;
            }
            float td = (space->frames * 1.0f / space->freq);
            
            for(int i=0; i<space->frames; ++i){
                try{
                    int ofs = int(b.t*space->freq+0.5);
                    buf[i] += b.buffer->buffer.at(
                        (i + ofs) * b.buffer->channels
                    );
                }catch(const std::out_of_range&){
                    b.ended = true;
                    ++done_count;
                    break;
                }
            }

            b.t += td;
        }
        
        for(auto& s: streams){
            if(!s.enabled){
                continue;
            }
            if(s.ended){
                ++done_count;
                continue;
            }

            s.stream->update();
            
            float td = (space->frames * 1.0f / space->freq);

            if(s.stream->buffers.size() == 0)
                continue;
                
            float new_t_in_buffer = 0.0f;
            for(int i=0; i<space->frames; ++i){
                try{
                    int ofs = int(s.stream->t_in_buffer * space->freq + 0.5);
                    buf[i] += s.stream->buffers[0].at(
                        (i + ofs) * s.stream->channels
                    );
                    new_t_in_buffer += 1.0f/space->freq;
                }catch(const std::out_of_range&){
                    if(s.stream->ended && s.stream->buffers.size() == 0){
                        s.ended = true;
                        ++done_count;
                        new_t_in_buffer = 0.0f;
                        break;
                    }else{
                        s.stream->buffers.pop_front();
                        s.stream->t_in_buffer = 0.0f;
                        new_t_in_buffer = 0.0f;
                        --i; // redo this index
                    }
                }
            }

            s.t += td;
            s.stream->t += td;
            s.stream->t_in_buffer += new_t_in_buffer;
        }

        if(done_count == size)
            playing = false; // all done
    }
    
    void Source :: restart()
    {
        playing = true;
        for(auto& b: buffers){
            b.t = 0.0f;
            b.ended = false;
        }
        for(auto& s: streams){
            s.t = 0.0f;
            s.stream->seek(s.t);
            s.ended = false;
        }
    }
    
    void Source :: play()
    {
        playing = true;
        for(auto& b: buffers){
            if(b.ended)
            {
                b.t = 0.0f;
                b.ended = false;
            }
        }
        for(auto& s: streams){
            if(s.ended)
            {
                s.t = 0.0f;
                s.ended = false;
            }
        }
    }
    
    void Source :: stop()
    {
        playing = false;
        for(auto& b: buffers){
            b.t = 0.0f;
            b.ended = true;
        }
        for(auto& s: streams){
            s.t = 0.0f;
            s.stream->seek(s.t);
            s.ended = true;
        }
    }
    
    void Source :: pause()
    {
        playing = false;
    }
    
    Space :: Space():
        buffers(3)
    {
        auto err = Pa_OpenDefaultStream(
            &stream,
            0,
            2,
            paFloat32,
            freq,
            frames,
            &Space::cb_sample,
            this
        );
        assert(err == paNoError);
        err = Pa_StartStream(stream);
        assert(err == paNoError);

        sample_history = boost::circular_buffer<vector<float>>(172 * 5);
    }
    
    Space :: ~Space()
    {
        if(stream){
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            stream = nullptr;
        }
    }

    void Space :: add(
        std::shared_ptr<Listener> listener, std::shared_ptr<Source> src
    ){
        sources[listener].push_back(src);
    }
    
    void Space :: update()
    {
        if(buffers_queued < 1)
        {
            auto buf = vector<float>(frames);
            for(auto&& b: buf)
                b = 0.0f;
            
            for(auto&& listener: sources)
                for(auto&& source: listener.second)
                    source->update(this, buf);

            sample_history.push_front(buf);

            // effect: delay
            if(delay)
                for(int j=1;j<=delay_count;++j){
                    int i=0;
                    for(auto&& b: buf){
                        try{
                            b += sample_history.at(
                                int(freq/frames * j/delay_speed)
                            ).at(i) * delay_mix/j;
                        }catch(std::out_of_range&){}
                        ++i;
                    }
                }

            // effect: reverb
            if(reverb)
                for(int j=1;j<=reverb_count;++j){
                    int i=0;
                    for(auto&& b: buf){
                        try{
                            b += sample_history.at(
                                int(freq/frames * j/reverb_speed)
                            ).at(i) * reverb_mix/j;
                        }catch(std::out_of_range&){}
                        ++i;
                    }
                }
            
            buffers.push(std::move(buf));
            buffers_queued++;
        }
    }

    int Space :: cb_sample(
        const void* in, void* out,
        unsigned long framecount,
        const PaStreamCallbackTimeInfo* tinfo,
        PaStreamCallbackFlags flags, void* user
    ){
        return ((Space*)user)->sample(in,out,framecount,tinfo,flags);
    }
    int Space :: sample(
        const void* in, void* out,
        unsigned long framecount,
        const PaStreamCallbackTimeInfo* tinfo,
        PaStreamCallbackFlags flags
    ){
        float* o = (float*)out;
        
        for(int i=0;i<frames;++i)
            o[i] = 0.0f;
        
        std::vector<float> buf;
        if(!buffers.pop(buf))
            return 0;
        
        buffers_queued--;

        for(int i=0;i<frames;++i){
            auto f = buf[i];
            f = atan(f)*2.0/COAL_PI;
            for(int j=0;j<2;++j)
                o[i*2+j] = f;
        }
    
        return 0;
    }
}

