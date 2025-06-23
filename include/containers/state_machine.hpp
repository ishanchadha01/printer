// State machine is a bill for which threads should execute when
// Depending on which plugins (WorkerThreads) are running, we should use different state machines

class StateMachine
{
public:
    StateMachine(std::function<std::shared_ptr<WorkerThread>()> request_worker_callback)
        : _request_worker_callback(request_worker_callback) {}
    virtual ~StateMachine() = default;

    auto request_worker() {
        return _request_worker_callback();
    }

private:
    std::function<std::shared_ptr<WorkerThread>()> _request_worker_callback;

};

class BasicStateMachine : public StateMachine
{
public:
    BasicStateMachine(std::function<std::shared_ptr<WorkerThread>()> cb) : StateMachine(cb) {}
};