#pragma once

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) || defined(_M_AMD64)
#define _CPU_X64
#elif defined(__i386__) || defined(_M_IX86)
#define _CPU_X86
#else
#error "Unsupported CPU"
#endif
