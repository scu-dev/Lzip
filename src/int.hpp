#pragma once
#include <array>
#include <bit>
#include <type_traits>
#include <vector>

namespace Lzip::Util {
    typedef uint8_t u8;
    using std::array, std::bit_cast, std::vector, std::endian, std::is_integral_v;

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