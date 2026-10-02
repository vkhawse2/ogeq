// OgeqTrace.h -- lightweight file logging for APO debugging.
//
// Only active when OGEQ_APO_TRACE is defined. Writes to
// C:\ProgramData\OGEQ\apo-trace.log. Safe to call from Initialize/
// LockForProcess (NOT from APOProcess -- file I/O is not RT-safe).

#pragma once

#ifdef OGEQ_APO_TRACE
#include <windows.h>
#include <cstdio>
#include <cstdarg>

namespace ogeq {

inline void TraceLog(const wchar_t* fmt, ...) {
    wchar_t buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);

    // Timestamp + thread ID
    SYSTEMTIME st;
    GetLocalTime(&st);
    DWORD tid = GetCurrentThreadId();
    wchar_t line[640];
    _snwprintf_s(line, _TRUNCATE, L"[%02d:%02d:%02d.%03d] [tid=%lu] %s\r\n",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, tid, buf);

    // Append to log file (synchronous, simple -- debug only)
    CreateDirectoryW(L"C:\\ProgramData\\OGEQ", nullptr);
    HANDLE h = CreateFileW(L"C:\\ProgramData\\OGEQ\\apo-trace.log",
                           FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        // Write as UTF-16 with BOM on first write
        LARGE_INTEGER size{};
        if (GetFileSizeEx(h, &size) && size.QuadPart == 0) {
            const BYTE bom[] = { 0xFF, 0xFE };
            WriteFile(h, bom, 2, &written, nullptr);
        }
        WriteFile(h, line, (DWORD)(wcslen(line) * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(h);
    }
}

} // namespace ogeq

#define OGEQ_TRACE(...) ogeq::TraceLog(__VA_ARGS__)
#else
#define OGEQ_TRACE(...) ((void)0)
#endif
