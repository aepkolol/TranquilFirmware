// TestPipelinePlannerCLRCPP.cpp : main project file.

#include "conio.h"

using namespace System;

#include "TestCaseMotion.h"
#include "MotionHelper.h"

static const char* ROBOT_CONFIG_STR_XY =
	"{\"robotType\": \"XYBot\", \"xMaxMM\":500, \"yMaxMM\":500, \"pipelineLen\":100, "
    " \"stepEnablePin\":\"A2\", \"stepEnableActiveLevel\":1, \"stepDisableSecs\":1.0,"
    " \"cmdsAtStart\":\"\", "
    " \"axis0\": { \"stepPin\": \"D2\", \"dirnPin\":\"D3\", \"maxSpeed\":100.0, \"maxAcc\":100.0,"
    " \"stepsPerRotation\":3200, \"unitsPerRotation\":60 },"
    " \"axis1\": { \"stepPin\": \"D4\", \"dirnPin\":\"D5\", \"maxSpeed\":100.0, \"maxAcc\":100.0,"
    " \"stepsPerRotation\":3200, \"unitsPerRotation\":60 },"
    " \"commandQueue\": { \"cmdQueueMaxLen\":50 } "
    "}";

static bool ptToActuator(MotionElem& motionElem, AxisFloats& actuatorCoords, AxisParams axisParams[], int numAxes)
{
	bool isValid = true;
	for (int i = 0; i < RobotConsts::MAX_AXES; i++)
	{
		// Axis val from home point
		float axisValFromHome = motionElem._pt2MM.getVal(i) - axisParams[i]._homeOffsetVal;
		// Convert to steps and add offset to home in steps
		actuatorCoords.setVal(i, axisValFromHome * axisParams[i].stepsPerUnit()
			+ axisParams[i]._homeOffsetSteps);

		// Check machine bounds
		bool thisAxisValid = true;
		if (axisParams[i]._minValValid && motionElem._pt2MM.getVal(i) < axisParams[i]._minVal)
			thisAxisValid = false;
		if (axisParams[i]._maxValValid && motionElem._pt2MM.getVal(i) > axisParams[i]._maxVal)
			thisAxisValid = false;
		Log.trace("ptToActuator (%s) %f -> %f (homeOffVal %f, homeOffSteps %ld)", thisAxisValid ? "OK" : "INVALID",
			motionElem._pt2MM.getVal(i), actuatorCoords._pt[i], axisParams[i]._homeOffsetVal, axisParams[i]._homeOffsetSteps);
		isValid &= thisAxisValid;
	}
	return isValid;
}

static void actuatorToPt(AxisFloats& actuatorCoords, AxisFloats& pt, AxisParams axisParams[], int numAxes)
{
	for (int i = 0; i < RobotConsts::MAX_AXES; i++)
	{
		float ptVal = actuatorCoords.getVal(i) - axisParams[i]._homeOffsetSteps;
		ptVal = ptVal / axisParams[i].stepsPerUnit() + axisParams[i]._homeOffsetVal;
		if (axisParams[i]._minValValid && ptVal < axisParams[i]._minVal)
			ptVal = axisParams[i]._minVal;
		if (axisParams[i]._maxValValid && ptVal > axisParams[i]._maxVal)
			ptVal = axisParams[i]._maxVal;
		pt.setVal(i, ptVal);
		Log.trace("actuatorToPt %d %f -> %f (perunit %f)", i, actuatorCoords.getVal(i), ptVal, axisParams[i].stepsPerUnit());
	}
}

static void correctStepOverflow(AxisParams axisParams[], int numAxes)
{
	// Not necessary for a non-continuous rotation bot
}

bool isApproxF(double a, double b, double epsilon = 0.001)
{
	if (a == b)
		return true;
	if ((fabs(a) < 1e-5) && (fabs(b) < 1e-5))
		return true;
	return (fabs(a - b) < epsilon);
}

bool isApproxL(int64_t a, int64_t b, int64_t epsilon = 1)
{
	return (llabs(a - b) < epsilon);
}

static bool getCmdNumber(const char* pCmdStr, int& cmdNum)
{
	// String passed in should start with a G or M
	// And be followed immediately by a number
	if (strlen(pCmdStr) < 1)
		return false;
	const char* pStr = pCmdStr + 1;
	if (!isdigit(*pStr))
		return false;
	long rsltStr = strtol(pStr, NULL, 10);
	cmdNum = (int)rsltStr;
	return true;
}

static bool getGcodeCmdArgs(const char* pArgStr, RobotCommandArgs& cmdArgs)
{
	const char* pStr = pArgStr;
	char* pEndStr = NULL;
	while (*pStr)
	{
		switch (toupper(*pStr))
		{
		case 'X':
			cmdArgs.setAxisValue(0, (float)strtod(++pStr, &pEndStr), true);
			pStr = pEndStr;
			break;
		case 'Y':
			cmdArgs.setAxisValue(1, (float)strtod(++pStr, &pEndStr), true);
			pStr = pEndStr;
			break;
		case 'Z':
			cmdArgs.setAxisValue(2, (float)strtod(++pStr, &pEndStr), true);
			pStr = pEndStr;
			break;
		case 'E':
			cmdArgs.extrudeValid = true;
			cmdArgs.extrudeVal = (float)strtod(++pStr, &pEndStr);
			pStr = pEndStr;
			break;
		case 'F':
			cmdArgs.feedrateValid = true;
			cmdArgs.feedrateVal = (float)strtod(++pStr, &pEndStr);
			pStr = pEndStr;
			break;
		case 'S':
		{
			int endstopIdx = strtol(++pStr, &pEndStr, 10);
			pStr = pEndStr;
			if (endstopIdx == 1)
				cmdArgs.endstopEnum = RobotEndstopArg_Check;
			else if (endstopIdx == 0)
				cmdArgs.endstopEnum = RobotEndstopArg_Ignore;
			break;
		}
		default:
			pStr++;
			break;
		}
	}
	return true;
}

// Interpret GCode G commands
static bool interpG(String& cmdStr, bool takeAction, MotionHelper& motionHelper)
{
	// Command string as a text buffer
	const char* pCmdStr = cmdStr.c_str();

	// Command number
	int cmdNum = 0;
	bool rslt = getCmdNumber(cmdStr, cmdNum);
	if (!rslt)
		return false;

	// Get args string (separated from the GXX or MXX by a space)
	const char* pArgsStr = "";
	const char* pArgsPos = strstr(pCmdStr, " ");
	if (pArgsPos != 0)
		pArgsStr = pArgsPos + 1;
	RobotCommandArgs cmdArgs;
	rslt = getGcodeCmdArgs(pArgsStr, cmdArgs);

	switch (cmdNum)
	{
	case 0: // Move rapid
	case 1: // Move
		if (takeAction)
		{
			cmdArgs.moveRapid = (cmdNum == 0);
			motionHelper.moveTo(cmdArgs);
		}
		return true;
	}
	return false;
}

struct TEST_FIELD
{
	bool checkThis;
	char* name;
	bool isFloat;
	double valFloat;
	int64_t valLong;
};


void testMotionElemVals(int outIdx, int valIdx, int& errorCount, MotionBlock& elem, const char* pRslt)
{
	TEST_FIELD __testFields[] = {
		{ false, "idx", false, 0, 0 },
		{ true, "_entrySpeedMMps", true, elem._entrySpeedMMps, 0},
		{ true, "_exitSpeedMMps", true, elem._exitSpeedMMps, 0 },
		{ true, "_totalMoveTicks", false, 0, elem._totalMoveTicks },
		{ true, "_initialStepRate", true, elem._initialStepRate, 0 },
		{ true, "_accelUntil", false, 0, elem._accelUntil },
		{ true, "_accelPerTick", true, elem._accelPerTick, 0 },
		{ true, "_decelAfter", false, 0, elem._decelAfter },
		{ true, "_decelPerTick", true, elem._decelPerTick, 0 }
	};

	if (valIdx >= sizeof(__testFields) / sizeof(TEST_FIELD))
		return;
	if (!__testFields[valIdx].checkThis)
		return;

	if (__testFields[valIdx].isFloat)
	{
		if (!isApproxF(__testFields[valIdx].valFloat, atof(pRslt), fabs(__testFields[valIdx].valFloat)/1000))
		{
			Log.info("ERROR Out %d field %s mismatch %f != %s", outIdx, __testFields[valIdx].name, __testFields[valIdx].valFloat, pRslt);
			errorCount++;
		}
	}
	else
	{
		if (!isApproxL(__testFields[valIdx].valLong, atol(pRslt), 1))
		{
			Log.info("ERROR Out %d field %s mismatch %ld != %s", outIdx, __testFields[valIdx].name, __testFields[valIdx].valLong, pRslt);
			errorCount++;
		}
	}
}

int main()
{
    Console::WriteLine(L"Test RBot Motion");

	TestCaseHandler testCaseHandler;
	bool readOk = testCaseHandler.readTestFile("TestCaseMotionFile.txt");
	if (!readOk)
	{
		printf("Failed to read test case file");
		exit(1);
	}

	Log.trace("Sizeof MotionPipelineElem %d, AxisBools %d, AxisFloat %d, AxisUint32s %d", 
				sizeof(MotionBlock), sizeof(AxisBools), sizeof(AxisFloats), sizeof(AxisInt32s));

	int totalErrorCount = 0;

	// Go through tests
	for each (TestCaseM^ tc in testCaseHandler.testCases)
	{
		int errorCount = 0;

		MotionHelper _motionHelper;

		_motionHelper.setTransforms(ptToActuator, actuatorToPt, correctStepOverflow);
		_motionHelper.configure(ROBOT_CONFIG_STR_XY);

		for (int i = 0; i < tc->numIns(); i++)
		{
			RobotCommandArgs rcArgs;
			const char* pStr = NULL;
			tc->getIn(i, pStr);
			String cmdStr = pStr;
			interpG(cmdStr, true, _motionHelper);
		}

		if (tc->numIns() != _motionHelper.testGetPipelineCount())
		{
			Log.info("ERROR Pipeline len != Gcode count\n");
			errorCount++;
		}

		if (tc->numOuts() != _motionHelper.testGetPipelineCount())
		{
			Log.info("ERROR Pipeline len != Output check count (%d vs %d)\n", _motionHelper.testGetPipelineCount(), tc->numOuts());
			errorCount++;
		}

		for (int i = 0; i < tc->numOuts(); i++)
		{
			char* pStr = NULL;
			tc->getOut(i, pStr);
			MotionBlock elem;
			_motionHelper.testGetPipelineBlock(i, elem);
			char toks[] = " ";
			char *pRslt;
			pRslt = strtok(pStr, toks);
			int valIdx = 0;
			while (pRslt != NULL)
			{
				testMotionElemVals(i, valIdx, errorCount, elem, pRslt);
				valIdx++;
				pRslt = strtok(NULL, toks);
			}
		}

		_motionHelper.debugShowBlocks();

		if (errorCount != 0)
		{
			Log.info("-------------ERRORS---------------");
		}
		Log.info("Test Case error count %d", errorCount);


		while (true)
		{
			if (_kbhit())
			{
				_getch();
				break;
			}
			_motionHelper.service(true);
			if (_motionHelper.isIdle())
			{
				testCompleted();
			}
		}

		totalErrorCount += errorCount;

	}
	if (totalErrorCount != 0)
	{
		Log.info("-------------TOTAL ERRORS---------------");
	}
	Log.info("Total error count %d", totalErrorCount);
	return 0;
}
