#pragma once

#include <cstdint>

// For controller
constexpr size_t THREAD_POOL_CAPACITY = 16;

// For data packet
constexpr size_t DEFAULT_NUM_TASK_BITS = 64;
constexpr size_t DEFAULT_BLOCK_SIZE = 2048;

enum class CmdId : uint64_t
{
    ReleaseWorker = 1
};