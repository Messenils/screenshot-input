#pragma once
#include "pch.h"
#include <string>
#include <map>
#include <vector>
#include "MainThread.h"

using namespace GtoMnK;

// Gamepad Input Method
enum class GamepadMethod {
    SDL2, // 0
    XInput// 1
};

// For Controller Input
enum CustomInputID {
    CUSTOM_ID_A = 0,
    CUSTOM_ID_B,
    CUSTOM_ID_X,
    CUSTOM_ID_Y,
    CUSTOM_ID_LB,
    CUSTOM_ID_RB,
    CUSTOM_ID_LSB,
    CUSTOM_ID_RSB,
    CUSTOM_ID_START,
    CUSTOM_ID_BACK,
    CUSTOM_ID_DPAD_UP,
    CUSTOM_ID_DPAD_DOWN,
    CUSTOM_ID_DPAD_LEFT,
    CUSTOM_ID_DPAD_RIGHT,

    CUSTOM_ID_LT = 0x10000,
    CUSTOM_ID_RT,

    CUSTOM_ID_LSU,
    CUSTOM_ID_LSD,
    CUSTOM_ID_LSL,
    CUSTOM_ID_LSR,
    CUSTOM_ID_RSU,
    CUSTOM_ID_RSD,
    CUSTOM_ID_RSL,
    CUSTOM_ID_RSR
};

// Find Window
extern char iniWindowName[256];
extern char iniClassName[256];

// API
extern InputMethod g_InputMethod;
extern GamepadMethod g_GamepadMethod;

// Hooks
extern int getCursorPosHook, setCursorPosHook, clipCursorHook, getKeyStateHook, getAsyncKeyStateHook, getKeyboardStateHook, setCursorHook, setRectHook;

// MessageFilter
extern bool g_filterRawInput;
extern bool g_filterMouseMove;
extern bool g_filterMouseActivate;
extern bool g_filterWindowActivate;
extern bool g_filterWindowActivateApp;
extern bool g_filterMouseWheel;
extern bool g_filterMouseButton;
extern bool g_filterKeyboardButton;

// General Settings
extern int startUpDelay;
extern bool recheckHWND;
extern bool disableOverlayOptions;
extern int controllerID;
extern bool g_EnableOpenXinput;
extern int mode;
extern int drawfakecursor;
extern int drawProtoFakeCursor;
extern int ShowProtoCursorWhenImageUpdated;
extern int createdWindowIsOwned;
extern int responsetime;

// StickToMouse
extern int righthanded;
extern float radial_deadzone, axial_deadzone, sensitivity, max_threshold, curve_slope, curve_exponent, sensitivity_multiplier, horizontal_sensitivity, vertical_sensitivity, look_accel_multiplier;

// KeyMapping
extern bool g_EnableMouseDoubleClick;
extern float stick_as_button_deadzone;
extern float g_TriggerThreshold;
extern std::map<UINT, ButtonState> buttonStates;

//scanning options


void LoadIniSettings();