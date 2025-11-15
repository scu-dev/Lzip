#pragma once
#include <array>
#include <cstdint>

typedef uint8_t u8;
typedef uint32_t u32;
using std::array;

inline constexpr const char
    //I f**king hate C++ syntax forcing pointers to be a part of the variable name instead of the type!
    * LZIP_COPYRIGHT_NOTICE = "25 秋 数据结构课程设计 第三次实验\nLJM12914\nhttps://github.com/ljm12914",
    * LZIP_APP_NAME = "Lzip",
    * LZIP_SEMATIC_VERSION = "1.0.0";

inline constexpr array<u8, 4> LZIP_MAGIC = { 'L', 'z', 'i', 'p' };
inline constexpr u32 LZIP_VERSION = 1u;