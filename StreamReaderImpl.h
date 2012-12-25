#pragma once
#include <stdint.h>
#include "../SDK/foobar2000.h"
#include "strutil.h" /* for ssize_t */

class StreamReaderImpl: public IStreamWriter {
    service_ptr_t<file> m_pfile;
    abort_callback_dummy m_abort;
public:
    StreamReaderImpl(service_ptr_t<file> &pfile)
        : m_pfile(pfile)
    {}
    ssize_t read(void *data, size_t count)
    {
        return m_pfile->read(data, count, m_abort);
    }
    int seek(int64_t off, int whence)
    {
        static const file::t_seek_mode tab[] = {
            file::seek_from_beginning,
            file::seek_from_current,
            file::seek_from_eof
        };
        m_pfile->seek_ex(off, tab[whence], m_abort);
        return 0; // XXX
    }
    int64_t get_position()
    {
        return m_pfile->get_position(m_abort);
    }
    int64_t get_size()
    {
        return m_pfile->get_size(m_abort);
    }
    ssize_t write(const void *data, size_t count)
    {
        m_pfile->write(data, count, m_abort);
        return count; // XXX
    }
    int set_size(int64_t size)
    {
        m_pfile->resize(size, m_abort);
        return 0; // XXX
    }
};

