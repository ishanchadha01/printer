#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "include/constants.hpp"
#include "include/containers/data_packet.hpp"
#include "include/workers/controller.hpp"
#include "include/workers/path_plan.hpp"

class ControllerTest : public testing::Test {
protected:
    Controller controller_;
};

// helper to build a "ReleaseWorker" packet
static DefaultDataPacketT make_release_worker_packet(std::size_t worker_id) {
    DefaultDataPacketT packet;
    packet.reset();

    // index 0: command id
    packet.insert(0, static_cast<std::int64_t>(CmdId::ReleaseWorker));
    // index 1: worker index
    packet.insert(1, static_cast<std::int64_t>(worker_id));

    return packet;
}

// test 1 - request worker, release worker, check pool
TEST_F(ControllerTest, RequestWorker) {
    // no worker objects in pool
    EXPECT_EQ(controller_.get_thread_pool_size(), 0u);

    // create worker
    std::shared_ptr<PathPlanner> worker1 = controller_.request_worker<PathPlanner>();
    ASSERT_TRUE(worker1 != nullptr);
    std::cout << "2" << std::endl;
    auto worker_id = worker1->get_id();
    EXPECT_EQ(controller_.get_thread_pool_size(), 1u);

    // build release packet and send to Controller
    auto release_data = make_release_worker_packet(static_cast<std::size_t>(worker_id));
    worker1->send_data(std::move(release_data), ControllerTaskIdx);

    // release worker
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (controller_.get_thread_pool_size() == 0u) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_EQ(controller_.get_thread_pool_size(), 0u);
}

// test 2 - max out worker pool

// test 3 - max out task queue

// test 4 - test other commands in task dispatcher loop (like mock path commands? or mock PID control?)