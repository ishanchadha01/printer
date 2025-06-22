#include <jthread>

class WorkerThread
{
public:
    WorkerThread();
    virtual ~WorkerThread();

    virtual void run() = 0;

    void setThread(std::jthread&& thread);

    // delete copy and copy assignment constructors
    WorkerThread& operator=(const WorkerThread& thread) = delete;
    WorkerThread(const WorkerThread& thread) = delete;

    // Match format for controller read/write bit
    enum class RWBit
    {
        WRITE = 0,
        READ = 1
    }

private:
    std::jthread _thread;
};