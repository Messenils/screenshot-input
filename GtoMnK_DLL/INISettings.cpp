#include "pch.h"
#include "INISettings.h"
#include "Logging.h"
#include "InputState.h" 
#include "Input.h"
#include "ScanThread.h"

extern HMODULE g_hModule;

// For Controller Button States
std::map<UINT, ButtonState> buttonStates;
bool g_EnableMouseDoubleClick = false;
int righthanded;

// For Controller Setting
int controllerID = 0;
GamepadMethod g_GamepadMethod = GamepadMethod::SDL2;
float radial_deadzone, axial_deadzone, sensitivity, max_threshold, curve_slope, curve_exponent, sensitivity_multiplier, horizontal_sensitivity, vertical_sensitivity, look_accel_multiplier;
float stick_as_button_deadzone;
float g_TriggerThreshold = 175;

// For Initialization and Thread state
int startUpDelay = 0;
bool recheckHWND = true;
bool disableOverlayOptions = false;
bool g_EnableOpenXinput = false;

// Drawing & cursor state
int drawfakecursor = 0;
int drawProtoFakeCursor = 0; // From ProtoInput also for the Cursor visibility hooks
int createdWindowIsOwned = 1;
int ShowProtoCursorWhenImageUpdated = 1; // From ProtoInput
int mode = 1;
int responsetime = 4;

// For the Hooks
InputMethod g_InputMethod = InputMethod::PostMessage;
int getCursorPosHook, setCursorPosHook, clipCursorHook, getKeyStateHook, getAsyncKeyStateHook, getKeyboardStateHook, setCursorHook, setRectHook;

// For MessageFilterHook
bool g_filterRawInput = false;
bool g_filterMouseMove = false;
bool g_filterMouseActivate = false;
bool g_filterWindowActivate = false;
bool g_filterWindowActivateApp = false;
bool g_filterMouseWheel = false;
bool g_filterMouseButton = false;
bool g_filterKeyboardButton = false;

// For finding the game window
char iniWindowName[256] = { 0 };
char iniClassName[256] = { 0 };

// UGetExecutableFolder_main is for getting the executable folder path
std::string UGetExecutableFolder_main() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exePath(path);
    size_t lastSlash = exePath.find_last_of("\\/");
    return exePath.substr(0, lastSlash);
}

// UGetDllFolder_main is for getting the DLL folder path
std::string UGetDllFolder_main() {
    char path[MAX_PATH];
    GetModuleFileNameA(g_hModule, path, MAX_PATH);
    std::string dllPath(path);
    size_t lastSlash = dllPath.find_last_of("\\/");
    return dllPath.substr(0, lastSlash);
}

// getShortenedPath_Manual is for shortening the path
std::string getShortenedPath_Manual(const std::string& fullPath)
{
    const char separator = '\\';
    size_t last_separator_pos = fullPath.find_last_of(separator);
    if (last_separator_pos == std::string::npos) {
        return fullPath;
    }
    size_t second_last_separator_pos = fullPath.find_last_of(separator, last_separator_pos - 1);

    if (second_last_separator_pos == std::string::npos) {
        return fullPath;
    }
    std::string shortened = fullPath.substr(second_last_separator_pos + 1);

    return "...\\" + shortened;
}

void LoadIniSettings() {

    std::string iniPath = "";

    std::string exeIniPath = UGetExecutableFolder_main() + "\\GtoMnK.ini";

    if (GetFileAttributesA(exeIniPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        iniPath = exeIniPath;
        LOG("INI file found next the EXE at: %s", getShortenedPath_Manual(exeIniPath).c_str());
    }
    else {
        std::string dllIniPath = UGetDllFolder_main() + "\\GtoMnK.ini";

        if (GetFileAttributesA(dllIniPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            iniPath = dllIniPath;
            LOG("INI file found next the DLL at: %s", getShortenedPath_Manual(dllIniPath).c_str());
        }
        else {
            LOG("WARNING: No INI file found in next the DLL or the EXE files. Using default values...");
        }
    }

    char buffer[256];

    // [FindWindow]
    GetPrivateProfileStringA("FindWindow", "WindowName", NULL, iniWindowName, sizeof(iniWindowName), iniPath.c_str());
    GetPrivateProfileStringA("FindWindow", "ClassName", NULL, iniClassName, sizeof(iniClassName), iniPath.c_str());

    // [API]
    int inputMethod = GetPrivateProfileIntA("API", "InputMethod", 0, iniPath.c_str());
    if (inputMethod == 1) {
        g_InputMethod = InputMethod::RawInput;
    }
    else {
        g_InputMethod = InputMethod::PostMessage;
    }
    LOG("Using Input Method: %s", (g_InputMethod == InputMethod::RawInput) ? "RawInput" : "PostMessage");

    int gamepadMethod = GetPrivateProfileIntA("API", "GamepadMethod", 0, iniPath.c_str());
    if (gamepadMethod == 1) {
        g_GamepadMethod = GamepadMethod::XInput;
    }
    else {
        g_GamepadMethod = GamepadMethod::SDL2;   
    }
	LOG("Using Gamepad Method: %s", (g_GamepadMethod == GamepadMethod::XInput) ? "XInput" : "SDL2");

    g_EnableOpenXinput = GetPrivateProfileIntA("API", "EnableOpenXinput", 0, iniPath.c_str()) == 1;

    // [Hooks]
    getCursorPosHook = GetPrivateProfileIntA("Hooks", "GetCursorposHook", 1, iniPath.c_str());
    setCursorPosHook = GetPrivateProfileIntA("Hooks", "SetCursorposHook", 1, iniPath.c_str());
    clipCursorHook = GetPrivateProfileIntA("Hooks", "ClipCursorHook", 0, iniPath.c_str());
    getKeyStateHook = GetPrivateProfileIntA("Hooks", "GetKeystateHook", 1, iniPath.c_str());
    getAsyncKeyStateHook = GetPrivateProfileIntA("Hooks", "GetAsynckeystateHook", 1, iniPath.c_str());
    getKeyboardStateHook = GetPrivateProfileIntA("Hooks", "GetKeyboardstateHook", 1, iniPath.c_str());
    setCursorHook = GetPrivateProfileIntA("Hooks", "SetCursorHook", 0, iniPath.c_str());
    setRectHook = GetPrivateProfileIntA("Hooks", "SetRectHook", 0, iniPath.c_str());

    // [MessageFilter]
    g_filterRawInput = GetPrivateProfileIntA("MessageFilter", "RawInputFilter", 0, iniPath.c_str()) == 1;
    g_filterMouseMove = GetPrivateProfileIntA("MessageFilter", "MouseMoveFilter", 0, iniPath.c_str()) == 1;
    g_filterMouseActivate = GetPrivateProfileIntA("MessageFilter", "MouseActivateFilter", 0, iniPath.c_str()) == 1;
    g_filterWindowActivate = GetPrivateProfileIntA("MessageFilter", "WindowActivateFilter", 0, iniPath.c_str()) == 1;
    g_filterWindowActivateApp = GetPrivateProfileIntA("MessageFilter", "WindowActivateAppFilter", 0, iniPath.c_str()) == 1;
    g_filterMouseWheel = GetPrivateProfileIntA("MessageFilter", "MouseWheelFilter", 0, iniPath.c_str()) == 1;
    g_filterMouseButton = GetPrivateProfileIntA("MessageFilter", "MouseButtonFilter", 0, iniPath.c_str()) == 1;
    g_filterKeyboardButton = GetPrivateProfileIntA("MessageFilter", "KeyboardButtonFilter", 0, iniPath.c_str()) == 1;

    // [Settings]
    startUpDelay = GetPrivateProfileIntA("Settings", "StartUpDelay", 0, iniPath.c_str());
    recheckHWND = GetPrivateProfileIntA("Settings", "RecheckHWND", 1, iniPath.c_str()) == 1;
    disableOverlayOptions = (GetPrivateProfileIntA("Settings", "DisableOverlayOptions", 0, iniPath.c_str()) != 0);
    controllerID = GetPrivateProfileIntA("Settings", "Controllerid", 0, iniPath.c_str());
    mode = GetPrivateProfileIntA("Settings", "Mode", 1, iniPath.c_str());
    drawfakecursor = GetPrivateProfileIntA("Settings", "drawfakecursor", 0, iniPath.c_str());
    drawProtoFakeCursor = GetPrivateProfileIntA("Settings", "DrawProtoFakeCursor", 0, iniPath.c_str());
    ShowProtoCursorWhenImageUpdated = GetPrivateProfileIntA("Settings", "ShowCursorWhenImageUpdated", 1, iniPath.c_str());
    if (drawProtoFakeCursor == 1) {
        if (drawfakecursor == 1) {
            LOG("INI Info: Both 'drawfakecursor' and 'drawProtoFakeCursor' are enabled. Prioritizing the ProtoInput cursor.");
            drawfakecursor = 0; // Force the old cursor to be disabled.
        }
    }
    // Let the PointerWindow and RawInput window owned by the main window (game).
    createdWindowIsOwned = GetPrivateProfileIntA("Settings", "CreatedWindowIsOwned", 1, iniPath.c_str());
    responsetime = GetPrivateProfileIntA("Settings", "Responsetime", 4, iniPath.c_str());

    // [StickToMouse]
    righthanded = GetPrivateProfileIntA("StickToMouse", "Righthanded", 2, iniPath.c_str());
    GetPrivateProfileStringA("StickToMouse", "Sensitivity", "5.00", buffer, sizeof(buffer), iniPath.c_str()); sensitivity = std::stof(buffer);
    GetPrivateProfileStringA("StickToMouse", "Sensitivity_Multiplier", "2.40", buffer, sizeof(buffer), iniPath.c_str()); sensitivity_multiplier = std::stof(buffer);
    GetPrivateProfileStringA("StickToMouse", "Horizontal_Sensitivity", "0.0", buffer, sizeof(buffer), iniPath.c_str()); horizontal_sensitivity = std::stof(buffer);
    GetPrivateProfileStringA("StickToMouse", "Vertical_Sensitivity", "0.58", buffer, sizeof(buffer), iniPath.c_str()); vertical_sensitivity = std::stof(buffer);
    GetPrivateProfileStringA("StickToMouse", "Max_Threshold", "0.045", buffer, sizeof(buffer), iniPath.c_str()); max_threshold = std::stof(buffer);
    GetPrivateProfileStringA("StickToMouse", "Radial_Deadzone", "0.1", buffer, sizeof(buffer), iniPath.c_str()); radial_deadzone = std::stof(buffer);
    GetPrivateProfileStringA("StickToMouse", "Axial_Deadzone", "0.0", buffer, sizeof(buffer), iniPath.c_str()); axial_deadzone = std::stof(buffer);
    GetPrivateProfileStringA("StickToMouse", "Look_Accel_Multiplier", "1.40", buffer, sizeof(buffer), iniPath.c_str()); look_accel_multiplier = std::stof(buffer);
    GetPrivateProfileStringA("StickToMouse", "Curve_Slope", "0.16", buffer, sizeof(buffer), iniPath.c_str()); curve_slope = std::stof(buffer);
    GetPrivateProfileStringA("StickToMouse", "Curve_Exponent", "1.85", buffer, sizeof(buffer), iniPath.c_str()); curve_exponent = std::stof(buffer);

    // [KeyMapping] Section
    g_EnableMouseDoubleClick = GetPrivateProfileIntA("KeyMapping", "EnableMouseDoubleClick", 0, iniPath.c_str()) == 1;
    GetPrivateProfileStringA("KeyMapping", "TriggerThreshold", "40", buffer, sizeof(buffer), iniPath.c_str()); g_TriggerThreshold = std::stof(buffer);
    GetPrivateProfileStringA("KeyMapping", "StickAsButtonDeadzone", "0.25", buffer, sizeof(buffer), iniPath.c_str()); stick_as_button_deadzone = std::stof(buffer);

    GetPrivateProfileStringA("KeyMapping", "A", "13", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_A].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "B", "0", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_B].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "X", "42", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_X].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "Y", "0", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_Y].actions = Input::ParseActionString(buffer);

    // Triggers
    GetPrivateProfileStringA("KeyMapping", "LT", "-2", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_LT].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "RT", "-1", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_RT].actions = Input::ParseActionString(buffer);

    // Shoulder Buttons
    GetPrivateProfileStringA("KeyMapping", "RB", "0", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_RB].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "LB", "0", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_LB].actions = Input::ParseActionString(buffer);

    // D-Pad
    GetPrivateProfileStringA("KeyMapping", "D_UP", "14", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_DPAD_UP].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "D_DOWN", "15", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_DPAD_DOWN].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "D_LEFT", "16", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_DPAD_LEFT].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "D_RIGHT", "17", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_DPAD_RIGHT].actions = Input::ParseActionString(buffer);

    // Start & Back
    GetPrivateProfileStringA("KeyMapping", "Start", "1", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_START].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "Back", "3", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_BACK].actions = Input::ParseActionString(buffer);

    // Stick Buttons
    GetPrivateProfileStringA("KeyMapping", "RSB", "0", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_RSB].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "LSB", "4", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_LSB].actions = Input::ParseActionString(buffer);

    // Left Stick
    GetPrivateProfileStringA("KeyMapping", "LSU", "47", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_LSU].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "LSD", "43", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_LSD].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "LSL", "25", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_LSL].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "LSR", "28", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_LSR].actions = Input::ParseActionString(buffer);

    // Right Stick
    GetPrivateProfileStringA("KeyMapping", "RSU", "0", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_RSU].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "RSD", "0", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_RSD].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "RSL", "0", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_RSL].actions = Input::ParseActionString(buffer);
    GetPrivateProfileStringA("KeyMapping", "RSR", "0", buffer, sizeof(buffer), iniPath.c_str());
    buttonStates[CUSTOM_ID_RSR].actions = Input::ParseActionString(buffer);

	// [ScannerOptions] //input type 0 is move and click. 1 is only click. 2 is only move.
    ScanThread::scanAtype = GetPrivateProfileIntA("ScannerOptions", "AbmpInputAction", 0, iniPath.c_str());
    ScanThread::scanBtype = GetPrivateProfileIntA("ScannerOptions", "BbmpInputAction", 0, iniPath.c_str());
    ScanThread::scanXtype = GetPrivateProfileIntA("ScannerOptions", "XbmpInputAction", 0, iniPath.c_str());
    ScanThread::scanYtype = GetPrivateProfileIntA("ScannerOptions", "YbmpInputAction", 0, iniPath.c_str());
    ScanThread::Ctype = GetPrivateProfileIntA("ScannerOptions", "RSBbmpInputAction", 0, iniPath.c_str());
    ScanThread::Dtype = GetPrivateProfileIntA("ScannerOptions", "LSBbmpInputAction", 0, iniPath.c_str());
    ScanThread::Etype = GetPrivateProfileIntA("ScannerOptions", "RBbmpInputAction", 0, iniPath.c_str());
    ScanThread::Ftype = GetPrivateProfileIntA("ScannerOptions", "LBbmpInputAction", 0, iniPath.c_str());

    ScanThread::scanoption = GetPrivateProfileIntA("ScannerOptions", "PreScan", 0, iniPath.c_str());
    ScanThread::ShoulderNextBMP = GetPrivateProfileIntA("ScannerOptions", "ShouldersScrollPoints", 0, iniPath.c_str());

    ScanThread::Aisstatic = GetPrivateProfileIntA("ScannerOptions", "Astatic", 0, iniPath.c_str());
    ScanThread::Bisstatic = GetPrivateProfileIntA("ScannerOptions", "Bstatic", 0, iniPath.c_str());
    ScanThread::Xisstatic = GetPrivateProfileIntA("ScannerOptions", "Xstatic", 0, iniPath.c_str());
    ScanThread::Yisstatic = GetPrivateProfileIntA("ScannerOptions", "Ystatic", 0, iniPath.c_str());
}