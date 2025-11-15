#pragma once
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

namespace Lzip::File {
    typedef uint8_t u8;
    typedef uint64_t u64;
    using std::filesystem::path, std::span, std::min, std::ofstream, std::ifstream, std::vector, std::error_code;

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
}