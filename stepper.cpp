// // #include <exception>

// #include "stepper.h"

// void Stepper::step(int step_num)
// {
//     // if (this->pin_count != 4)
//     //     throw exception();

//     switch (step_num)
//     {
//         case 0:
//             digitalWrite(1);
//             digitalWrite(0);
//             digitalWrite(0);
//             digitalWrite(1);
//         case 1:
//             digitalWrite(1);
//             digitalWrite(1);
//             digitalWrite(0);
//             digitalWrite(0);
//         case 2:
//             digitalWrite(0);
//             digitalWrite(1);
//             digitalWrite(1);
//             digitalWrite(0);
//         case 3:
//             digitalWrite(0);
//             digitalWrite(0);
//             digitalWrite(1);
//             digitalWrite(1);
//     }
// }

// void Stepper:move()
// {
//     this->step(0);
//     sleep(1000);
//     this->step(1);
//     sleep(1000);
//     this->step(2);
//     sleep(1000);
//     this->step(3);
//     sleep(1000);
// }