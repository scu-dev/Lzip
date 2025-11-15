#pragma once

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__NT__)
    #define _LZIP_WINDOWS 1
    #include <windows.h> // IWYU pragma: keep
#else
    #define _LZIP_UNIX 1
    #include <unistd.h> // IWYU pragma: keep
#endif