// State machine is a bill for which threads should execute when
// Depending on which plugins (WorkerThreads) are running, we should use different state machines

class StateMachine
{
public:
    StateMachine() {}
    virtual ~StateMachine((WorkerThread&)(std::function<void>) request_worker_callback) {}

    bool request_worker() {
        WorkerThreadThread& worker = request_worker_callback();
    }

};

class BasicStateMachine : public StateMachine
{
public:
    BasicStateMachine() : StateMachine() {

    }



}