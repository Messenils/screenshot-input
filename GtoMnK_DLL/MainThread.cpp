#include "pch.h"
#include "Logging.h"
#include "Hooks.h"
#include "Mouse.h"
#include "Keyboard.h"
#include "InputState.h"
#include "RawInput.h"
#include "MainThread.h"
#include "FakeCursor.h"
#include "OverlayMenu.h"
#include "EnableOpenXinput.h"
#include "INISettings.h"
#include "HandleMainWindow.h"
#include "GamepadState.h"
#include "Xinput_Gamepad.h"
#include "SDL2_Gamepad.h"
#include "ScanThread.h"
#include <thread>

using namespace GtoMnK;

// For Initialization and Thread state
HMODULE g_hModule = nullptr;
bool hooksinited = false;
bool loop = true;

// For Keyboard States
int keystatesend = 0;

// Others
HWND hwnd = nullptr;
HANDLE hMutex = nullptr;

// Drawing & cursor state
bool onoroff = true;
int MainThread::showmessage;
int counter = 0;
bool pausedraw = false;
// For SetRectHook and AdjustWindowRectHook
int leftrect, toprect, rightrect, bottomrect;
//CRITICAL_SECTION GtoMnK::ScanThread::critical;
// General
//bool gotcursoryet = false;
//POINT startdrag = { 0, 0 };
//DWORD lastClickTime = 0;
//int userealmouse;
//int alwaysdrawcursor;
//int doubleclicks;
//int scrolloutsidewindow;
//int quickMW;
//int scrollenddelay;
//int tick = 0;
//bool doscrollyes = false;
//POINT scroll = { 0, 0 };
//bool scrollmap = false;
//POINT rectignore = { 0, 0 };
//int ignorerect;

int AuseStatic, BuseStatic, XuseStatic, YuseStatic;

void DrawOverlay() {
    if (MainThread::showmessage == 0 && !(drawfakecursor && mode == 1 && onoroff)) {
        return;
    }
    HDC hdcWindow = GetDC(hwnd);
    if (!hdcWindow) {
        return;
    }
    if (MainThread::showmessage == 1) TextOutA(hdcWindow, Mouse::Xf, Mouse::Yf, "KEYBOARD MODE", 13);
    else if (MainThread::showmessage == 2) TextOutA(hdcWindow, Mouse::Xf, Mouse::Yf, "CURSOR MODE", 11);
    else if (MainThread::showmessage == 69) TextOutA(hdcWindow, Mouse::Xf, Mouse::Yf, "DISABLED", 8);
    else if (MainThread::showmessage == 70) TextOutA(hdcWindow, Mouse::Xf, Mouse::Yf, "ENABLED", 7);
    else if (MainThread::showmessage == 12) TextOutA(hdcWindow, 20, 20, "CONTROLLER DISCONNECTED", 23);
    else if (drawfakecursor && mode == 1 && onoroff) {
        
    }
    Mouse::DrawBeautifulCursor(hdcWindow);
    ReleaseDC(hwnd, hdcWindow);
}

DWORD WINAPI ThreadFunction(LPVOID lpParam) {
    LOG("ThreadFunction started.");
    
    LoadIniSettings();
    LOG("INI settings is Loaded");

    // Check Controller ID
    if (controllerID < 0) {
        LOG("Controller is disabled by INI. Thread is exiting.");
        loop = false;
        return 0;
    }
    else {
        LOG("Using controller ID: %d", controllerID);
    }

    // Startup Delay
    if (startUpDelay > 0) {
        LOG("Waiting for startup delay of %dSec before hooking...", startUpDelay);
        Sleep(startUpDelay * 1000);
    }

    // Find Main Window
    LOG("Searching for Game Window...");
    if (iniWindowName[0] != '\0') { LOG("Filter Name: '%s'", iniWindowName); }
    if (iniClassName[0] != '\0') { LOG("Filter Class: '%s'", iniClassName); }
    hwnd = GetMainWindowHandle(GetCurrentProcessId(), iniWindowName, iniClassName, 300000);
    if (!hwnd) {
        LOG("CRITICAL: Game window not found after 5min. Thread exiting to prevent crash.");
        loop = false;
        return 0;
    }
    LOG("Initial window handle acquired: 0x%p", hwnd);

    InitializeCriticalSection(&ScanThread::critical);
    if (!ScanThread::enumeratebmps()) //false means no bmps found. also counts statics
    { 
        LOG("BMPs enumerated but not found: 0x%p", hwnd);
        if (ScanThread::scanoption == 1)
        {
            ScanThread::scanoption = 0;
            LOG("Error. Nothing to scan for. Disabling scanoption: 0x%p", hwnd);
        }
    }
    else {
        LOG("BMPs found: 0x%p", hwnd);
        ScanThread::staticPointA.assign(ScanThread::numphotoA + 1, POINT{ 0, 0 });
        ScanThread::staticPointB.assign(ScanThread::numphotoB + 1, POINT{ 0, 0 });
        ScanThread::staticPointX.assign(ScanThread::numphotoX + 1, POINT{ 0, 0 });
        ScanThread::staticPointY.assign(ScanThread::numphotoY + 1, POINT{ 0, 0 });
    }
    ScanThread::initovector();
    if (ScanThread::scanoption == 1)
    { //starting bmp continous scanner
        ScanThread::StartScanThread(g_hModule, ScanThread::Aisstatic, ScanThread::Bisstatic, ScanThread::Xisstatic, ScanThread::Yisstatic, ScanThread::scanoption);
        LOG("BMP scanner started: 0x%p", hwnd);
    }

    // Initialize Fake Cursor
    if (drawProtoFakeCursor == 1) {
        LOG("ProtoInput Fake Cursor is enabled. Initializing...");
        FakeCursor::Initialise();
        if (!createdWindowIsOwned)
            FakeCursor::EnableDisableFakeCursor(true);
    }
    else if (drawfakecursor) {
        LOG("Simple Fake Cursor is enabled.");
    }

    // Setup Hooks
    Hooks::SetupHooks();
    hooksinited = true;

    if (g_InputMethod == InputMethod::RawInput) {
        RawInput::Initialize();
    }

    // Initialize Overlay
    if (!disableOverlayOptions)
    {
        OverlayMenu::state.Initialise();
        LOG("Overlay Options Initialized.");
    }
    else {
        LOG("Overlay Options is disabled.");
    }

    // Initialize Input API
    if (g_GamepadMethod == GamepadMethod::SDL2) {
        SDL2_Initialize();
    }
    else {
        XInput_Initialize();
    }

    ULONGLONG menuToggleTimer = 0;
    bool menuTogglePending = false;
    ULONGLONG lastMoveTime = 0;
    const DWORD MOVE_UPDATE_INTERVAL = 8;
    CustomControllerState state; // Reused every frame

    while (loop) {
        bool movedmouse = false;

        if (recheckHWND) {
            if (hwnd && !IsWindow(hwnd)) {
                LOG("Window handle became invalid. Resetting...");
                hwnd = nullptr;
            }
        }

        if (!hwnd) {
            hwnd = GetMainWindowHandle(GetCurrentProcessId(), iniWindowName, iniClassName, 300000);
            if (!hwnd) {
                LOG("CRITICAL: Game window missing for 5 minute.");
                loop = false;
                break;
            }
            LOG("Acquired new window handle: 0x%p", hwnd);
        }

        RECT rect;
        if (!GetClientRect(hwnd, &rect)) {
            rect.left = 0; rect.top = 0; rect.right = 800; rect.bottom = 600;
        }

        bool isConnected = false;

        switch (g_GamepadMethod) {
        case GamepadMethod::SDL2:
            isConnected = SDL2_GetState(state);
            break;
        case GamepadMethod::XInput:
        default:
            isConnected = XInput_GetState(state);
            break;
        }

        if (isConnected) {

            if (!disableOverlayOptions && OverlayMenu::state.IsReady())
            {
                bool backDown = state.buttons[CUSTOM_ID_BACK];
                bool dpadDown = state.buttons[CUSTOM_ID_DPAD_DOWN];

                if (backDown && dpadDown) {
                    ProcessButton(CUSTOM_ID_BACK, false);
                    ProcessButton(CUSTOM_ID_DPAD_DOWN, false);

                    if (!menuTogglePending) {
                        menuToggleTimer = GetTickCount64();
                        menuTogglePending = true;
                    }
                    else if (GetTickCount64() - menuToggleTimer > 50) {
                        // Toggle Menu
                        bool newState = !OverlayMenu::state.isMenuOpen;
                        OverlayMenu::state.EnableDisableMenu(newState);
                        menuTogglePending = false;
                        Mouse::Xf = Mouse::Xf;

                        CustomControllerState waitState;
                        do {
                            Sleep(100); // Important
                            bool waitConnected = false;
                            if (g_GamepadMethod == GamepadMethod::SDL2) {waitConnected = SDL2_GetState(waitState);}
                            else {waitConnected = XInput_GetState(waitState);}
                            if (!waitConnected) break;
                        } while (waitState.buttons[CUSTOM_ID_BACK] || waitState.buttons[CUSTOM_ID_DPAD_DOWN]);
                        continue;
                    }
                }
                else {
                    menuTogglePending = false;
                }
            }

            if (MainThread::showmessage == 12) MainThread::showmessage = 0; // "disconnected" message on reconnect

            if (!disableOverlayOptions && OverlayMenu::state.isMenuOpen) {
                OverlayMenu::state.ProcessInput(state);
            }
            else {
                // Buttons
                ProcessButton(CUSTOM_ID_A, state.buttons[CUSTOM_ID_A]);
                ProcessButton(CUSTOM_ID_B, state.buttons[CUSTOM_ID_B]);
                ProcessButton(CUSTOM_ID_X, state.buttons[CUSTOM_ID_X]);
                ProcessButton(CUSTOM_ID_Y, state.buttons[CUSTOM_ID_Y]);
                ProcessButton(CUSTOM_ID_RB, state.buttons[CUSTOM_ID_RB]);
                ProcessButton(CUSTOM_ID_LB, state.buttons[CUSTOM_ID_LB]);
                ProcessButton(CUSTOM_ID_RSB, state.buttons[CUSTOM_ID_RSB]);
                ProcessButton(CUSTOM_ID_LSB, state.buttons[CUSTOM_ID_LSB]);
                ProcessButton(CUSTOM_ID_DPAD_UP, state.buttons[CUSTOM_ID_DPAD_UP]);
                ProcessButton(CUSTOM_ID_DPAD_DOWN, state.buttons[CUSTOM_ID_DPAD_DOWN]);
                ProcessButton(CUSTOM_ID_DPAD_LEFT, state.buttons[CUSTOM_ID_DPAD_LEFT]);
                ProcessButton(CUSTOM_ID_DPAD_RIGHT, state.buttons[CUSTOM_ID_DPAD_RIGHT]);
                ProcessButton(CUSTOM_ID_START, state.buttons[CUSTOM_ID_START]);
                ProcessButton(CUSTOM_ID_BACK, state.buttons[CUSTOM_ID_BACK]);

                // Triggers
                ProcessTrigger(CUSTOM_ID_LT, state.LeftTrigger);
                ProcessTrigger(CUSTOM_ID_RT, state.RightTrigger);

                // Analog Sticks (as buttons)
                // Left Thumbsticks
                if (onoroff && (mode == 0 || (mode == 1 && righthanded == 1))) {
                    float normLX = static_cast<float>(state.ThumbLX) / 32767.0f;
                    float normLY = static_cast<float>(state.ThumbLY) / 32767.0f;
                    float magnitudeL = static_cast<float>(sqrt(normLX * normLX + normLY * normLY));
                    bool isLSU = false, isLSD = false, isLSL = false, isLSR = false;
                    if (magnitudeL > stick_as_button_deadzone) {
                        float angleDeg = atan2(normLY, normLX) * 180.0f / 3.1415926535f;
                        if (angleDeg < 0) angleDeg += 360.0f;
                        if (angleDeg > 22.5f && angleDeg < 157.5f) isLSU = true;
                        if (angleDeg > 202.5f && angleDeg < 337.5f) isLSD = true;
                        if (angleDeg > 112.5f && angleDeg < 247.5f) isLSL = true;
                        if (angleDeg > 292.5f || angleDeg < 67.5f) isLSR = true;
                    }
                    ProcessButton(CUSTOM_ID_LSU, isLSU);
                    ProcessButton(CUSTOM_ID_LSD, isLSD);
                    ProcessButton(CUSTOM_ID_LSL, isLSL);
                    ProcessButton(CUSTOM_ID_LSR, isLSR);
                }
				// Right Thumbsticks
                if (onoroff && (mode == 0 || (mode == 1 && righthanded == 0))) {
                    float normRX = static_cast<float>(state.ThumbRX) / 32767.0f;
                    float normRY = static_cast<float>(state.ThumbRY) / 32767.0f;
                    float magnitudeR = static_cast<float>(sqrt(normRX * normRX + normRY * normRY));
                    bool isRSU = false, isRSD = false, isRSL = false, isRSR = false;
                    if (magnitudeR > stick_as_button_deadzone) {
                        float angleDeg = atan2(normRY, normRX) * 180.0f / 3.1415926535f;
                        if (angleDeg < 0) angleDeg += 360.0f;
                        if (angleDeg > 22.5f && angleDeg < 157.5f) isRSU = true;
                        if (angleDeg > 202.5f && angleDeg < 337.5f) isRSD = true;
                        if (angleDeg > 112.5f && angleDeg < 247.5f) isRSL = true;
                        if (angleDeg > 292.5f || angleDeg < 67.5f) isRSR = true;
                    }
                    ProcessButton(CUSTOM_ID_RSU, isRSU);
                    ProcessButton(CUSTOM_ID_RSD, isRSD);
                    ProcessButton(CUSTOM_ID_RSL, isRSL);
                    ProcessButton(CUSTOM_ID_RSR, isRSR);
                }

                // Mouse Movement
                if (mode == 1 && onoroff) {
                    SHORT thumbX = (righthanded == 1) ? state.ThumbRX : state.ThumbLX;
                    SHORT thumbY = (righthanded == 1) ? state.ThumbRY : state.ThumbLY;
                    POINT delta = ThumbstickMouseMove(thumbX, thumbY);
                    if (delta.x != 0 || delta.y != 0) {
                        Mouse::Xf += delta.x; Mouse::Yf += delta.y;
                        Mouse::Xf = std::max((LONG)rect.left, std::min((LONG)Mouse::Xf, (LONG)rect.right - 1));
                        Mouse::Yf = std::max((LONG)rect.top, std::min((LONG)Mouse::Yf, (LONG)rect.bottom - 1));

                        //ProtoInput Fake Cursor update
                        if (drawProtoFakeCursor == 1) {
                            EnterCriticalSection(&ScanThread::critical);
                            FakeCursor::NotifyUpdatedCursorPosition();
                            LeaveCriticalSection(&ScanThread::critical);
                        }

                        ULONGLONG currentTime = GetTickCount64();
                        if (currentTime - lastMoveTime > MOVE_UPDATE_INTERVAL) {
                            lastMoveTime = currentTime;
                            if (g_InputMethod == InputMethod::RawInput) {
                                Input::SendActionDelta(delta.x, delta.y);
                            }
                            else {
                                POINT screenPos = { (LONG)Mouse::Xf, (LONG)Mouse::Yf };
                                ClientToScreen(hwnd, &screenPos);
                                Input::SendMouseMove(screenPos.x, screenPos.y);
                            }
                        }
                    }
                }
            }
        }
        else {
            MainThread::showmessage = 12; // Controller disconnected
        }

        // Drawing and Message
        DrawOverlay();
        if (MainThread::showmessage != 0 && MainThread::showmessage != 12) {
            if (++counter > 150) {
                MainThread::showmessage = 0;
                counter = 0;
            }
        }

        if (responsetime <= 0)
            responsetime = 1;
        Sleep(responsetime);
    }

	// Cleanup
    if (g_GamepadMethod == GamepadMethod::SDL2) {
        SDL2_Cleanup();
    }
    DeleteCriticalSection(&ScanThread::critical);
    LOG("ThreadFunction gracefully exiting.");
    return 0;
}