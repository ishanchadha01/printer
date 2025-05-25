
// include macros - ramps, gcodes


// look at marlin stepper and planner

// A+B moves diagonal, -A-B moves diagonal
// A-B or B-A moves one  direction

// is given initial gcode
// modifies gcode based on sensed data


#define SET_TO_ZERO(a) memset(a, 0, sizeof(a))

uint8_t CMD_QUEUE_SIZE = 128;
uint8_t CMD_QUEUE[CMD_QUEUE_SIZE];


class StateMachine
{
public:
    StateMachine();
    void create_ui();
    void run();

private:
    state_t state;
    status_t status;
    PathPlan plan;
}