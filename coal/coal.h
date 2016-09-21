#ifndef COAL_H_L5UOCN9W
#define COAL_H_L5UOCN9W

#include <portaudio.h>
#include <sndfile.h>
#include <glm/glm.hpp>
#include <vector>
#include <map>
#include <atomic>
#include <boost/circular_buffer.hpp>
#include <boost/lockfree/spsc_queue.hpp>

struct Coal
{
    Coal();
    ~Coal();

    Coal(const Coal&) = delete;
    Coal(Coal&&) = default;
    Coal& operator=(const Coal&) = delete;
    Coal& operator=(Coal&&) = default;
};

namespace coal {
    
    struct Space;
    
    void init();
    void deinit();
    
    struct Buffer
    {
        Buffer() {}
        Buffer(std::string fn);
        ~Buffer() {}
        
        Buffer(const Buffer&) = default;
        Buffer(Buffer&&) = default;
        Buffer& operator=(const Buffer&) = default;
        Buffer& operator=(Buffer&&) = default;
        
        float gain = 1.0f;
        std::vector<float> buffer;
        int rate = 0;
        int channels = 1;
    };
    
    struct Stream
    {
        Stream(){}
        Stream(std::string fn);
        ~Stream();

        Stream(const Stream&) = default;
        Stream(Stream&&) = default;
        Stream& operator=(const Stream&) = default;
        Stream& operator=(Stream&&) = default;

        void seek(float pos);
        void update();

        boost::circular_buffer<std::vector<float>> buffers;

        SNDFILE* m_pFile;
        std::function<void(const void*, void*, void*)> callback;
        float t = 0.0f;
        float t_in_buffer = 0.0f;
        int channels = 1;
        //unsigned len = 0;
        unsigned len_in_buffer = 0;
        int sz = 0;
        int rate = 0;
        bool ended = false;
        const int buffer_capacity=2;
        const int buffer_size = 4096 * 8;
    };

    struct Source
    {
        struct BufferInfo
        {
            BufferInfo(std::shared_ptr<Buffer> buf):
                buffer(buf)
            {}
            BufferInfo(const BufferInfo&) = default;
            BufferInfo(BufferInfo&&) = default;
            BufferInfo& operator=(const BufferInfo&) = default;
            BufferInfo& operator=(BufferInfo&&) = default;

            std::shared_ptr<Buffer> buffer;
            float t = 0.0f;
            bool enabled = true;
            bool ended = false;
            float gain = 1.0f;
            float pitch = 0.0f;
        };

        struct StreamInfo
        {
            StreamInfo(std::shared_ptr<Stream> strm):
                stream(strm)
            {}
            StreamInfo(const StreamInfo&) = default;
            StreamInfo(StreamInfo&&) = default;
            StreamInfo& operator=(const StreamInfo&) = default;
            StreamInfo& operator=(StreamInfo&&) = default;

            std::shared_ptr<Stream> stream;
            float t = 0.0f;
            bool enabled = true;
            bool ended = false;
            float gain = 1.0f;
            float pitch = 0.0f;
        };

        bool playing = false;
        glm::vec3 pos;
        std::vector<BufferInfo> buffers;
        std::vector<StreamInfo> streams;
        boost::circular_buffer<float> history;
        
        Source() {}
        Source(const Source&) = default;
        Source(Source&&) = default;
        Source& operator=(const Source&) = default;
        Source& operator=(Source&&) = default;

        void add(std::shared_ptr<Buffer> buf);
        void add(std::shared_ptr<Stream> strm);
        void update(Space* s, std::vector<float>& buf);
        void restart();
        void play();
        void stop();
        void pause();
    };

    struct Listener
    {
        glm::mat4 matrix;

        Listener() {}
        Listener(const Listener&) = default;
        Listener(Listener&&) = default;
        Listener& operator=(const Listener&) = default;
        Listener& operator=(Listener&&) = default;
    };

    struct Space
    {
        Space();
        ~Space();
        Space(const Space&) = default;
        Space(Space&&) = default;
        Space& operator=(const Space&) = default;
        Space& operator=(Space&&) = default;
        
        std::map<
            std::shared_ptr<Listener>,
            std::vector<std::shared_ptr<Source>>
        > sources;

        void add(std::shared_ptr<Listener> listener, std::shared_ptr<Source> src);
        void update();

        int freq = 44100;
        int frames = 512;

        bool delay = false;
        int delay_count = 3;
        float delay_speed = 0.5;
        float delay_mix = 0.5;

        bool reverb = false;
        int reverb_count = 30;
        float reverb_speed = 20.0;
        float reverb_mix = 0.25;

    private:
        
        static int cb_sample(
            const void* in, void* out,
            unsigned long framecount,
            const PaStreamCallbackTimeInfo* tinfo,
            PaStreamCallbackFlags flags, void* user
        );
        int sample(
            const void* in, void* out,
            unsigned long framecount,
            const PaStreamCallbackTimeInfo* tinfo,
            PaStreamCallbackFlags flags
        );

        boost::lockfree::spsc_queue<std::vector<float>> buffers;
        std::atomic<int> buffers_queued = ATOMIC_VAR_INIT(0);
        PaStream* stream = nullptr;
        boost::circular_buffer<std::vector<float>> sample_history;
    };
}

#endif

