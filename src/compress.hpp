#pragma once
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "huffman.hpp"
#include "meta.hpp"
#include "utils.hpp"

namespace Lzip {
    typedef uint8_t u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;
    using std::array, std::cout, std::cin, std::fixed, std::setprecision, std::to_string, std::string, std::vector, std::span, std::filesystem::path, std::chrono::steady_clock, std::chrono::duration_cast, std::chrono::milliseconds, Util::FileReader, Util::FileWriter, Util::normalize, Util::IO_CHUNK_SIZE, Util::printCodes, Huffman::HuffmanCode, Huffman::updateFrequency, Huffman::getHuffmanCode;
    inline void compress(vector<u8>& result, span<const u8> input, const array<HuffmanCode, 256>& huffmanCodes, u8& previousOffset) noexcept;

    [[nodiscard]] inline bool compressFile(const string& inputFile, const string& outputFile) noexcept {
        steady_clock::time_point startTime = steady_clock::now();
        path inputPath(inputFile);
        if (!normalize(inputPath)) {
            Util::setError(string("输入文件路径有误：") + inputFile);
            return false;
        }
        path outputPath = outputFile.empty() ? inputFile + ".lzip" : outputFile;
        if (!normalize(outputPath)) {
            Util::setError(string("输出文件路径有误：") + outputFile);
            return false;
        }
        if (exists(outputPath) && is_regular_file(outputPath)) {
            cout << "输出文件 \"" << STR(outputPath) << "\" 已存在，是否覆盖？(y/n)：";
            string input;
            cin >> input;
            if (input != "y" && input != "Y") {
                cout << "操作已取消。\n";
                return true;
            }
        }
        FileReader reader(inputPath);
        if (!reader.file.is_open()) {
            Util::setError(string("无法打开输入文件：") + reinterpret_cast<const char*>(inputPath.u8string().c_str()));
            return false;
        }
        FileWriter writer(outputPath);
        if (!writer.file.is_open()) {
            Util::setError(string("无法打开输出文件：") + reinterpret_cast<const char*>(outputPath.u8string().c_str()));
            return false;
        }
        static array<u64, 256> frequencies{};
        frequencies.fill(0);
        //Get byte frequencies
        vector<u8> inputData_header;
        u64 length = reader.nextChunk(inputData_header, IO_CHUNK_SIZE), ax = 0;
        while (true) {
            if (length == 0) break;
            if ((ax & 63) == 0) cout << "正在统计区块 #" + to_string(ax) << '\n';
            updateFrequency(inputData_header, frequencies);
            inputData_header.clear();
            length = reader.nextChunk(inputData_header, IO_CHUNK_SIZE);
            ax++;
        }
        ax = 0;
        //Generate Huffman codes
        static array<HuffmanCode, 256> huffmanCodes;
        huffmanCodes.fill({});
        u16 presentedByteCount;
        if (!getHuffmanCode(frequencies, huffmanCodes, presentedByteCount)) {
            Util::setError("无法生成霍夫曼树。");
            return false;
        }
        printCodes(huffmanCodes);
        inputData_header.insert(inputData_header.end(), LZIP_MAGIC.begin(), LZIP_MAGIC.end());
        Util::writeIntLE(inputData_header, LZIP_VERSION);
        Util::writeIntLE(inputData_header, static_cast<u64>(reader.fileSize));
        Huffman::serialize(huffmanCodes, inputData_header);
        writer.writeChunk(inputData_header);
        if (presentedByteCount > 0) {
            reader.reset();
            inputData_header.clear();
            vector<u8> outputData;
            length = reader.nextChunk(inputData_header, IO_CHUNK_SIZE);
            u8 offset = 0;
            while (true) {
                if (length == 0) break;
                if ((ax & 255) == 0) cout << "正在压缩区块 #" + to_string(ax) << '\n';
                compress(outputData, inputData_header, huffmanCodes, offset);
                if (offset > 0) {
                    u8 lastByte = outputData.back();
                    outputData.pop_back();
                    writer.writeChunk(outputData);
                    outputData.clear();
                    outputData.push_back(lastByte);
                }
                else {
                    writer.writeChunk(outputData);
                    outputData.clear();
                }
                inputData_header.clear();
                length = reader.nextChunk(inputData_header, IO_CHUNK_SIZE);
                ax++;
            }
            if (outputData.size() > 0) writer.writeChunk(outputData);
        }
        cout << "压缩完成，耗时 " << duration_cast<milliseconds>(steady_clock::now() - startTime).count() << " 毫秒\n输出文件：" << reinterpret_cast<const char*>(outputPath.u8string().c_str()) << "\n压缩比：" << fixed << setprecision(2) << (static_cast<double>(writer.fileSize()) / reader.fileSize) * 100 << "%\n";
        return true;
    }

    inline void compress(vector<u8>& result, span<const u8> input, const array<HuffmanCode, 256>& huffmanCodes, u8& previousOffset) noexcept {
        if (input.size() == 0) return;
        u64 cursor;
        switch (result.size()) {
            case 0:
                cursor = 0;
                break;
            case 1:
                cursor = previousOffset;
                break;
            default:
                cursor = ((result.size() - 1) << 3) + previousOffset;
                break;
        }
        for (u64 i = 0; i < input.size(); i++) {
            const u8 byte = input[i], length = huffmanCodes[byte].codeLen;
            const u64 code = huffmanCodes[byte].code, newCursor = cursor + length;
            //We're crossing byte boundary
            if (cursor == 0 || (newCursor - 1) >> 3 > (cursor - 1) >> 3) [[unlikely]] {
                u8 bitsLeft;
                //We will write to a brand new byte
                if ((cursor & 7) == 0) {
                    result.push_back(0);
                    if (length > 8) [[unlikely]] {
                        result[cursor >> 3] |= static_cast<u8>(code >> (length - 8));
                        cursor += 8;
                        bitsLeft = length - 8;
                    }
                    //Length is less than 8, so just write and continue
                    else [[likely]] {
                        result[cursor >> 3] |= static_cast<u8>(code << (8 - length));
                        cursor = newCursor;
                        continue;
                    }
                }
                //We will also be writing to the old byte
                else {
                    const u8 bitsInLastByte = 8 - static_cast<u8>(cursor & 7);
                    result[cursor >> 3] |= static_cast<u8>(code >> (length - bitsInLastByte));
                    cursor += bitsInLastByte;
                    bitsLeft = length - bitsInLastByte;
                }
                while (bitsLeft >= 8) {
                    result.push_back(static_cast<u8>(code >> (bitsLeft - 8)));
                    cursor += 8;
                    bitsLeft -= 8;
                }
                if (bitsLeft > 0) {
                    result.push_back(static_cast<u8>(code << (8 - bitsLeft)));
                    cursor += bitsLeft;
                }
            }
            //We're staying in the same byte
            else [[likely]] {
                result[cursor >> 3] |= static_cast<u8>(code << (8 - length - static_cast<u8>(cursor & 7)));
                cursor = newCursor;
            }
        }
        previousOffset = static_cast<u8>(cursor & 7);
    }
}