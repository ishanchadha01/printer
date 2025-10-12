#pragma once

#include <concepts>
#include <variant>
#include <array>
#include <bitset>
#include <cstdint>
#include <atomic>
#include <type_traits>
#include <cstring>

#include "include/constants.hpp"

// Num Task Bits
template<size_t NumTaskBits>
using TaskEncodedType = std::bitset<NumTaskBits>;

// Use concept to tie together default task
template<size_t NumTaskBits, size_t BlockSize>
concept IsDefaultTask = (NumTaskBits == DEFAULT_NUM_TASK_BITS) && (BlockSize == DEFAULT_BLOCK_SIZE);

using DefaultTaskDataType = std::variant< 
                        std::array<char, 8>,
                        long int,
                        double>;

enum class DefaultTaskId : uint8_t {
    CHAR_ARR = 0,
    INT = 1,
    DOUBLE = 2
};

template<typename T, typename Variant>
struct is_in_variant : std::false_type {};

template<typename T, typename... Ts>
struct is_in_variant<T, std::variant<Ts...>> : std::disjunction<std::is_same<T, Ts...>> {};

template<typename T>
concept IsDefaultTaskDataType = is_in_variant<T, DefaultTaskDataType>::value;

template<size_t NumTaskBits, size_t BlockSize>
    requires IsDefaultTask<NumTaskBits, BlockSize>
struct DefaultDataPacket
{

    std::array< std::bitset<NumTaskBits>, BlockSize> data_array; // bitfield and cast to/from value specified
    std::array<uint8_t, BlockSize> data_array_types; // which one of variant fields is being used
    size_t data_array_size = 0;

    constexpr void reset() {
        data_array.fill(static_cast<std::bitset<NumTaskBits>>(0));
        data_array_size = 0;
    }

    template<IsDefaultTaskDataType TaskT>
    void insert(const int& i, const TaskT& data)
    {
        // convert to bitset manually
        using StrippedT = std::decay<TaskT>::type;
        uint64_t raw = 0;
        if constexpr (std::is_same<StrippedT, double>::value) {
            data_array_types[i] = static_cast<uint8_t>(DefaultTaskId::DOUBLE);
            std::memcpy(&raw, &data, sizeof(uint64_t));
        } else if constexpr (std::is_same<StrippedT, long int>::value) {
            data_array_types[i] = static_cast<uint8_t>(DefaultTaskId::INT);
            std::memcpy(&raw, &data, sizeof(uint64_t));
        } else if constexpr (std::is_same_v<StrippedT, std::array<char, 8>>) {
            data_array_types[i] = static_cast<uint8_t>(DefaultTaskId::CHAR_ARR);
            std::memcpy(&raw, data.data(), sizeof(uint64_t)); // std doesnt guarantee &data == &data[0] ??
        } else {
            static_assert(std::is_same<TaskT, TaskT>::value, "Unsupported type for Default Task!");
        }
        data_array[i] = std::bitset<NumTaskBits>(raw);
        data_array_size++;
    }
};