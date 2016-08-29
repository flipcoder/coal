#include <iostream>
#include "coal.h"
using namespace std;

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

    void Source :: add(std::shared_ptr<Buffer> buf)
    {
        buffers.emplace_back(buf);
    }

    void Source :: update()
    {
    }
    
    void Source :: restart()
    {
    }
    
    void Source :: play()
    {
    }
    
    void Source :: stop()
    {
    }
    
    void Source :: pause()
    {
    }

    void Space :: add(
        std::shared_ptr<Listener> listener, std::shared_ptr<Source> src
    ){
        sources[listener].push_back(src);
    }
    
    void Space :: update()
    {
    }
}

