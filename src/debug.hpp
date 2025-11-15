#pragma once
#include <array>
#include <bitset>
#include <iostream>
#include <string>

#include "huffman.hpp"

namespace Debug {
    typedef uint8_t u8;
    typedef uint64_t u64;
    using std::array, std::cout, std::flush, std::bitset, std::to_string, std::string, Lzip::Huffman::HuffmanCode;

    inline void printCodes(const array<HuffmanCode, 256>& codes) noexcept {
        for (u64 i = 0; i < 256; i++) if (codes[i].codeLen > 0) {
            cout << "字节 " << to_string(i) << "：" << bitset<64>(codes[i].code).to_string().substr(64 - codes[i].codeLen) << "\n";
            cout << flush;
        }
    }
}