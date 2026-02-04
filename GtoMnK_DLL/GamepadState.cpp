#include "pch.h"
#include "GamepadState.h"
#include "INISettings.h"
#include "InputState.h"
#include "Input.h"
#include "ScanThread.h"

// For Controller Button States
const size_t NO_ACTION = static_cast<size_t>(-1);

POINT ThumbstickMouseMove(SHORT stickX, SHORT stickY) {
    // Make sure the deadzone is not 0
    double c = (radial_deadzone < 0.01) ? 0.01 : radial_deadzone;
    const double s = axial_deadzone;
    const double m = max_threshold;
    const double b = curve_slope;
    const double a = curve_exponent;
    const double t = look_accel_multiplier;

    double base_sensitivity = sensitivity;

    double safe_multiplier = (sensitivity_multiplier <= 0.001) ? 1.0 : sensitivity_multiplier;

    static double mouseDeltaAccumulatorX = 0.0;
    static double mouseDeltaAccumulatorY = 0.0;

    double normX = static_cast<double>(stickX) / 32767.0;
    double normY = static_cast<double>(stickY) / 32767.0;

    if (std::abs(normX) < s) normX = 0.0;
    if (std::abs(normY) < s) normY = 0.0;

    double magnitude = std::sqrt(normX * normX + normY * normY);
    if (magnitude <= c) return { 0, 0 };
    if (magnitude > 1.0) magnitude = 1.0;

    double effectiveRange = 1.0 - c;
    if (effectiveRange < 1e-6) effectiveRange = 1.0;

    double x = (magnitude - c) / effectiveRange;

    double finalCurveValue = 0.0;
    double accelBoundary = 1.0 - m;
    accelBoundary = (std::max)(0.0, (std::min)(1.0, accelBoundary));

    if (x <= accelBoundary) {
        finalCurveValue = b * x + (1.0 - b) * std::pow(x, a);
    }
    else {
        double kneeValue = b * accelBoundary + (1.0 - b) * std::pow(accelBoundary, a);

        double rampWidth = 1.0 - accelBoundary;

        if (rampWidth <= 1e-6) {
            finalCurveValue = t;
        }
        else {
            double rampProgress = (x - accelBoundary) / rampWidth;
            finalCurveValue = kneeValue + (t - kneeValue) * rampProgress;
        }
    }

    double finalSpeedX = base_sensitivity * safe_multiplier * (1.0f + horizontal_sensitivity);
    double finalSpeedY = base_sensitivity * safe_multiplier * (1.0f + vertical_sensitivity);

    double dirX = normX / magnitude;
    double dirY = normY / magnitude;

    double finalMouseDeltaX = dirX * finalCurveValue * finalSpeedX;
    double finalMouseDeltaY = dirY * finalCurveValue * finalSpeedY;

    mouseDeltaAccumulatorX += finalMouseDeltaX;
    mouseDeltaAccumulatorY += finalMouseDeltaY;

    LONG integerDeltaX = static_cast<LONG>(mouseDeltaAccumulatorX);
    LONG integerDeltaY = static_cast<LONG>(mouseDeltaAccumulatorY);

    mouseDeltaAccumulatorX -= integerDeltaX;
    mouseDeltaAccumulatorY -= integerDeltaY;

    return { integerDeltaX, -integerDeltaY };
}

// 0-255 lower value is more sensitive
bool IsTriggerPressed(BYTE triggerValue) {
    return triggerValue > g_TriggerThreshold;
}

void ProcessButton(UINT buttonFlag, bool isCurrentlyPressed) {
    ButtonState& bs = buttonStates[buttonFlag];

    // On Press
    if (!bs.isPhysicallyPressed && isCurrentlyPressed) {
        bs.isPhysicallyPressed = true;
        bs.pressTime = GetTickCount64();
        bs.activeActionIndex = NO_ACTION;
        bs.pressActionFired = false;
        bs.heldActionString = "0";
        bool found = ScanThread::ButtonPressed(buttonFlag);
        if (!bs.actions.empty() && bs.actions[0].holdDurationMs == 0) {
            bs.activeActionIndex = 0;
            const Action& tapAction = bs.actions[0];

            if (!tapAction.onRelease) {
                if (!found)
                    Input::SendAction(tapAction.actionString, true);
                bs.heldActionString = tapAction.actionString;
                bs.pressActionFired = true;
            }
        }
        return;
    }

    // While Held
    if (bs.isPhysicallyPressed && isCurrentlyPressed) {
        ULONGLONG holdTime = GetTickCount64() - bs.pressTime;
        size_t newActionIndex = NO_ACTION;
        for (size_t i = 0; i < bs.actions.size(); ++i) {
            if (holdTime >= bs.actions[i].holdDurationMs) {
                newActionIndex = i;
            }
        }
        if (newActionIndex != NO_ACTION && newActionIndex != bs.activeActionIndex) {
            if (!bs.heldActionString.empty() && bs.heldActionString != "0") {
                Input::SendAction(bs.heldActionString, false);
            }
            bs.activeActionIndex = newActionIndex;
            bs.pressActionFired = false;
            bs.heldActionString = "0";
            const Action& newActiveAction = bs.actions[bs.activeActionIndex];
            if (!newActiveAction.onRelease) {
                Input::SendAction(newActiveAction.actionString, true);
                bs.heldActionString = newActiveAction.actionString;
                bs.pressActionFired = true;
            }
        }
        return;
    }

    // On Release
    if (bs.isPhysicallyPressed && !isCurrentlyPressed) {
        bs.isPhysicallyPressed = false;

        size_t finalActionIndex = NO_ACTION;
        ULONGLONG finalHoldTime = GetTickCount64() - bs.pressTime;
        for (size_t i = 0; i < bs.actions.size(); ++i) {
            if (finalHoldTime >= bs.actions[i].holdDurationMs) {
                finalActionIndex = i;
            }
        }

        if (finalActionIndex != NO_ACTION && bs.actions[finalActionIndex].onRelease) {
            const Action& finalAction = bs.actions[finalActionIndex];

            Input::SendAction(finalAction.actionString, true);
            Sleep(20);
            Input::SendAction(finalAction.actionString, false);
        }

        if (!bs.heldActionString.empty() && bs.heldActionString != "0") {
            Input::SendAction(bs.heldActionString, false);
        }

        bs.activeActionIndex = NO_ACTION;
        bs.pressActionFired = false;
        bs.heldActionString = "0";
        return;
    }
}

void ProcessTrigger(UINT triggerID, BYTE triggerValue) {
    bool isPressed = IsTriggerPressed(triggerValue);
    ProcessButton(triggerID, isPressed);
}