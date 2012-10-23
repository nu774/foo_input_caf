#ifndef UTIL_H
#define UTIL_H

#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#define NOMINMAX
#include <windows.h>
#include <shlwapi.h>
#include <io.h>
#include "utf8_codecvt_facet.hpp"
#include "strutil.h"

#define fseeko _fseeki64

namespace util {
    inline uint32_t b2host32(uint32_t n)
    {
	return _byteswap_ulong(n);
    }
    inline uint32_t h2big32(uint32_t n)
    {
	return _byteswap_ulong(n);
    }
    inline uint64_t b2host64(uint64_t n)
    {
	return _byteswap_uint64(n);
    }

    inline void throw_crt_error(const std::string &message)
    {
	std::stringstream ss;
	ss << message << ": " << std::strerror(errno);
	throw std::runtime_error(ss.str());
    }

    inline void throw_crt_error(const std::wstring &message)
    {
	std::stringstream ss;
	utf8_codecvt_facet u8;
	ss << strutil::w2m(message, u8) << ": " << std::strerror(errno);
	throw std::runtime_error(ss.str());
    }
    inline void throw_win32_error(const std::wstring &msg, DWORD code)
    {
	LPWSTR pszMsg;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		       FORMAT_MESSAGE_FROM_SYSTEM,
		       0,
		       code,
		       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		       (LPWSTR)&pszMsg,
		       0,
		       0);
	std::wstringstream ss;
	if (pszMsg) {
	    strutil::squeeze(pszMsg, L"\r\n");
	    ss << msg << L": " << pszMsg;
	    LocalFree(pszMsg);
	}
	else if (code < 0xfe00)
	    ss << code << L": " << msg;
	else
	    ss << std::hex << code << L": " << msg;
	throw std::runtime_error(strutil::w2m(ss.str(), utf8_codecvt_facet()));
    }
    inline void throw_win32_error(const std::string &msg, DWORD code)
    {
	throw_win32_error(strutil::m2w(msg, utf8_codecvt_facet()), code);
    }
    inline std::shared_ptr<FILE> open_file(const std::wstring &fname,
				    const wchar_t *mode)
    {
	FILE * fp = _wfopen(fname.c_str(), mode);
	if (!fp) throw_crt_error(fname);
	return std::shared_ptr<FILE>(fp, std::fclose);
    }
    inline std::wstring GetModuleFileNameX(HMODULE module=0)
    {
	std::vector<wchar_t> buffer(32);
	DWORD cclen = GetModuleFileNameW(module, &buffer[0],
					 static_cast<DWORD>(buffer.size()));
	while (cclen >= buffer.size() - 1) {
	    buffer.resize(buffer.size() * 2);
	    cclen = GetModuleFileNameW(module, &buffer[0],
				       static_cast<DWORD>(buffer.size()));
	}
	return std::wstring(&buffer[0], &buffer[cclen]);
    }
    inline std::wstring get_module_directory(HMODULE module=0)
    {
	std::wstring path = GetModuleFileNameX(module);
	const wchar_t *fpos = PathFindFileNameW(path.c_str());
	return path.substr(0, fpos - path.c_str());
    }
    inline void shift_file_content(FILE *fp, int64_t space)
    {
	std::fflush(fp);
	int64_t current_size = _filelengthi64(_fileno(fp));
	int64_t begin, end = current_size;
	char buf[8192];
	for (; (begin = std::max(0LL, end - 8192)) < end; end = begin) {
	    fseeko(fp, begin, SEEK_SET);
	    std::fread(buf, 1, end - begin, fp);
	    fseeko(fp, begin + space, SEEK_SET);
	    std::fwrite(buf, 1, end - begin, fp);
	}
    }
    /*
     * Loads CoreFoundation.dll constants.
     * Since DATA cannot be delayimp-ed, we have to manually
     * load it using .
     */
    inline void *load_cf_constant(const char *name)
    {
	HMODULE cf = GetModuleHandleA("CoreFoundation.dll");
	if (!cf)
	    util::throw_win32_error("CoreFouncation.dll", GetLastError());
	return GetProcAddress(cf, name);
    }
    inline
    std::wstring PathReplaceExtension(const std::wstring &path,
				      const wchar_t *ext)
    {
	const wchar_t *beg = path.c_str();
	const wchar_t *end = PathFindExtensionW(beg);
	std::wstring s(beg, end);
	if (ext[0] != L'.') s.push_back(L'.');
	s += ext;
	return s;
    }
}

#endif
