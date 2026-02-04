#pragma once
#include <map>
#include "Input.h"

struct ButtonState {
    bool isPhysicallyPressed = false;
    ULONGLONG pressTime = 0;
    size_t activeActionIndex = static_cast<size_t>(-1);
    bool pressActionFired = false;
    std::vector<Action> actions;
    std::string heldActionString = "0";
};

extern volatile bool g_isInitialized;
extern HMODULE g_hModule;
extern bool loop;
extern bool hooksinited;
extern std::map<UINT, ButtonState> buttonStates;

// Used by Input.cpp, Hooks.cpp, dllmain.cpp, etc.
extern GtoMnK::InputMethod g_InputMethod;
extern HWND hwnd;

// For finding the game window
extern char iniWindowName[256];
extern char iniClassName[256];

extern int keystatesend;
extern bool g_EnableMouseDoubleClick;

extern HWND GetMainWindowHandle(DWORD targetPID, const char* requiredName = nullptr, const char* requiredClass = nullptr, DWORD timeoutMS = 0);
DWORD WINAPI ThreadFunction(LPVOID lpParam);
namespace GtoMnK {
    class MainThread {
    //private:
    public:
        static int showmessage;
    };
}