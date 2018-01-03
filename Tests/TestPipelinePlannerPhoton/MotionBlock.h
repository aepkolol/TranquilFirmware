// RBotFirmware
// Rob Dobson 2016

#pragma once

#include "AxisValues.h"
#include "math.h"
#ifdef USE_SMOOTHIE_CODE
#include <vector>
#endif
#ifdef ROB_IMPLEMENT_TESTS
extern void testCompleted();
#endif

class MotionBlock
{
public:
	// Step phases of each block: acceleration, plateau, deceleration
#ifdef USE_SMOOTHIE_CODE
	static constexpr int MAX_STEP_PHASES = 3;
#endif
	static constexpr int STEP_PHASE_ACCEL = 0;
	static constexpr int STEP_PHASE_PLATEAU = 1;
	static constexpr int STEP_PHASE_DECEL = 2;

	// Minimum move distance
	static constexpr double MINIMUM_MOVE_DIST_MM = 0.0001;

	// Number of ticks to accumulate for rate actuation
	static constexpr uint32_t K_VALUE = 1000000000l;

	// Tick interval in NS
	static constexpr uint32_t TICK_INTERVAL_NS = 1000;

	// Number of ns in ms
	static constexpr uint32_t NS_IN_A_MS = 1000000;

	// Struct for moving parameters around while computing block data
	struct motionParams
	{
		float _masterAxisMaxAccMMps2;
		float _masterAxisStepDistanceMM;
		uint32_t _minStepIntervalNS;
		uint32_t _maxStepIntervalNS;
	};

public:
	// Max speed set for master axis (maybe reduced by feedrate in a GCode command)
	float _maxParamSpeedMMps;
	// Steps required for each axis
	AxisInt32s _axisStepsToTarget;
	// Distance (pythagorean) to move considering primary axes only
	float _moveDistPrimaryAxesMM;
	// Computed max entry speed for a block based on max junction deviation calculation
	float _maxEntrySpeedMMps;
	// Computed entry speed for this block
	float _entrySpeedMMps;
	// Computed exit speed for this block
	float _exitSpeedMMps;
	// Unit vectors for movement
	AxisFloats unitVectors;

	// Flags
	struct
	{
		bool _nominalLengthFlag : 1;
		bool _recalcFlag : 1;
		bool _isRunning : 1;
		bool _changeInProgress : 1;
	};

#ifdef USE_SMOOTHIE_CODE
	uint32_t _accelUntil;
	uint32_t _decelAfter;
	uint32_t _totalMoveTicks;
	double _accelPerTick;
	double _decelPerTick;
	float _initialStepRate;

	// this is the data needed to determine when each motor needs to be issued a step
	using tickinfo_t = struct {
		int32_t steps_per_tick; // 2.30 fixed point
		int32_t counter; // 2.30 fixed point
		int32_t acceleration_change; // 2.30 fixed point signed
		int32_t deceleration_change; // 2.30 fixed point
		int32_t plateau_rate; // 2.30 fixed point
		uint32_t steps_to_move;
		uint32_t step_count;
		uint32_t next_accel_event;

		//uint32_t _totalSteps;
		//uint32_t _accelSteps;
		//uint32_t _accelReducePerStepNs;
		//uint32_t _decelStartSteps;
		//uint32_t _decelIncreasePerStepNs;
		//uint32_t _nsAccum;
		//uint32_t _nsToNextStep;

		bool _stepDirection;
		//uint32_t _curStepCount;
	};
	std::vector<tickinfo_t> _tickInfo;

	struct axisStepPhase_t
	{
		int _stepsInPhase;
		int32_t _stepIntervalChangeNs;
	};
	struct axisStepInfo_t
	{
		axisStepPhase_t _stepPhases[MAX_STEP_PHASES];
		uint32_t _initialStepIntervalNs;
	};
	axisStepInfo_t _axisStepInfo[RobotConsts::MAX_AXES];
#endif

	struct axisStepData_t
	{
		uint32_t _initialStepRatePerKTicks;
		uint32_t _accStepsPerKTicksPerMS;
		uint32_t _stepsInAccPhase;
		uint32_t _stepsInPlateauPhase;
		uint32_t _stepsInDecelPhase;
	};
	axisStepData_t _axisStepData[RobotConsts::MAX_AXES];

public:
	MotionBlock()
	{
#ifdef USE_SMOOTHIE_CODE
		// Reset size of tick vector
		_tickInfo.resize(RobotConsts::MAX_AXES);
#endif
		clear();
	}

	void clear()
	{
		// Clear values
		_maxParamSpeedMMps = 0;
		_moveDistPrimaryAxesMM = 0;
		_maxEntrySpeedMMps = 0;
		_entrySpeedMMps = 0;
		_exitSpeedMMps = 0;
		_isRunning = false;
		_nominalLengthFlag = false;
		_recalcFlag = false;
		_changeInProgress = false;
#ifdef USE_SMOOTHIE_CODE
		_accelUntil = 0;
		_decelAfter = 0;
		_totalMoveTicks = 0;
		_accelPerTick = 0;
		_decelPerTick = 0;
		_initialStepRate = 0;
#endif
	}

	void getAbsMaxStepsForAnyAxis(uint32_t& axisAbsMaxSteps, int &axisIdxWithMaxSteps)
	{
		axisAbsMaxSteps = 0;
		axisIdxWithMaxSteps = 0;
		for (int axisIdx = 0; axisIdx < RobotConsts::MAX_AXES; axisIdx++)
		{
			if (axisAbsMaxSteps < uint32_t(labs(_axisStepsToTarget.vals[axisIdx])))
			{
				axisAbsMaxSteps = labs(_axisStepsToTarget.vals[axisIdx]);
				axisIdxWithMaxSteps = axisIdx;
			}
		}
	}

	float calcMaxSpeedReverse(float exitSpeed, MotionBlock::motionParams& motionParams)
	{
		// If entry speed is already at the maximum entry speed, no need to recheck. Block is cruising.
		// If not, block in state of acceleration or deceleration. Reset entry speed to maximum and
		// check for maximum allowable speed reductions to ensure maximum possible planned speed.
		if (_entrySpeedMMps != _maxEntrySpeedMMps)
		{
			// If nominal length true, max junction speed is guaranteed to be reached. Only compute
			// for max allowable speed if block is decelerating and nominal length is false.
			if ((!_nominalLengthFlag) && (_maxEntrySpeedMMps > exitSpeed))
			{
				float maxEntrySpeed = maxAllowableSpeed(-motionParams._masterAxisMaxAccMMps2, exitSpeed, this->_moveDistPrimaryAxesMM);
				_entrySpeedMMps = std::min(maxEntrySpeed, _maxEntrySpeedMMps);
			}
			else
			{
				_entrySpeedMMps = _maxEntrySpeedMMps;
			}
		}
		return _entrySpeedMMps;
	}

	void calcMaxSpeedForward(float prevMaxExitSpeed, MotionBlock::motionParams& motionParams)
	{
		// If the previous block is an acceleration block, but it is not long enough to complete the
		// full speed change within the block, we need to adjust the entry speed accordingly. Entry
		// speeds have already been reset, maximized, and reverse planned by reverse planner.
		// If nominal length is true, max junction speed is guaranteed to be reached. No need to recheck.
		if (prevMaxExitSpeed > _maxParamSpeedMMps)
			prevMaxExitSpeed = _maxParamSpeedMMps;
		if (prevMaxExitSpeed > _maxEntrySpeedMMps)
			prevMaxExitSpeed = _maxEntrySpeedMMps;
		if (prevMaxExitSpeed <= _entrySpeedMMps)
		{
			// We're acceleration limited
			_entrySpeedMMps = prevMaxExitSpeed;
			// since we're now acceleration or cruise limited
			// we don't need to recalculate our entry speed anymore
			if (_entrySpeedMMps >= _maxParamSpeedMMps)
				_recalcFlag = false;
		}
		// Now max out the exit speed
		maximizeExitSpeed(motionParams);
	}

	float maxAllowableSpeed(float acceleration, float target_velocity, float distance)
	{
		return sqrtf(target_velocity * target_velocity - 2.0F * acceleration * distance);
	}

	void maximizeExitSpeed(MotionBlock::motionParams& motionParams)
	{
		// If block is being executed then don't change
		if (_isRunning)
			return;

		// If nominalLengthFlag then guaranteed to reach nominal speed regardless of entry
		// speed so just use nominal speed
		if (_nominalLengthFlag)
			_exitSpeedMMps = std::min(_maxParamSpeedMMps, _exitSpeedMMps);

		// Otherwise work out max exit speed based on entry and acceleration
		float maxExitSpeed = maxAllowableSpeed(-motionParams._masterAxisMaxAccMMps2, _entrySpeedMMps, _moveDistPrimaryAxesMM);
		_exitSpeedMMps = std::min(maxExitSpeed, _exitSpeedMMps);
	}

	void forceInBounds(float& val, float lowBound, float highBound)
	{
		if (val < lowBound)
			val = lowBound;
		if (val > highBound)
			val = highBound;
	}

	// Calculate trapezoid parameters so that the entry- and exit-speed is compensated by the provided factors.
	// The factors represent a factor of braking and must be in the range 0.0-1.0.
	//                                +--------+ <- nominal_rate
	//                               /          \
	// nominal_rate*entry_factor -> +            \
	//                              |             + <- nominal_rate*exit_factor
	//                              +-------------+
	//                                  time -->
	void calculateTrapezoid(MotionBlock::motionParams& motionParams)
	{
		// If block is currently being processed don't change it
		if (_isRunning)
			return;

		// At this point we are ready to calculate the phases of motion for this block: acceleration, plateau, deceleration

		// First calculate the step rate (steps per sec) for entry and exit
		float initialStepRatePerSec = _entrySpeedMMps / motionParams._masterAxisStepDistanceMM;
		float finalStepRatePerSec = _exitSpeedMMps / motionParams._masterAxisStepDistanceMM;

		// Calculate the distance accelerating and ensure within bounds
		// Using the facts for the block ... (assuming max accleration followed by max deceleration):
		//		Vmax * Vmax = Ventry * Ventry + 2 * Amax * Saccelerating
		//		Vexit * Vexit = Vmax * Vmax - 2 * Amax * Sdecelerating
		//      Stotal = Saccelerating + Sdecelerating
		// And solving for Saccelerating (distance accelerating)
		float distAccelerating = (powf(_exitSpeedMMps, 2) - powf(_entrySpeedMMps, 2)) / 4 / motionParams._masterAxisMaxAccMMps2 + _moveDistPrimaryAxesMM / 2;
		forceInBounds(distAccelerating, 0, _moveDistPrimaryAxesMM);
		float distDecelerating = _moveDistPrimaryAxesMM - distAccelerating;
		float distPlateau = 0;

		// Check if max speed is reached
		float maxSpeedReachedMMps = 0;
		float distToMaxSpeed = (powf(_maxParamSpeedMMps, 2) - powf(_entrySpeedMMps, 2)) / 2 / motionParams._masterAxisMaxAccMMps2;
		if (distToMaxSpeed < distAccelerating)
		{
			// We hit max speed
			maxSpeedReachedMMps = _maxParamSpeedMMps;
			// Max speed reached so we need to plateau
			distAccelerating = distToMaxSpeed;
			// Also need to recalculate distance decelerating
			distDecelerating = (powf(_maxParamSpeedMMps, 2) - powf(_exitSpeedMMps, 2)) / 2 / motionParams._masterAxisMaxAccMMps2;
			// And the plateau
			distPlateau = _moveDistPrimaryAxesMM - distAccelerating - distDecelerating;
		}
		else
		{
			// Calculate max speed we do reach
			maxSpeedReachedMMps = sqrtf(pow(_entrySpeedMMps, 2) + 2 * motionParams._masterAxisMaxAccMMps2 * distAccelerating);
		}

		// Proportions of distance in each phase (acceleration, plateau, deceleration)
		float distPropAccelerating = distAccelerating / _moveDistPrimaryAxesMM;
		float distPropPlateau = distPlateau / _moveDistPrimaryAxesMM;
		float distPropDecelerating = distDecelerating / _moveDistPrimaryAxesMM;

		// Max step rate reached
		float maxStepRateReached = maxSpeedReachedMMps / motionParams._masterAxisStepDistanceMM;

		// Find the axisIdx with max steps and the max number of steps
		uint32_t absMaxStepsForAnyAxis = 0;
		int axisIdxWithMaxSteps = 0;
		getAbsMaxStepsForAnyAxis(absMaxStepsForAnyAxis, axisIdxWithMaxSteps);
		float oneOverAbsMaxStepsAnyAxis = 1.0f / absMaxStepsForAnyAxis;

		// KTicks
		float ticksPerSec = 1e9 / TICK_INTERVAL_NS;

		// Master axis acceleration in steps per KTicks per millisecond
		float masterAxisMaxAccStepsPerSec2 = motionParams._masterAxisMaxAccMMps2 / motionParams._masterAxisStepDistanceMM;
		float masterAxisMaxAccStepsPerKTicksPerSec = (K_VALUE * masterAxisMaxAccStepsPerSec2) / ticksPerSec;
		float masterAxisMaxAccStepsPerKTicksPerMilliSec = masterAxisMaxAccStepsPerKTicksPerSec / 1000;

		// Now setup the values for the ticker to generate steps
		for (uint8_t axisIdx = 0; axisIdx < RobotConsts::MAX_AXES; axisIdx++)
		{

			// Calculate scaling factor for this axis
			uint32_t absStepsThisAxis = labs(_axisStepsToTarget.vals[axisIdx]);
			float axisFactor = absStepsThisAxis * oneOverAbsMaxStepsAnyAxis;

			// Initial step rate for this axis
			float axisInitialStepRatePerSec = initialStepRatePerSec * axisFactor;

			// Initial step rate for this axis per KTicks
			float axisInitialStepRatePerKTicksFloat = (K_VALUE * axisInitialStepRatePerSec) / ticksPerSec;

			// Axis max acceleration in units suitable for actuation
			float axisMaxAccStepsPerKTicksPerMilliSec = masterAxisMaxAccStepsPerKTicksPerMilliSec * axisFactor;

			// Step values
			uint32_t stepsAccel = uint32_t(ceilf(absStepsThisAxis * distPropAccelerating));
			uint32_t stepsPlateau = uint32_t(absStepsThisAxis * distPropPlateau);
			uint32_t stepsDecel = uint32_t(absStepsThisAxis - stepsAccel - stepsPlateau);

			// Setup actuation data
			_axisStepData[axisIdx]._initialStepRatePerKTicks = uint32_t(axisInitialStepRatePerKTicksFloat + axisMaxAccStepsPerKTicksPerMilliSec);
			_axisStepData[axisIdx]._accStepsPerKTicksPerMS = uint32_t(axisMaxAccStepsPerKTicksPerMilliSec);
			_axisStepData[axisIdx]._stepsInAccPhase = stepsAccel;
			_axisStepData[axisIdx]._stepsInPlateauPhase = stepsPlateau;
			_axisStepData[axisIdx]._stepsInDecelPhase = stepsDecel;


#ifdef USE_SMOOTHIE_CODE

			// Initial step time this axis
			float accInStepsPerS2 = motionParams._masterAxisMaxAccMMps2 / motionParams._masterAxisStepDistanceMM;
			float axisInitialStepTime = (sqrtf(powf(axisInitialStepRatePerSec, 2) + 2 * accInStepsPerS2) - axisInitialStepRatePerSec) / accInStepsPerS2;

			float axisMaxStepRate = maxStepRateReached * axisFactor;
			float axisFinalStepRate = finalStepRatePerSec * axisFactor;

			// Step targets for each phase
			_axisStepInfo->_stepPhases[STEP_PHASE_ACCEL]._stepsInPhase = stepsAccel;
			_axisStepInfo->_stepPhases[STEP_PHASE_PLATEAU]._stepsInPhase = stepsPlateau;
			_axisStepInfo->_stepPhases[STEP_PHASE_DECEL]._stepsInPhase = stepsDecel;


			/// ROB


			/// THIS NEEDS FIXING
			// Currently  motionParams._maxStepIntervalNS is 1.8 seconds I think!!!!
			/// Need to work out how to take the first step when rate is 0 steps per sec
			/// Need to look ahead to second step maybe??? should always be accelerating???




			// Calculate initial step interval (NS)
			_axisStepInfo->_initialStepIntervalNs = motionParams._maxStepIntervalNS;
			if (axisInitialStepRatePerSec > 1e9f / motionParams._maxStepIntervalNS)
			{
				_axisStepInfo->_initialStepIntervalNs = uint32_t(1e9f / axisInitialStepRatePerSec);
			}

			// Acceleration each phase
			_axisStepInfo->_stepPhases[STEP_PHASE_ACCEL]._stepIntervalChangeNs = 0;
			_axisStepInfo->_stepPhases[STEP_PHASE_PLATEAU]._stepIntervalChangeNs = 0;
			_axisStepInfo->_stepPhases[STEP_PHASE_DECEL]._stepIntervalChangeNs = 0;
			if (stepsAccel > 0)
			{
				double stepIntervalNSChangePerStepAccel = ((1e9 / axisInitialStepRatePerSec) - (1e9 / axisMaxStepRate)) / stepsAccel;
				_axisStepInfo->_stepPhases[STEP_PHASE_ACCEL]._stepIntervalChangeNs = (uint32_t)stepIntervalNSChangePerStepAccel;
			}
			if (stepsDecel > 0)
			{
				double stepIntervalNSChangePerStepDecel = ((1e9 / axisMaxStepRate) - (1e9 / axisFinalStepRate)) / stepsDecel;
				_axisStepInfo->_stepPhases[STEP_PHASE_DECEL]._stepIntervalChangeNs = (uint32_t)stepIntervalNSChangePerStepDecel;
			}
#endif
		}

#ifdef USE_SMOOTHIE_CODE
		//// Time for this block of movement
		//float blockTimeS = (sqrtf(_entrySpeedMMps * _entrySpeedMMps + 2 * motionParams._masterAxisMaxAccMMps2 * _moveDistPrimaryAxesMM) - _entrySpeedMMps) /
		//							motionParams._masterAxisMaxAccMMps2;

		////

		// Now calculate the step rate at max parametric speed (might be altered by feedrate demanded)
		float blockTime = _moveDistPrimaryAxesMM / _maxParamSpeedMMps;
		float stepRatePerSecAtMaxParamSpeed = absMaxStepsForAnyAxis / blockTime;

		Log.trace("maxStepsOfAnyAxis %lu, _maxParamSpeedMMps %0.3f mm/s, stepRatePerSecAtMaxParamSpeed %0.3f steps/s",
			absMaxStepsForAnyAxis, _maxParamSpeedMMps, stepRatePerSecAtMaxParamSpeed);

		// Initial rate
		float initialStepRate = stepRatePerSecAtMaxParamSpeed * (_entrySpeedMMps / _maxParamSpeedMMps);
		float finalStepRate = stepRatePerSecAtMaxParamSpeed * (_exitSpeedMMps / _maxParamSpeedMMps);

		Log.trace("trapezoid initRate %0.3f steps/s, finalRate %0.3f steps/s", initialStepRate, finalStepRate);

		// How many steps ( can be fractions of steps, we need very precise values ) to accelerate and decelerate
		// This is a simplification to get rid of rate_delta and get the steps/s� accel directly from the mm/s� accel
		float accInStepUnits = (motionParams._masterAxisMaxAccMMps2 * absMaxStepsForAnyAxis) / _moveDistPrimaryAxesMM;
		float maxPossStepRate = sqrtf((absMaxStepsForAnyAxis * accInStepUnits) + ((powf(initialStepRate, 2) + powf(finalStepRate, 2)) / 2.0F));

		Log.trace("trapezoid accInStepUnits %0.3f steps/s2, maxPossStepRate %0.3f steps/s", accInStepUnits, maxPossStepRate);

		// Now this is the maximum rate we'll achieve this move, either because
		// it's the higher we can achieve, or because it's the higher we are
		// allowed to achieve
		float maxStepRatePerSec = std::min(maxPossStepRate, stepRatePerSecAtMaxParamSpeed);

		// Now figure out how long it takes to accelerate in seconds
		float timeToAccelerateSecs = (maxStepRatePerSec - initialStepRate) / accInStepUnits;

		// Now figure out how long it takes to decelerate
		float timeToDecelerateSecs = (finalStepRate - maxStepRatePerSec) / -accInStepUnits;

		Log.trace("trapezoid maxStepRatePerSec %0.3f steps/s, timeToAccelerate %0.3f s, timeToDecelerate %0.3f s", maxStepRatePerSec, timeToAccelerateSecs, timeToDecelerateSecs);

		// Now we know how long it takes to accelerate and decelerate, but we must
		// also know how long the entire move takes so we can figure out how long
		// is the plateau if there is one
		float plateauTimeSecs = 0;

		// Only if there is actually a plateau ( we are limited by nominal_rate )
		if (maxPossStepRate > stepRatePerSecAtMaxParamSpeed)
		{
			// Figure out the acceleration and deceleration distances ( in steps )
			float accelDistance = ((initialStepRate + maxStepRatePerSec) / 2.0F) * timeToAccelerateSecs;
			float decelDistance = ((maxStepRatePerSec + finalStepRate) / 2.0F) * timeToDecelerateSecs;

			// Figure out the plateau steps
			float plateauDist = absMaxStepsForAnyAxis - accelDistance - decelDistance;

			// Figure out the plateau time in seconds
			plateauTimeSecs = plateauDist / maxStepRatePerSec;
		}

		// Figure out how long the move takes total ( in seconds )
		float totalMoveTimeSecs = timeToAccelerateSecs + timeToDecelerateSecs + plateauTimeSecs;
		//puts "total move time: #{total_move_time}s time to accelerate: #{time_to_accelerate}, time to decelerate: #{time_to_decelerate}"

		Log.trace("plateauTime %0.3f s, totalMoveTime %0.3f s", plateauTimeSecs, totalMoveTimeSecs);

		// We now have the full timing for acceleration, plateau and deceleration,
		// yay \o/ Now this is very important these are in seconds, and we need to
		// round them into ticks. This means instead of accelerating in 100.23
		// ticks we'll accelerate in 100 ticks. Which means to reach the exact
		// speed we want to reach, we must figure out a new/slightly different
		// acceleration/deceleration to be sure we accelerate and decelerate at
		// the exact rate we want

		// First off round total time, acceleration time and deceleration time in ticks
		uint32_t accelTicks = uint32_t(floorf(timeToAccelerateSecs * STEP_TICKER_FREQUENCY));
		uint32_t decelTicks = uint32_t(floorf(timeToDecelerateSecs * STEP_TICKER_FREQUENCY));
		uint32_t totalTicks = uint32_t(floorf(totalMoveTimeSecs    * STEP_TICKER_FREQUENCY));

		// Now deduce the plateau time for those new values expressed in tick
		//uint32_t plateau_ticks = total_move_ticks - acceleration_ticks - deceleration_ticks;

		// Now we figure out the acceleration value to reach EXACTLY maximum_rate(steps/s) in EXACTLY acceleration_ticks(ticks) amount of time in seconds
		// This can be moved into the operation below, separated for clarity, note we need to do this instead of using time_to_accelerate(seconds)
		// directly because time_to_accelerate(seconds) and acceleration_ticks(seconds) do not have the same value anymore due to the rounding
		float acceleration_time = 1.0f * accelTicks / STEP_TICKER_FREQUENCY;
		float deceleration_time = 1.0f * decelTicks / STEP_TICKER_FREQUENCY;

		float acceleration_in_steps = (timeToAccelerateSecs > 0.000001F) ? (maxStepRatePerSec - initialStepRate) / timeToAccelerateSecs : 0;
		float deceleration_in_steps = (timeToDecelerateSecs > 0.000001F) ? (maxStepRatePerSec - finalStepRate) / timeToDecelerateSecs: 0;

		// we have a potential race condition here as we could get interrupted anywhere in the middle of this call, we need to lock
		// the updates to the blocks to get around it
		_changeInProgress = true;
#endif

#ifdef USE_SMOOTHIE_CODE

		// Now figure out the two acceleration ramp change events in ticks
		_accelUntil = accelTicks;
		_decelAfter = totalTicks - decelTicks;

		// Now figure out the acceleration PER TICK, this should ideally be held as a float, even a double if possible as it's very critical to the block timing
		// steps/tick^2

		_accelPerTick = acceleration_in_steps / STEP_TICKER_FREQUENCY_2;
		_decelPerTick = deceleration_in_steps / STEP_TICKER_FREQUENCY_2;

		// We now have everything we need for this block to call a Steppermotor->move method !!!!
		// Theorically, if accel is done per tick, the speed curve should be perfect.
		_totalMoveTicks = totalTicks;

		//puts "accelerate_until: #{this->accelerate_until}, decelerate_after: #{this->decelerate_after}, acceleration_per_tick: #{this->acceleration_per_tick}, total_move_ticks: #{this->total_move_ticks}"

		_initialStepRate = initialStepRate;

		// Make calculations for the MotionActuator (which gets the block and effects it)
		float inv = 1.0F / absMaxStepsForAnyAxis;
		for (uint8_t axisIdx = 0; axisIdx < RobotConsts::MAX_AXES; axisIdx++) {

			//uint32_t axisTotalSteps = labs(_axisStepsToTarget.getVal(axisIdx));
			//float axisStepRatio = inv * axisTotalSteps;

			//_tickInfo[axisIdx]._nsAccum = 0;
			//_tickInfo[axisIdx]._nsToNextStep = 1000000000;
			//_tickInfo[axisIdx]._curStepCount = 0;
			_tickInfo[axisIdx]._stepDirection = (_axisStepsToTarget.getVal(axisIdx) >= 0);
			//_tickInfo[axisIdx]._totalSteps = axisTotalSteps;
			//_tickInfo[axisIdx]._accelSteps = 0;
			//_tickInfo[axisIdx]._decelStartSteps = 0;
			//if (axisTotalSteps == 0)
			//	continue;

			//// Calculate the steps for this axis scaled by ratio vs axis with max steps
			//_tickInfo[axisIdx]._nsToNextStep = uint32_t(1e9f / (_initialStepRate * axisStepRatio));
			//float stepsAccelerating = _initialStepRate * axisStepRatio * timeToAccelerateSecs + 0.5f * axisStepRatio * acceleration_in_steps * powf(timeToAccelerateSecs, 2);
			//_tickInfo[axisIdx]._accelSteps = uint32_t(stepsAccelerating);



			//_tickInfo[axisIdx]._accelSteps = timeToAccelerateSecs

			//_tickInfo[axisIdx].next_accel_event = this->_totalMoveTicks + 1;


			//uint32_t _nsAccum;
			//uint32_t _nsToNextStep;
			//bool _stepDirection;
			//uint32_t _curStepCount;
			//uint32_t _accelSteps;
			//uint32_t _accelReducePerStepNs;
			//uint32_t _decelStartSteps;
			//uint32_t _totalSteps;
			//uint32_t _decelIncreasePerStepNs;


			uint32_t absSteps = labs(_axisStepsToTarget.getVal(axisIdx));
			_tickInfo[axisIdx].steps_to_move = absSteps;
			if (absSteps == 0)
				continue;

			float aratio = inv * absSteps;
			_tickInfo[axisIdx].steps_per_tick = STEPTICKER_TOFP((_initialStepRate * aratio) / STEP_TICKER_FREQUENCY); // steps/sec / tick frequency to get steps per tick in 2.30 fixed point
			_tickInfo[axisIdx].counter = 0; // 2.30 fixed point
			_tickInfo[axisIdx].step_count = 0;
			_tickInfo[axisIdx].next_accel_event = this->_totalMoveTicks + 1;

			double acceleration_change = 0;
			if (_accelUntil != 0) { // If the next accel event is the end of accel
				_tickInfo[axisIdx].next_accel_event = _accelUntil;
				acceleration_change = _accelPerTick;
			}
			else if (_decelAfter == 0 /*&& this->accelerate_until == 0*/) {
				// we start off decelerating
				acceleration_change = -_decelPerTick;
			}
			else if (_decelAfter != _totalMoveTicks /*&& this->accelerate_until == 0*/) {
				// If the next event is the start of decel ( don't set this if the next accel event is accel end )
				_tickInfo[axisIdx].next_accel_event = _decelAfter;
			}

			// convert to fixed point after scaling
			_tickInfo[axisIdx].acceleration_change = STEPTICKER_TOFP(acceleration_change * aratio);
			_tickInfo[axisIdx].deceleration_change = -STEPTICKER_TOFP(_decelPerTick * aratio);
			_tickInfo[axisIdx].plateau_rate = STEPTICKER_TOFP((maxStepRatePerSec * aratio) / STEP_TICKER_FREQUENCY);

		}
#endif

		// No more changes
		_changeInProgress = false;
	}

	void debugShowBlkHead()
	{
		Log.trace("#idx\tEntMMps\tExtMMps\tXSteps\tYSteps\tXStPKtk\tXAcPKms\tXAccSt\tXPlatSt\tXDecSt\tYStPKtk\tYAcPKms\tYAccSt\tYPlatSt\tYDecSt");
	}

	void debugShowBlock(int elemIdx)
	{
#ifdef USE_SMOOTHIE_CODE
		Log.trace("%d\t%0.3f\t%0.3f\t%lu\t%0.1f\t%lu\t%0.1f\t%lu\t%0.1f\t%0.3f\t%0.3f\t%0.3f\t%0.3f\t%0.3f\t%0.3f\t%0.3f\t%0.3f", elemIdx,
				_entrySpeedMMps, _exitSpeedMMps, _totalMoveTicks, _initialStepRate,
				_accelUntil, _accelPerTick, _decelAfter, _decelPerTick,
				STEPTICKER_FROMFP(_tickInfo[0].steps_per_tick)*1e3, STEPTICKER_FROMFP(_tickInfo[0].acceleration_change)*1e10,
				STEPTICKER_FROMFP(_tickInfo[0].deceleration_change)*1e10, STEPTICKER_FROMFP(_tickInfo[0].plateau_rate)*1e3,
				STEPTICKER_FROMFP(_tickInfo[1].steps_per_tick)*1e3, STEPTICKER_FROMFP(_tickInfo[1].acceleration_change)*1e10,
				STEPTICKER_FROMFP(_tickInfo[1].deceleration_change)*1e10, STEPTICKER_FROMFP(_tickInfo[1].plateau_rate)*1e3
			);
#endif
		Log.trace("%d\t%0.3f\t%0.3f\t%ld\t%ld\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu   \t%lu   \t%lu   \t%lu", elemIdx,
			_entrySpeedMMps, _exitSpeedMMps,
			_axisStepsToTarget.X(), _axisStepsToTarget.Y(),
			_axisStepData[0]._initialStepRatePerKTicks, _axisStepData[0]._accStepsPerKTicksPerMS,
			_axisStepData[0]._stepsInAccPhase, _axisStepData[0]._stepsInPlateauPhase, _axisStepData[0]._stepsInDecelPhase,
			_axisStepData[1]._initialStepRatePerKTicks, _axisStepData[1]._accStepsPerKTicksPerMS,
			_axisStepData[1]._stepsInAccPhase, _axisStepData[1]._stepsInPlateauPhase, _axisStepData[1]._stepsInDecelPhase);
	}

};