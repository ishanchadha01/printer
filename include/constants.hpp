#pragma once

#include <cstdint>

// For controller
constexpr size_t THREAD_POOL_CAPACITY = 16;

// For data packet
constexpr size_t DEFAULT_NUM_TASK_BITS = 64;
constexpr size_t DEFAULT_BLOCK_SIZE = 2048;

// For queued tasks
using TaskId = std::int8_t;
constexpr TaskId ControllerTaskIdx = -1;

enum class CmdId : uint64_t
{
    ReleaseWorker = 1
};

enum class States {
    EXECUTE_ERROR = -3,
    PLAN_ERROR = -2,
    INIT_ERROR = -1,
    INIT = 0,
    PLAN = 1,
    EXECUTE = 2,
    SHUTDOWN = 3,
};