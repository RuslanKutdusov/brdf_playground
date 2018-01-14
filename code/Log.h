#pragma once
#include <stdio.h>

inline void LogStdOut(const char* format, ...)
{
	char buf[2048];
	va_list args;
	va_start(args, format);
	int len = vsprintf_s(buf, format, args);
	va_end(args);
	fwrite(buf, 1, len, stdout);
	OutputDebugStringA(buf);
}


inline void LogStdErr(const char* format, ...)
{
	char buf[2048];
	va_list args;
	va_start(args, format);
	int len = vsprintf_s(buf, format, args);
	va_end(args);
	fwrite(buf, 1, len, stderr);
	OutputDebugStringA(buf);
}


#define Assert(condition)                                                            \
	if (!(bool)(condition))                                                          \
	{                                                                                \
		LogStdErr("%s(%u): Assertion failed: %s\n", __FILE__, __LINE__, #condition); \
		__debugbreak();                                                              \
	}

#define AssertMsg(condition, msg)                                                    \
	if (!(bool)(condition))                                                          \
	{                                                                                \
		LogStdErr("%s(%u): Assertion failed: %s\n", __FILE__, __LINE__, #condition); \
		LogStdErr("%s\n", msg);                                                      \
		__debugbreak();                                                              \
	}

#if _DEBUG
#define AssertDebug(condition) Assert(condition)
#define AssertMsgDebug(condition, msg) AssertMsg(condition, msg)
#else
#define AssertDebug(condition) ((void)0)
#define AssertMsgDebug(condition, msg) ((void)0)
#endif

#define BreakMsg(msg) AssertMsg(false, msg)
