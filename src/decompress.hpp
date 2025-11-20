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
    typedef uint64_t u64;
    using std::array, std::cout, std::cin, std::fixed, std::setprecision, std::to_string, std::string, std::vector, std::span, std::filesystem::path, std::filesystem::exists, std::filesystem::is_regular_file, std::chrono::steady_clock, std::chrono::duration_cast, std::chrono::milliseconds, Util::FileReader, Util::FileWriter, Util::normalize, Util::IO_CHUNK_SIZE, Util::printCodes, Huffman::deserialize, Huffman::IndexedNodeR, Huffman::HuffmanCode, Huffman::getCodeMap;
    inline void decompress(span<const u8> data, const vector<IndexedNodeR>& tree, vector<u8>& result, u64& writtenBytes, u16& currentNode, u64 maxBytes) noexcept;

    inline constexpr const char* INVALID_LZIP_FILE_ERROR = "输入文件不是有效的 Lzip 文件。";
    [[nodiscard]] inline bool decompressFile(const string& inputFile, const string& outputFile) noexcept {
        steady_clock::time_point startTime = steady_clock::now();
        path inputPath(inputFile);
        if (!normalize(inputPath)) {
            Util::setError(string("输入文件路径有误：") + inputFile);
            return false;
        }
        path outputPath;
        if (outputFile.empty()) {
            outputPath = inputFile;
            const string fileName = outputPath.filename().string();
            if (fileName.find(".lzip") == fileName.size() - 5) outputPath = outputPath.parent_path() / fileName.substr(0, fileName.size() - 5);
        }
        else outputPath = outputFile;
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
            Util::setError(string("无法打开输入文件：") + STR(inputPath));
            return false;
        }
        FileWriter writer(outputPath);
        if (!writer.file.is_open()) {
            Util::setError(string("无法打开输出文件：") + STR(outputPath));
            return false;
        }
        if (reader.fileSize < 272) {
            Util::setError(INVALID_LZIP_FILE_ERROR);
            return false;
        }
        //Check magic number
        array<u8, 4> magic{};
        reader.file.read(reinterpret_cast<char*>(magic.data()), 4);
        if (magic[0] != LZIP_MAGIC[0] || magic[1] != LZIP_MAGIC[1] || magic[2] != LZIP_MAGIC[2] || magic[3] != LZIP_MAGIC[3]) {
            Util::setError(INVALID_LZIP_FILE_ERROR);
            return false;
        }
        //Skip version
        reader.file.read(reinterpret_cast<char*>(magic.data()), 4);
        //Read size
        array<u8, 8> sizeBuffer{};
        reader.file.read(reinterpret_cast<char*>(sizeBuffer.data()), 8);
        const u64 originalSize = Util::readIntLE<u64>(sizeBuffer.data());
        //Read code length table
        static array<u8, 256> codeLens{};
        codeLens.fill(0);
        reader.file.read(reinterpret_cast<char*>(codeLens.data()), 256);
        //Deserialize Huffman tree
        vector<IndexedNodeR> huffmanTree;
        deserialize(codeLens, huffmanTree);
        static array<HuffmanCode, 256> codeMap;
        codeMap.fill({});
        getCodeMap(huffmanTree, codeMap);
        printCodes(codeMap);
        vector<u8> inputData, outputData;
        u64 length = reader.nextChunk(inputData, IO_CHUNK_SIZE), ax = 0, writtenBytes = 0;
        u16 currentNode = 0;
        while (true) {
            if (length == 0) break;
            if ((ax & 63) == 0) cout << "正在解压区块 #" + to_string(ax) << '\n';
            decompress(inputData, huffmanTree, outputData, writtenBytes, currentNode, originalSize);
            writer.writeChunk(outputData);
            outputData.clear();
            inputData.clear();
            length = reader.nextChunk(inputData, IO_CHUNK_SIZE);
            ax++;
        }
        cout << "解压完成，耗时 " << duration_cast<milliseconds>(steady_clock::now() - startTime).count() << " 毫秒\n输出文件：" << STR(outputPath) << '\n';
        return true;
    }

    inline void decompress(span<const u8> data, const vector<IndexedNodeR>& tree, vector<u8>& result, u64& writtenBytes, u16& currentNode, u64 maxBytes) noexcept {
        for (u64 i = 0; i < data.size() * 8; i++) {
            currentNode = data[i >> 3] & (1 << (7 - (i & 7))) ? tree[currentNode].right : tree[currentNode].left;
            if (tree[currentNode].isLeaf) {
                result.push_back(tree[currentNode].byte);
                writtenBytes++;
                if (writtenBytes == maxBytes) return;
                currentNode = 0;
            }
        }
    }
}