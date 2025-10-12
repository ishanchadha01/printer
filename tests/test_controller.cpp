#include "gtest/gtest.h"

#include "workers/controller.hpp"
#include "workers/path_plan.hpp"

class ControllerTest : testing::Test {
protected:
    Controller controller_;
};

// test 1 - request worker, release worker, check pool
TEST_F(ControllerTest, RequestWorker) {
    // Only state machine thread requested
    EXPECT_EQ(controller_.get_thread_pool_size(), 1);

    // Create another thread
    std::shared_ptr<PathPlanner> worker1 = controller_.request_worker<PathPlanner>();
    auto worker_id = worker1->get_id();
    EXPECT_EQ(controller_.get_thread_pool_size(), 2);

    // Release worker
    DefaultDataPacketT release_data;
    release_data.insert(static_cast<int>(CmdId::ReleaseWorker));
    release_data.insert(static_cast<int>(worker_id));
    worker1->send_data(ControllerTaskIdx);
    EXPECT_EQ(controller_.get_thread_pool_size(), 1);
}

// test 2 - max out worker pool

// test 3 - max out task queue

// test 4 - test other commands in task dispatcher loop (like mock path commands? or mock PID control?)