#ifndef _LIBOS_H
#define _LIBOS_H

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define AP_WINDOWS
#elif defined(__APPLE__)
#define AP_MACOS
#elif defined(__linux__)
#define AP_LINUX
#endif // WIN32

#endif // _LIBOS_H
