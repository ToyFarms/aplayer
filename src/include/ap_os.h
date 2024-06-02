#ifndef __AP_OS_H
#define __AP_OS_H

#define OS_WINDOWS     0
#define OS_LINUX       1
#define OS_APPLE       2
#define OS_UNSUPPORTED 3

#if defined(_WIN32) || defined(WIN32)
#    define OS_TYPE OS_WINDOWS
#elif defined(__APPLE__)
#    define OS_TYPE OS_APPLE
#elif defined(__linux__)
#    define OS_TYPE OS_LINUX
#else
#    define OS_TYPE OS_UNSUPPORTED
#endif // defined(_WIN32) || defined(WIN32)

static const char *os_str_map[] = {
    "Windows",
    "Linux",
    "Apple",
    "Unsupported",
};

#define OS_TYPE_TOSTR(os_type)                                                 \
    os_type > (sizeof(os_str_map) / sizeof(os_str_map[0]))                     \
        ? "Unknown OS type"                                                    \
        : os_str_map[os_type]

void prepare_app_arguments(int *argc_ptr, char ***argv_ptr);

#endif /* __AP_OS_H */
