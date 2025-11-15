#pragma once
#include <array>
#include <bit>
#include <bitset>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include "huffman.hpp"

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__NT__)
    #define _LZIP_WINDOWS 1
    #include <windows.h> // IWYU pragma: keep
#else
    #define _LZIP_UNIX 1
    #include <unistd.h> // IWYU pragma: keep
#endif

namespace Lzip::Util {
    typedef uint8_t u8;
    typedef uint64_t u64;
    using std::array, std::endian, std::bit_cast, std::cout, std::flush, std::bitset, std::to_string, std::string, std::filesystem::path, std::span, std::min, std::ofstream, std::ifstream, std::vector, std::error_code, std::is_integral_v, Huffman::HuffmanCode;

    #define STR(p) reinterpret_cast<const char*>(p.u8string().c_str())

    inline void printCodes(const array<HuffmanCode, 256>& codes) noexcept {
        for (u64 i = 0; i < 256; i++) if (codes[i].codeLen > 0) {
            cout << "字节 " << to_string(i) << "：" << bitset<64>(codes[i].code).to_string().substr(64 - codes[i].codeLen) << "\n";
            cout << flush;
        }
    }

    inline string errorMessage;

    [[nodiscard]] inline string getLastError() noexcept { return errorMessage; }
    inline void setError(const string& err) noexcept { errorMessage = err; }

    inline constexpr u64 IO_CHUNK_SIZE = 1048576;

    struct FileReader {
        ifstream file;
        size_t fileSize{0};

        [[nodiscard]] explicit FileReader(const path& filePath) noexcept {
            file.open(filePath, std::ios::binary);
            if (!file.is_open()) return;
            file.seekg(0, std::ios::end);
            fileSize = static_cast<size_t>(file.tellg());
            file.seekg(0, std::ios::beg);
        }

        [[nodiscard]] u64 nextChunk(vector<u8>& result, u64 size) noexcept {
            if (!file.is_open()) return 0;
            const size_t toRead = static_cast<size_t>(min<u64>(size, fileSize - static_cast<size_t>(file.tellg())));
            if (toRead == 0) return 0;
            vector<u8> buffer(toRead);
            file.read(reinterpret_cast<char*>(buffer.data()), toRead);
            result.insert(result.end(), buffer.begin(), buffer.end());
            return toRead;
        }

        void reset() noexcept {
            if (!file.is_open()) return;
            file.seekg(0, std::ios::beg);
        }
    };

    struct FileWriter {
        ofstream file;

        [[nodiscard]] explicit FileWriter(const path& filePath) noexcept {
            file.open(filePath, std::ios::binary);
            if (!file.is_open()) return;
        }

        void writeChunk(span<const u8> data) noexcept {
            if (!file.is_open()) return;
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
        }

        [[nodiscard]] u64 fileSize() noexcept {
            if (!file.is_open()) return 0;
            const auto currentPos = file.tellp();
            file.seekp(0, std::ios::end);
            const auto size = file.tellp();
            file.seekp(currentPos);
            return static_cast<u64>(size);
        }
    };

    inline bool normalize(path& p) noexcept {
        error_code ec;
        p = absolute(p, ec);
        if (ec) return false;
        p = p.lexically_normal();
        p.make_preferred();
        return true;
    }

    template <typename T> requires is_integral_v<T>
    inline void writeIntLE(vector<u8>& result, T value) noexcept {
        const auto arr = bit_cast<array<u8, sizeof(value)>>(value);
        if (endian::native == endian::little) [[likely]] result.insert(result.end(), arr.begin(), arr.end());
        else [[unlikely]] result.insert(result.end(), arr.rbegin(), arr.rend());
    }

    template <typename T> requires is_integral_v<T>
    inline T readIntLE(const u8* data) noexcept {
        array<u8, sizeof(T)> arr{};
        if (endian::native == endian::little) [[likely]] memcpy(arr.data(), data, sizeof(T));
        else [[unlikely]] for (size_t i = 0; i < sizeof(T); i++) arr[sizeof(T) - 1 - i] = data[i];
        return bit_cast<T>(arr);
    }
}