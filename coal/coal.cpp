#include <iostream>
#include "coal.h"
#include <boost/algorithm/string.hpp>
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

        if(!sf_error(sndfile))
        {
            assert(info.frames > 0 && info.channels > 0);
            
            channels = info.channels;
            rate = info.samplerate;
            
            std::vector<float> buf;
            buf.resize(info.frames * info.channels);
            
            int len = 0;
            int r = 0;
            try {
                while ((r = sf_read_float(sndfile, (float*)&buf.at(len), buf.size() - len))) {
                    len += r;
                }
            }
            catch (std::out_of_range&) {}

            buffer.resize(channels);
            buffer = extract(buf, channels);
            
            assert(!sf_error(sndfile));

            sf_close(sndfile);
        }
    }

    Stream :: Stream()
    {
        buffers.set_capacity(buffer_capacity);
        channels = 2;
        rate = 44100;
    }
    
    Stream :: Stream(std::string fn)
        //buffers(buffer_capacity)
    {
        buffers.set_capacity(buffer_capacity);
        SF_INFO info;
        memset(&info, 0, sizeof(info));
        m_pFile = sf_open(fn.c_str(), SFM_READ, &info);
        if(!sf_error(m_pFile))
        {
            channels = info.channels;
            rate = info.samplerate;
            sz = info.frames;
            assert(info.frames > 0 && info.channels > 0);
            
            update();
        }
        else
            m_pFile = nullptr;
    }

    Stream :: ~Stream()
    {
        if(m_pFile)
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
        if(ended || !(m_pFile || callback))
            return;
        
        while(buffers.size() < buffer_capacity)
        {
            std::vector<float> buf;
            buf.resize(buffer_size);
            int r;
            if(m_pFile){
                r = sf_read_float(m_pFile, (float*)&buf[0], buf.size());
            }else{
                r = callback((float*)&buf[0], buf.size(), user);
            }
            if(r == 0){
                if(loop)
                {
                    if(m_pFile)
                        sf_seek(m_pFile, 0, SEEK_SET);
                    if(reset_callback)
                        reset_callback(user);
                    len_in_buffer = 0;
                }
                else
                {
                    len_in_buffer = 0;
                    ended = true;
                    break;
                }
            }else{
                len_in_buffer += r;
                buf.resize(r);
                buffers.push_back(buf);
                assert(not buffers.empty());
            }
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

    void Source :: clear()
    {
        buffers.clear();
        streams.clear();
    }

    void Source :: update(Space* space, vector<float>& buf)
    {
        if(!playing)
            return;
        
        auto size = buffers.size() + streams.size();
        auto done_count = 0;
        
        for(auto& b: buffers){
            if(!b.enabled)
                continue;
            if(b.ended){
                ++done_count;
                continue;
            }
            float td = space->frames * 1.0f / space->freq;
            bool restart = false;
            float bt = b.t;

            auto sp = shared_ptr<Buffer>(b.buffer);
            if(!sp->good())
                continue;
            
            for(int i=0; i<space->frames; i+=space->channels){
                try{
                    int ofs = int(bt*space->freq);
                    auto ptch = sp->pitch * b.pitch * pitch;
                    int idx = int(ofs * sp->channels + i) * ptch;
                    float g = sp->gain * b.gain * gain;
                    for(int j=0; j<space->channels; ++j)
                        buf[i + j] += sp->buffer[j % sp->channels].at(
                            idx + (j % sp->channels)
                        ) * g;
                    b.t += 1.0f/space->freq;
                }catch(const std::out_of_range&){
                    if(b.loop){
                        restart = true;
                        b.t = 0.0f;
                    }else{
                        b.ended = true;
                        ++done_count;
                    }
                    break;
                }
            }
        }
        
        for(auto& s: streams){
            if(!s.enabled){
                continue;
            }
            if(s.ended){
                ++done_count;
                continue;
            }

            auto sp = shared_ptr<Stream>(s.stream);
            if(!sp->good())
                continue;
            
            sp->update();
            
            float td = (space->frames * 1.0f / space->freq);

            if(sp->buffers.size() == 0)
                continue;
                
            float new_t_in_buffer = 0.0f;
            for(int i=0; i<space->frames; ++i){
                try{
                    int ofs = int(sp->t_in_buffer * space->freq + 0.5);
                    auto ptch = sp->pitch * s.pitch * pitch;
                    buf[i] += sp->buffers.at(0).at(
                        int((i + ofs) * sp->channels * ptch)
                    ) * sp->gain * s.gain * gain;
                    new_t_in_buffer += 1.0f/space->freq;
                }catch(const std::out_of_range&){
                    if(sp->ended && sp->buffers.size() == 0){
                        s.ended = true;
                        ++done_count;
                        new_t_in_buffer = 0.0f;
                        break;
                    }else{
                        if(not sp->buffers.empty())
                            sp->buffers.pop_front();
                        sp->t_in_buffer = 0.0f;
                        new_t_in_buffer = 0.0f;
                        --i; // redo this index
                    }
                }
            }

            s.t += td;
            sp->t += td;
            sp->t_in_buffer += new_t_in_buffer;
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
            shared_ptr<Stream>(s.stream)->seek(s.t);
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
            shared_ptr<Stream>(s.stream)->seek(s.t);
            s.ended = true;
        }
    }
    void Source:: seek(float t)
    {
        for(auto& b: buffers){
            b.t = t;
            b.ended = false;
        }
        for(auto& s: streams){
            s.t = t;
            shared_ptr<Stream>(s.stream)->seek(t);
            s.ended = false;
        }
    }
    
    void Source :: pause()
    {
        playing = false;
    }
    
    int Source :: loop()
    {
        int r = 0;
        for(auto&& b: buffers)
            r += b.loop;
        for(auto&& s: streams)
            r += shared_ptr<Stream>(s.stream)->loop;
        return r;
    }
    
    void Source :: loop(bool v)
    {
        for(auto&& b: buffers)
            b.loop = v;
        for(auto&& s: streams)
            shared_ptr<Stream>(s.stream)->loop = v;
    }

    unsigned Source :: size() const
    {
        return buffers.size() + streams.size();
    }
    
    Space :: Space():
        buffers(3)
    {
        auto err = Pa_OpenDefaultStream(
            &stream,
            0,
            channels,
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

    Space :: Space(int frames, std::string device):
        buffers(3)
    {
        this->frames = frames;
        
        int dev;
        if(not device.empty()){
            dev = -1;
            int num_dev = Pa_GetDeviceCount();
            for(int i=0; i<num_dev; ++i)
            {
                // check for exact match
                if(boost::to_lower_copy(string(Pa_GetDeviceInfo(i)->name)) == 
                    device
                ){
                    dev = i;
                    break; // exact match, ignore remaining partials
                }

                // check for partial match
                if(boost::to_lower_copy(string(Pa_GetDeviceInfo(i)->name)).find(
                    device
                ) != string::npos){
                    dev = i;
                }
            }
            if(dev == -1)
            {
                throw std::out_of_range("audio device not found");
            }
        }
        else
        {
            dev = Pa_GetDefaultOutputDevice();
        }

        PaStreamParameters outparams;
        memset(&outparams, 0, sizeof(outparams));
        outparams.channelCount = channels;
        outparams.device = dev;
        outparams.hostApiSpecificStreamInfo = NULL;
        outparams.sampleFormat = paFloat32;
        outparams.suggestedLatency = Pa_GetDeviceInfo(dev)->defaultLowOutputLatency;
        outparams.hostApiSpecificStreamInfo = NULL;
        auto err = Pa_OpenStream(
            &stream,
            NULL,
            &outparams,
            freq,
            frames,
            paNoFlag,
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

    void Space :: add(std::shared_ptr<Listener> listener){
        listeners.push_back(listener);
    }
    void Space :: add(std::shared_ptr<Source> src){
        sources.push_back(src);
        //for(auto&& listener: listeners)
        //    sources[listener].push_back(src);
    }

    //void Space :: add(
    //    std::shared_ptr<Listener> listener, std::shared_ptr<Source> src
    //){
    //    //sources[listener].push_back(src);
    //    sources.push_back(src);
    //}
    
    void Space :: update()
    {
        if(buffers_queued < 1)
        {
            auto buf = vector<float>(frames);
            for(auto&& b: buf)
                b = 0.0f;
            
            //for(auto&& listener: sources)
                //for(auto&& source: listener.second)
            for(auto&& source: sources)
            {
                try{
                    shared_ptr<Source>(source)->update(this, buf);
                }catch(...){}
            }

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

    std::vector<std::vector<float>> extract(const std::vector<float>& buf, int channels)
    {
        std::vector<std::vector<float>> r;
        r.resize(channels);
        for(int i=0; i<channels; ++i)
        {
            unsigned sz = buf.size() / channels;
            r[i].resize(sz);
            for(unsigned j=0; j<sz; ++j)
                r[i][j] = buf[i + j*channels];
        }
        return r;
    }
    
}

