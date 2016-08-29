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
        while((r = sf_readf_float(sndfile, (float*)&buffer[len], 2048)))
            len += r;
        cout << r << endl;
        
        assert(!sf_error(sndfile));

        sf_close(sndfile);
    }

    Stream :: Stream(std::string fn)
    {
        SF_INFO info;
        memset(&info, 0, sizeof(info));
        SNDFILE* sndfile = sf_open(fn.c_str(), SFM_READ, &info);

        assert(!sf_error(sndfile));

        assert(info.frames > 0 && info.channels > 0);
    }

    Stream :: ~Stream()
    {
        sf_close(m_pFile);
        //ov_clear(&m_Stream);
    }

    void Stream :: seek(float pos)
    {
        t = pos;
    }

    void Source :: add(std::shared_ptr<Buffer> buf)
    {
        buffers.emplace_back(buf);
        buffers[buffers.size()-1].enabled = true;
    }

    void Source :: update(Space* space, vector<float>& buf)
    {
        for(auto& b: buffers){
            if(!b.enabled)
                continue;
        }
        
        for(auto& s: streams){
            if(!s.enabled)
                continue;
        }
    }
    
    void Source :: restart()
    {
        playing = true;
        for(auto& b: buffers){
            b.t = 0.0f;
        }
        for(auto& s: streams){
            s.t = 0.0f;
            s.stream->seek(s.t);
        }
    }
    
    void Source :: play()
    {
        playing = true;
    }
    
    void Source :: stop()
    {
        playing = false;
        for(auto& b: buffers){
            b.t = 0.0f;
        }
        for(auto& s: streams){
            s.t = 0.0f;
            s.stream->seek(s.t);
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
