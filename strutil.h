#ifndef STRUTIL_HPP_INCLUDED
#define STRUTIL_HPP_INCLUDED

#include <cwchar>
#include <string>
#include <vector>
#include <locale>
#include <stdexcept>
#include <algorithm>
#include <iterator>

typedef intptr_t ssize_t;

namespace strutil {
    char *strsep(char **strp, const char *sep);
    std::wstring &m2w(std::wstring &dst, const char *src, size_t srclen,
	    const std::codecvt<wchar_t, char, std::mbstate_t> &cvt);

    std::string &w2m(std::string &dst, const wchar_t *src, size_t srclen,
		   const std::codecvt<wchar_t, char, std::mbstate_t> &cvt);

    template <typename T, typename Conv>
    inline
    std::basic_string<T> strtransform(const std::basic_string<T> &s, Conv conv)
    {
	std::basic_string<T> result;
	std::transform(s.begin(), s.end(), std::back_inserter(result), conv);
	return result;
    }
    inline
    std::wstring wslower(const std::wstring &s)
    {
	return strtransform(s, towlower);
    }
    inline ssize_t strindex(const char *s, int ch)
    {
	const char *p = std::strchr(s, ch);
	return p ? p - s : -1;
    }
    inline ssize_t strindex(const wchar_t *s, int ch)
    {
	const wchar_t *p = std::wcschr(s, ch);
	return p ? p - s : -1;
    }
    template <typename T>
    void squeeze(T *str, const T *charset)
    {
	T *q = str;
	for (T *p = str; *p; ++p)
	    if (strindex(charset, *p) == -1)
		*q++ = *p;
	*q = 0;
    }

    inline
    std::wstring m2w(const std::string &src,
		     const std::codecvt<wchar_t, char, std::mbstate_t> &cvt)
    {
	std::wstring result;
	return m2w(result, src.c_str(), src.size(), cvt);
    }
    inline
    std::wstring m2w(const std::string &src)
    {
	typedef std::codecvt<wchar_t, char, std::mbstate_t> cvt_t;
	std::locale loc("");
	return m2w(src, std::use_facet<cvt_t>(loc));
    }
    inline
    std::string w2m(const std::wstring& src,
		    const std::codecvt<wchar_t, char, std::mbstate_t> &cvt)
    {
	std::string result;
	return w2m(result, src.c_str(), src.size(), cvt);
    }
    inline
    std::string w2m(const std::wstring &src)
    {
	typedef std::codecvt<wchar_t, char, std::mbstate_t> cvt_t;
	std::locale loc("");
	return w2m(src, std::use_facet<cvt_t>(loc));
    }

    class Tokenizer {
	std::vector<char> m_buffer;
	const char *m_sep;
	char *m_tok;
    public:
	Tokenizer(const std::string &s, const char *sep)
	    : m_sep(sep)
	{
	    m_buffer.resize(s.size() + 1);
	    std::memcpy(&m_buffer[0], s.data(), m_buffer.size() - 1);
	    m_tok = &m_buffer[0];
	}
	char *next()
	{
	    return strsep(&m_tok, m_sep);
	}
	char *rest()
	{
	    return m_tok;
	}
    };
}

#endif
