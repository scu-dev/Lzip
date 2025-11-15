#pragma once
#include <array>
#include <algorithm>
#include <cassert>
#include <queue>
#include <span>
#include <sys/stat.h>
#include <vector>

namespace Lzip::Huffman {
    typedef uint8_t u8;
    typedef int16_t i16;
    typedef uint16_t u16;
    typedef uint64_t u64;
    using std::array, std::span, std::vector, std::sort, std::priority_queue;

    struct IndexedNode {
        u64 frequency{0};
        u16 left{0}, right{0};
        u8 byte{0};
        bool isLeaf{false};
    };

    struct IndexedNodeR {
        u16 left{0xFFFFu}, right{0xFFFFu};
        u8 byte{0};
        bool isLeaf{false};
    };

    //`codeLen` is `u16` because it may exceed `u8` before length reduction, but since `maxCodeLen` is 64, we can use `u8` safely after that
    struct HuffmanCode {
        u64 code{0};
        u16 codeLen{0};
    };

    struct ByteFrequency {
        u64 frequency{0};
        u8 byte{0};
    };

    //For deserialization only because `codeLen` is `u8`
    struct ByteCodeLen {
        u8 byte{0}, codeLen{0};
    };

    struct IndexCodeLen {
        u16 index{0}, codeLen{0};
    };

    struct IndexCodeCodeLen {
        u64 code{0};
        u16 index{0};
        u8 codeLen{0};
    };

    struct IndexedNodeComparator {
        const vector<IndexedNode>& nodesPtr;
        [[nodiscard]] IndexedNodeComparator(const vector<IndexedNode>& nodes) noexcept : nodesPtr(nodes) {}

        [[nodiscard]] bool operator()(const u16& a, const u16& b) const noexcept {
            const auto& nodeA = nodesPtr[a];
            const auto& nodeB = nodesPtr[b];
            if (nodeA.frequency != nodeB.frequency) return nodeA.frequency > nodeB.frequency;
            //Prefer leaf nodes
            if (nodeA.isLeaf != nodeB.isLeaf) return nodeA.isLeaf < nodeB.isLeaf;
            return nodeA.byte < nodeB.byte;
        }
    };

    inline void updateFrequency(span<const u8> data, array<u64, 256>& frequencies) noexcept {
        for (u64 i = 0; i < data.size(); i++) frequencies[data[i]]++;
    }

    [[nodiscard]] inline bool getHuffmanCode(const array<u64, 256>& frequencies, array<HuffmanCode, 256>& result, u16& presentedByteCount, u16 maxCodeLen = 24) noexcept {
        if (maxCodeLen > 64) return false;
        result.fill({});
        static array<ByteFrequency, 256> byteFrequencies{};
        presentedByteCount = 0;
        for (u16 i = 0; i < 256; i++) if (frequencies[i] > 0) {
            byteFrequencies[presentedByteCount].frequency = frequencies[i];
            byteFrequencies[presentedByteCount].byte = static_cast<u8>(i);
            presentedByteCount++;
        }
        switch (presentedByteCount) {
            case 0: return true;
            case 1:
                result[byteFrequencies[0].byte].code = 0;
                result[byteFrequencies[0].byte].codeLen = 1;
                return true;
            default: break;
        }
        vector<IndexedNode> nodes;
        nodes.reserve(static_cast<u64>(presentedByteCount) * 2);
        for (u16 i = 0; i < presentedByteCount; i++) nodes.emplace_back(byteFrequencies[i].frequency, 0xFFFFu, 0xFFFFu, byteFrequencies[i].byte, true);
        priority_queue<u16, vector<u16>, IndexedNodeComparator> nodePQ((IndexedNodeComparator(nodes)));
        for (u16 i = 0; i < presentedByteCount; i++) nodePQ.push(static_cast<u16>(i));
        //Construct the tree
        while (nodePQ.size() > 1) {
            u16 index1 = nodePQ.top();
            nodePQ.pop();
            u16 index2 = nodePQ.top();
            nodePQ.pop();
            IndexedNode newNode{
                .frequency = nodes[index1].frequency + nodes[index2].frequency,
                .left = index1,
                .right = index2,
                .byte = 0,
                .isLeaf = false,
            };
            u16 newIndex = static_cast<u16>(nodes.size());
            nodes.push_back(newNode);
            nodePQ.push(newIndex);
        }
        u16 actualMaxCodeLen = 0;
        { //Calculate lengths
            vector<IndexCodeLen> stack;
            stack.push_back({nodePQ.top(), 0});
            while (!stack.empty()) {
                IndexCodeLen currentIndex = stack.back();
                stack.pop_back();
                const IndexedNode& currentNode = nodes[currentIndex.index];
                if (currentNode.isLeaf) {
                    result[currentNode.byte].codeLen = currentIndex.codeLen;
                    if (currentIndex.codeLen > actualMaxCodeLen) actualMaxCodeLen = currentIndex.codeLen;
                }
                else {
                    if (currentNode.left != 0xFFFFu) stack.emplace_back(currentNode.left, currentIndex.codeLen + 1);
                    if (currentNode.right != 0xFFFFu) stack.emplace_back(currentNode.right, currentIndex.codeLen + 1);
                }
            }
        }
        vector<u16> codeLenCounts(actualMaxCodeLen + 1, 0);
        //Count each length's occurrences
        for (u16 i = 0; i < 256; i++) {
            u16 temp = result[i].codeLen;
            if (temp > 0) {
                if (temp > actualMaxCodeLen) temp = actualMaxCodeLen;
                codeLenCounts[temp]++;
            }
        }
        //Reduce lengths
        if (actualMaxCodeLen > maxCodeLen) {
            for (u16 i = actualMaxCodeLen; i > maxCodeLen; i--) while (codeLenCounts[i] > 0) {
                //Move two codes from length len to one code of length len - 1
                if (codeLenCounts[i] < 2) return false;
                codeLenCounts[i] -= 2;
                codeLenCounts[static_cast<size_t>(i) - 1]++;
                u16 shallowLeafCodeLen = i - 2;
                while (shallowLeafCodeLen > 0 && codeLenCounts[shallowLeafCodeLen] == 0) shallowLeafCodeLen--;
                if (shallowLeafCodeLen == 0) return false;
                codeLenCounts[shallowLeafCodeLen]--;
                codeLenCounts[static_cast<size_t>(shallowLeafCodeLen) + 1] += 2;
            }
            actualMaxCodeLen = maxCodeLen;
            //We might end up with less than maxCodeLength after reduction, so adjust actualMaxCodeLength
            while (codeLenCounts[actualMaxCodeLen] == 0 && actualMaxCodeLen > 0) actualMaxCodeLen--;
        }
        for (u16 i = 0; i < presentedByteCount; i++) result[byteFrequencies[i].byte].codeLen = 0;
        sort(byteFrequencies.begin(), byteFrequencies.begin() + presentedByteCount, [](const ByteFrequency& a, const ByteFrequency& b) {
            if (a.frequency == b.frequency) return a.byte < b.byte;
            return a.frequency > b.frequency;
        });
        u8 symbolIndex = 0;
        for (u16 i = 1; i <= actualMaxCodeLen; ++i) {
            u16 count = codeLenCounts[i];
            for (u16 j = 0; j < count; j++) {
                result[byteFrequencies[symbolIndex].byte].codeLen = i;
                symbolIndex++;
            }
        }
        sort(byteFrequencies.begin(), byteFrequencies.begin() + presentedByteCount, [&result](const ByteFrequency& a, const ByteFrequency& b) {
            if (result[a.byte].codeLen == result[b.byte].codeLen) return a.byte < b.byte;
            return result[a.byte].codeLen < result[b.byte].codeLen;
        });
        const u8 firstByte = byteFrequencies[0].byte;
        u16 currentLen = result[firstByte].codeLen;
        u64 currentCode = 0;
        result[firstByte].code = currentCode;
        for (u16 i = 1; i < presentedByteCount; ++i) {
            const u8 byteValue = byteFrequencies[i].byte;
            const u16 thisLen = result[byteValue].codeLen;
            currentCode = (currentCode + 1) << (thisLen - currentLen);
            result[byteValue].code = currentCode;
            currentLen = thisLen;
        }
        return true;
    }

    inline void serialize(const array<HuffmanCode, 256>& codes, vector<u8>& data) noexcept {
        for (u16 i = 0; i < 256; i++) data.push_back(codes[i].codeLen);
    }

    inline void deserialize(const array<u8, 256>& table, vector<IndexedNodeR>& tree) noexcept {
        tree.clear();
        vector<ByteCodeLen> presentedBytes;
        presentedBytes.reserve(256);
        for (u16 i = 0; i < 256; i++) if (table[i] > 0) presentedBytes.emplace_back(static_cast<u8>(i), table[i]);
        //Empty table
        if (presentedBytes.size() == 0) return;
        sort(presentedBytes.begin(), presentedBytes.end(), [](const ByteCodeLen& a, const ByteCodeLen& b) {
            if (a.codeLen == b.codeLen) return a.byte < b.byte;
            return a.codeLen < b.codeLen;
        });
        tree.emplace_back(); //Root
        u64 currentCode = 0;
        u8 currentLen = presentedBytes[0].codeLen;
        for (u16 i = 0; i < presentedBytes.size(); i++) {
            u8 thisLen = presentedBytes[i].codeLen;
            if (i > 0) {
                currentCode = (currentCode + 1u) << (thisLen - currentLen);
                currentLen = thisLen;
            }
            u16 nodeIndex = 0;
            for (i16 bitPos = static_cast<i16>(thisLen) - 1; bitPos >= 0; bitPos--) {
                u64 bit = (currentCode >> bitPos) & 1u;
                u16 nextIndex;
                if (bit) {
                    if (tree[nodeIndex].right == 0xFFFFu) {
                        nextIndex = static_cast<u16>(tree.size());
                        tree[nodeIndex].right = nextIndex;
                        IndexedNodeR newNode;
                        tree.push_back(newNode);
                    }
                    else nextIndex = tree[nodeIndex].right;
                }
                else {
                    if (tree[nodeIndex].left == 0xFFFFu) {
                        nextIndex = static_cast<u16>(tree.size());
                        tree[nodeIndex].left = nextIndex;
                        IndexedNodeR newNode;
                        tree.push_back(newNode);
                    }
                    else nextIndex = tree[nodeIndex].left;
                }
                nodeIndex = nextIndex;
            }
            IndexedNodeR& leaf = tree[nodeIndex];
            leaf.isLeaf = true;
            leaf.byte = presentedBytes[i].byte;
        }
    }

    inline void getCodeMap(const vector<IndexedNodeR>& tree, array<HuffmanCode, 256>& result) noexcept {
        result.fill({});
        if (tree.empty()) return;
        vector<IndexCodeCodeLen> stack;
        stack.emplace_back(0, 0, 0);
        while (!stack.empty()) {
            IndexCodeCodeLen current = stack.back();
            stack.pop_back();
            const IndexedNodeR& currentNode = tree[current.index];
            if (currentNode.isLeaf) {
                result[currentNode.byte].code = current.code;
                result[currentNode.byte].codeLen = current.codeLen == 0 ? 1 : current.codeLen;
            }
            else {
                if (currentNode.left != 0xFFFFu) stack.emplace_back(current.code << 1, currentNode.left, current.codeLen + 1);
                if (currentNode.right != 0xFFFFu) stack.emplace_back((current.code << 1) | 1u, currentNode.right, current.codeLen + 1);
            }
        }
    }
}