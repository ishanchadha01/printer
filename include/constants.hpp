#include <cstdint>


constexpr size_t THREAD_POOL_CAPACITY = 16;

enum class CmdId : uint64_t
{
    ReleaseWorker = 1
};