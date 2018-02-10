#pragma once

namespace RobotConsts
{
static constexpr int MAX_AXES = 3;

// MOTOR_TYPE_DRIVER has an A4988 or similar stepper driver chip that just requires step and direction
typedef enum
{
  MOTOR_TYPE_NONE,
  MOTOR_TYPE_DRIVER,
  MOTOR_TYPE_SERVO
} MOTOR_TYPE;

static constexpr int NUMBERED_COMMAND_NONE = 0;

struct RawMotionAxis_t
{
  RobotConsts::MOTOR_TYPE _motorType;
  int                     _pinDirection;
  bool                    _pinDirectionReversed;
  int                     _pinStep;
  int                     _pinStepCurLevel;
  int                     _pinEndStopMin;
  bool                    _pinEndStopMinActiveLevel;
  int                     _pinEndStopMax;
  bool                    _pinEndStopMaxActiveLevel;
};
class RawMotionHwInfo_t
{
public:
  RawMotionAxis_t _axis[RobotConsts::MAX_AXES];

  RawMotionHwInfo_t operator=(RawMotionHwInfo_t& other)
  {
    for (int i = 0; i < RobotConsts::MAX_AXES; i++)
    {
      _axis[i] = other._axis[i];
    }
  }
};
};
