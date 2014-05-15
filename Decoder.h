#ifndef DECODER_H
#define DECODER_H

#include <memory>
#include "CAFFile.h"

struct IDecoder {
    virtual ~IDecoder() {}
    virtual t_size set_stream_property(const GUID type, t_size p1,
                                       const void * p2, t_size p2size) = 0;
    virtual void get_info(file_info &info) = 0;
    virtual unsigned get_max_frame_dependency() = 0;
    virtual void decode(const void *buffer, t_size bytes,
                        audio_chunk &chunk, abort_callback &abort) = 0;
    virtual void reset_after_seek() = 0;
    virtual bool analyze_first_frame_supported() = 0;
    virtual void analyze_first_frame(const void *buffer, t_size bytes,
                                     abort_callback &abort) = 0;
    static std::shared_ptr<IDecoder>
        create_decoder(std::shared_ptr<CAFFile> &demuxer,
                       abort_callback &abort);
};

struct DecoderBase: public IDecoder {
    t_size set_stream_property(const GUID type, t_size p1,
                               const void * p2, t_size p2size) { return 0; }
    unsigned get_max_frame_dependency() { return 0; }
    void reset_after_seek() {}
    bool analyze_first_frame_supported() { return false; }
    void analyze_first_frame(const void *buffer, t_size bytes,
                             abort_callback &abort) {}
};

#endif
