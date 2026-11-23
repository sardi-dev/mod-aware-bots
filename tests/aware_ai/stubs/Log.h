// Test stub for SkyFire's Log.h: the SF_LOG_* macros print to stderr and count errors.
#ifndef AWARE_AI_TEST_STUB_LOG_H
#define AWARE_AI_TEST_STUB_LOG_H

#include <atomic>
#include <cstdio>

inline std::atomic<int>& StubLogErrors()
{
    static std::atomic<int> errors{0};
    return errors;
}

#define SF_LOG_DEBUG(filter, ...) ((void) 0)
#define SF_LOG_INFO(filter, ...) (std::fprintf(stderr, "[info %s] ", filter), std::fprintf(stderr, __VA_ARGS__), std::fputc('\n', stderr))
#define SF_LOG_WARN(filter, ...) (std::fprintf(stderr, "[warn %s] ", filter), std::fprintf(stderr, __VA_ARGS__), std::fputc('\n', stderr))
#define SF_LOG_ERROR(filter, ...) (++StubLogErrors(), std::fprintf(stderr, "[error %s] ", filter), std::fprintf(stderr, __VA_ARGS__), std::fputc('\n', stderr))

#endif
