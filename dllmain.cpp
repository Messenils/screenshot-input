// dllmain.cpp : Defines the entry point for the DLL application.


#include "pch.h"
#include <cmath>
#define NOMINMAX
#include <windows.h>
#include <algorithm>
//#include <windowsx.h>
#include "MinHook.h"
#include <Xinput.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <iostream>
#include <vector>
#include <cstdio>  // for swprintf
#include <psapi.h>
#include <string>
#include <cstdlib> // For strtoul
#include <dwmapi.h>
#include <thread>
#include "MessageFilterHooks.h"
#include "dllmain.h"
#include "PostKeyFunction.cpp"
#pragma comment(lib, "dwmapi.lib")

#pragma comment(lib, "Xinput9_1_0.lib")

std::vector<POINT> staticPointA;
std::vector<POINT> staticPointB;
std::vector<POINT> staticPointX;
std::vector<POINT> staticPointY;

int scanAtype = 0;
int scanBtype = 0;
int scanXtype = 0;
int scanYtype = 0;

HCURSOR WINAPI HookedSetCursor(HCURSOR hcursor) {
	    EnterCriticalSection(&critical);
		hCursor = hcursor; // Store the cursor handle  

        hcursor = fpSetCursor(hcursor);
		LeaveCriticalSection(&critical);
        return hcursor;
}
////SetRect_t)(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom);
BOOL WINAPI HookedSetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom) {
	xLeft = leftrect; // Set the left coordinate to Xrect  
    yTop = toprect; // Set the top coordinate to Yrect  
    
    xRight = rightrect; // Set the right coordinate to Xrect + 10 
	yBottom = bottomrect; // Set the bottom coordinate to Yrect + 10    
 
    
	bool result = fpSetRect(lprc, xLeft, yTop, xRight, yBottom);
    return result;
}

BOOL WINAPI HookedAdjustWindowRect(LPRECT lprc, DWORD  dwStyle, BOOL bMenu) {
    lprc->top = toprect; // Set the left coordinate to Xrect  
    lprc->bottom = bottomrect; // Set the left coordinate to Xrect  
    lprc->left = leftrect; // Set the left coordinate to Xrect  
    lprc->right = rightrect; // Set the left coordinate to Xrect  

    bool result = fpAdjustWindowRect(lprc, dwStyle, bMenu);
    return result;
}

SHORT WINAPI HookedGetAsyncKeyState(int vKey)
{
    if (keyState[vKey] & 0x80) {
        return 0x8000;   // key is down
    }
    else {
        return 0x0000;   // key is up
    }
}

HWND g_rawInputHwnd = nullptr;

const int RAWINPUT_BUFFER_SIZE = 20;
RAWINPUT g_inputBuffer[RAWINPUT_BUFFER_SIZE]{};
//std::vector<HWND> g_forwardingWindows{};
// Global/static counter
struct FakeKey {
    USHORT vkey;
    bool press;
};

static std::vector<FakeKey> g_fakeKeys;

// Add a fake key event to the array
void GenerateRawKey(USHORT vk, bool press) {
    g_fakeKeys.push_back({ vk, press });
    if (vk < 256) {
        if (press) {
            keyState[vk] |= 0x80;   // mark as "down"
        }
        else {
            keyState[vk] &= ~0x80;  // clear "down" bit
        }
    }
}

BOOL WINAPI HookedGetKeyboardState(PBYTE lpKeyState) {
    if (!lpKeyState) {
        return FALSE;
    }
    
    // Call the original function to get real states //disabled cause of testing
   // BOOL result = fpGetKeyboardState(lpKeyState);
    memset(lpKeyState, 0, 256);
    // Overlay fake states
    for (int vk = 0; vk < 256; ++vk) {
        if (keyState[vk] & 0x80) {
            lpKeyState[vk] |= 0x80;   // force down
        }
        else { //not needed if fpGetKeyboardState
            lpKeyState[vk] &= ~0x80;  // force up
        }
    }

    return TRUE;
}


UINT WINAPI HookedGetRawInputData(
    HRAWINPUT hRawInput,
    UINT uiCommand,
    LPVOID pData,
    PUINT pcbSize,
    UINT cbSizeHeader
) {
    // Call the original function
    UINT result = fpGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);

    if (uiCommand == RID_INPUT && pData != nullptr) {
        RAWINPUT* raw = (RAWINPUT*)pData;
	//	raw->header.wParam = RIM_INPUT; // Ensure wParam indicates input
        if (raw->header.dwType == RIM_TYPEMOUSE && raw->header.wParam == RIM_INPUT) {

            if (delta.x !=0)
                raw->data.mouse.lLastX = raw->data.mouse.lLastX + delta.x;
            if (delta.y != 0)
                raw->data.mouse.lLastY = raw->data.mouse.lLastY + delta.y;

			if (rawmouseR == true || rawmouseL == true || rawmouseWd == true || rawmouseWu == true)
                raw->data.mouse.usButtonFlags = 0;

            if (rawmouseR == true)
                raw->data.mouse.usButtonFlags |= RI_MOUSE_RIGHT_BUTTON_DOWN;
            else
                raw->data.mouse.usButtonFlags |= RI_MOUSE_RIGHT_BUTTON_UP;

            if (rawmouseL == true)
                raw->data.mouse.usButtonFlags |= RI_MOUSE_LEFT_BUTTON_DOWN;
            else
                raw->data.mouse.usButtonFlags |= RI_MOUSE_LEFT_BUTTON_UP;


             
            if (rawmouseWd == true)
            {
                raw->data.mouse.usButtonFlags |= RI_MOUSE_WHEEL;
                raw->data.mouse.usButtonData = -120;
            }
			else if (rawmouseWu == true)
            {
                raw->data.mouse.usButtonFlags |= RI_MOUSE_WHEEL;
                raw->data.mouse.usButtonData = 120;
            }

        }
        if (raw->header.dwType == RIM_TYPEKEYBOARD && !g_fakeKeys.empty()) {
            // Take the first fake key from the array
            FakeKey fk = g_fakeKeys.front();
            g_fakeKeys.erase(g_fakeKeys.begin());

            raw->data.keyboard.VKey = fk.vkey;
            raw->data.keyboard.MakeCode = MapVirtualKey(fk.vkey, MAPVK_VK_TO_VSC);
            raw->data.keyboard.Flags = fk.press ? RI_KEY_MAKE : RI_KEY_BREAK;
            raw->data.keyboard.Message = fk.press ? WM_KEYDOWN : WM_KEYUP;
        }

    }

    return result;
}

// Hooked GetKeyState
SHORT WINAPI HookedGetKeyState(int nVirtKey) {
    if (keyState[nVirtKey] & 0x80) {
        return 0x8000;   // key is down
    }
    else {
        return 0x0000;   // key is up
    }
}

HWND WINAPI HookedCreateWindowExA( DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth,  int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    // Call the original function
    HWND hWnd = fpCreateWindowExA(
        dwExStyle, lpClassName, lpWindowName, dwStyle,
        X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

    if (hWnd) {
        g_windows.push_back(hWnd);
        // Optional: debug output
        // printf("Created window: %s, handle=%p\n", lpWindowName, hWnd);
    }

    return hWnd;
}

HWND WINAPI HookedCreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
//    // Call the original function
    HWND hWnd = fpCreateWindowExW(
        dwExStyle, lpClassName, lpWindowName, dwStyle,
                X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

    if (hWnd) {
       g_windows.push_back(hWnd);

        // Optional: debug output
        // wprintf(L"Created window: %s, handle=%p\n", lpWindowName, hWnd);
    }

    return hWnd;
}
bool inwindow = true;
BOOL WINAPI MyGetCursorPos(PPOINT lpPoint) {
    if (lpPoint) 
    {
        POINT mpos;
        mpos.y = Yf;
        mpos.x = Xf;

        if (scrollmap == false)
        { 

            if (ignorerect == 1) {
                mpos.x = Xf + rectignore.x; //hwnd coordinates 0-800 on a 800x600 window
                mpos.y = Yf + rectignore.y;//hwnd coordinate s0-600 on a 800x600 window
				lpPoint->x = mpos.x; 
				lpPoint->y = mpos.y;    
            }
            else {
                if (hwnd)
                {
                    ClientToScreen(hwnd, &mpos);
                }
                lpPoint->x = mpos.x;
                lpPoint->y = mpos.y;
            }
        }
            
		else
		{
            mpos.x = scroll.x;
            mpos.y = scroll.y;
            if (hwnd)
            { 
                ClientToScreen(hwnd, &mpos);
            }

            lpPoint->x = mpos.x;
            lpPoint->y = mpos.y;
		}
        return TRUE;
    }
    return FALSE;
}
BOOL WINAPI HookedGetCursorInfo(PCURSORINFO pci) {
    bool result = fpGetCursorInfo(pci);
    if (result == true)
    {
        POINT nypt;
        MyGetCursorPos(&nypt);
        pci->ptScreenPos.x = nypt.x;
        pci->ptScreenPos.y = nypt.y;
    }
    return TRUE;
}
POINT mpos;
BOOL WINAPI MySetCursorPos(int X, int Y) {
    if (hwnd)
    {
        POINT point;
        point.x = X;
        point.y = Y;
        ScreenToClient(hwnd, &point);
        Xf = point.x; 
        Yf = point.y; 
    }
    return TRUE; //fpSetCursorPos(lpPoint); // Call the original SetCursorPos function
}
BOOL WINAPI HookedClipCursor(const RECT* lpRect) {
    return true; //nonzero bool or int
    //return originalClipCursor(nullptr);

}
POINT windowpos(HWND window, int ignorerect, bool getpos) //return pos if true
{
    if (ignorerect == 0)
    {
        POINT pos = { 0,0 };
        RECT recte;
        GetWindowRect(window, &recte);
        if (getpos) {
            pos.x = recte.left;
            pos.y = recte.top;
        }
        else {
            pos.x = recte.right - recte.left;
            pos.y = recte.bottom - recte.top;
        }
        return pos;
    }
    else
    {
        POINT pos = { 0,0 };
        RECT frameBounds;
        HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frameBounds, sizeof(frameBounds));
        if (SUCCEEDED(hr))
        {
            if (getpos)
            {
                pos.x = frameBounds.left;
                pos.y = frameBounds.top;
            }
            else
            {
                pos.x = frameBounds.right - frameBounds.left;
                pos.y = frameBounds.bottom - frameBounds.top;
            }
        }
        return pos;
    }
}
POINT GetStaticFactor(POINT pp, int doscale, bool isnotbmp)
{
   // FLOAT ny;
    POINT currentres = windowpos(hwnd, ignorerect, false);
    FLOAT currentwidth = static_cast<float>(currentres.x);
    FLOAT currentheight = static_cast<float>(currentres.y);
    if (doscale == 1)
    {
        float scalex = currentwidth / 1024.0f;
        float scaley = currentheight / 768.0f;

        pp.x = static_cast<int>(std::lround(pp.x * scalex));
        pp.y = static_cast<int>(std::lround(pp.y * scaley));
    }
    if (doscale == 2) //4:3 blackbar only x
    {
        float difference = 0.0f;
        float newwidth = currentwidth;
        float curraspect = currentheight / currentwidth;
        if (curraspect < 0.75f)
        {
            newwidth = currentheight / 0.75f;
            if (isnotbmp) //cant pluss blackbars on bmps
                difference = (currentwidth - newwidth) / 2;
        }
        float scalex = newwidth / 1024.0f;
        float scaley = currentheight / 768.0f;
        pp.x = static_cast<int>(std::lround(pp.x * scalex) + difference);
        pp.y = static_cast<int>(std::lround(pp.y * scaley));
    }
    if (doscale == 3) //only vertical stretch equal
    {
        float difference = 0.0f;
        float newwidth = currentwidth;
        float curraspect = currentheight / currentwidth;
        if (curraspect < 0.5625f)
        {
            newwidth = currentheight / 0.5625f;
            if (isnotbmp) //cant pluss blackbars on bmps
                difference = (currentwidth - newwidth) / 2;
        }
        float scalex = newwidth / 1337.0f;
        float scaley = currentheight / 768.0f;
        pp.x = static_cast<int>(std::lround(pp.x * scalex) + difference);
        pp.y = static_cast<int>(std::lround(pp.y * scaley));
    }
    return pp;
}

bool Mutexlock(bool lock) {
    // Create a named mutex
    if (lock == true)
    {
        hMutex = CreateMutexA(
            NULL,      // Default security
            FALSE,     // Initially not owned
            "Global\\PuttingInputByMessenils" // Name of mutex
        );
        if (hMutex == NULL) {
            std::cerr << "CreateMutex failed: " << GetLastError() << std::endl;
            MessageBox(NULL, "Error!", "Failed to create mutex", MB_OK | MB_ICONINFORMATION);
            return false;
        }
        // Check if mutex already exists
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            Sleep(5);
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
            Mutexlock(true); //is this okay?
            
        }
    }
    if (lock == false)
    {
        ReleaseMutex(hMutex);

        CloseHandle(hMutex);
       // hMutex = nullptr; // Optional: Prevent dangling pointer

    }
    return true;
}
BOOL WINAPI HookedRegisterRawInputDevices(PRAWINPUTDEVICE pRawInputDevices, UINT uiNumDevices, UINT cbSize)
{
    // Modify all devices in the array
  //  MessageBoxA(NULL, "RegisterRawInputDevices Hooked!", "Info", MB_OK | MB_ICONINFORMATION);
    for (UINT i = 0; i < uiNumDevices; ++i) {
        pRawInputDevices[i].dwFlags |= RIDEV_INPUTSINK;
    }
    if (pRawInputDevices[0].hwndTarget == NULL)
        MessageBoxA(NULL, "RegisterRawInputDevices failed! Window is NULL", "Error", MB_OK | MB_ICONERROR);
    BOOL result = fpRegisterRawInputDevices(pRawInputDevices, uiNumDevices, cbSize);
    hwnd = pRawInputDevices[0].hwndTarget;
    if (result == FALSE)
    {
        MessageBoxA(NULL, "RegisterRawInputDevices failed!", "Error", MB_OK | MB_ICONERROR);
	}
    return result;
}
void SetupHook() {
    if (MH_Initialize() != MH_OK) {
        MessageBox(NULL, "Failed to initialize MinHook", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    //each of there hooks have a high chance of crashing the game

    if (getcursorposhook == 1 || getcursorposhook == 2) {
        MH_CreateHookApi(L"user32", "GetCursorPos", &MyGetCursorPos, reinterpret_cast<LPVOID*>(&fpGetCursorPos));
        MH_EnableHook(&GetCursorPos);
    }
    if (setcursorposhook == 1) {
        MH_CreateHook(&SetCursorPos, &MySetCursorPos, reinterpret_cast<LPVOID*>(&fpSetCursorPos));
        MH_EnableHook(&SetCursorPos);
    }
    if (getkeystatehook == 1) {
        MH_CreateHook(&GetAsyncKeyState, &HookedGetAsyncKeyState, reinterpret_cast<LPVOID*>(&fpGetAsyncKeyState));
        MH_EnableHook(&GetAsyncKeyState);
    }
    if (getasynckeystatehook == 1) {
        MH_CreateHook(&GetKeyState, &HookedGetKeyState, reinterpret_cast<LPVOID*>(&fpGetKeyState));
        MH_EnableHook(&GetKeyState);
    }
    if (clipcursorhook == 1) {
        MH_CreateHook(&ClipCursor, &HookedClipCursor, reinterpret_cast<LPVOID*>(&fpClipCursor));
        MH_EnableHook(&ClipCursor);
    }
    if (setrecthook == 1) {
        MH_CreateHook(&SetRect, &HookedSetRect, reinterpret_cast<LPVOID*>(&fpSetRect));
        MH_EnableHook(&SetRect);

        MH_CreateHook(&AdjustWindowRect, &HookedAdjustWindowRect, reinterpret_cast<LPVOID*>(&fpAdjustWindowRect));
        MH_EnableHook(&AdjustWindowRect);
    }
    if (setcursorhook == 1)
    {
        MH_CreateHook(&SetCursor, &HookedSetCursor, reinterpret_cast<LPVOID*>(&fpSetCursor));
        MH_EnableHook(&SetCursor);
    }
    if (rawinputhook == 1 || rawinputhook == 2)
    {
        MH_CreateHook(&GetRawInputData, &HookedGetRawInputData,reinterpret_cast<LPVOID*>(&fpGetRawInputData));
        MH_EnableHook(&GetRawInputData);
    }
    if (GetCursorInfoHook == 1)
    {
        MH_CreateHook(&GetCursorInfo, &HookedGetCursorInfo, reinterpret_cast<LPVOID*>(&fpGetCursorInfo));
        MH_EnableHook(&GetCursorInfo);
    }
    if (GetKeyboardStateHook == 1)
    {
        MH_CreateHook(&GetKeyboardState, &HookedGetKeyboardState, reinterpret_cast<LPVOID*>(&fpGetKeyboardState));
        MH_EnableHook(&GetKeyboardState);
    }

    // this was to gather all created windows in an array for postmessage. not seen it effective yet
  //  MH_CreateHook(&CreateWindowExA, &HookedCreateWindowExA, reinterpret_cast<LPVOID*>(&fpCreateWindowExA));
  //  MH_EnableHook(&CreateWindowExA);
  ///  MH_CreateHook(&CreateWindowExW, &HookedCreateWindowExW, reinterpret_cast<LPVOID*>(&fpCreateWindowExW));
  //  MH_EnableHook(&CreateWindowExW);

    if (registerrawinputhook)
    { 
        MH_CreateHook(&RegisterRawInputDevices, &HookedRegisterRawInputDevices, reinterpret_cast<LPVOID*>(&fpRegisterRawInputDevices));
        MH_EnableHook(&RegisterRawInputDevices);
    }
    if (g_filterRawInput || g_filterMouseMove || g_filterMouseActivate || g_filterWindowActivate
        || g_filterWindowActivateApp || g_filterMouseWheel || g_filterMouseButton || g_filterKeyboardButton) // If one of them is enabled
    { // MessageFilter hooks
        // FIX: Add "MessageFilterHook::" before the function names
        MH_CreateHook(&GetMessageA, &ScreenshotInput::MessageFilterHook::Hook_GetMessageA, reinterpret_cast<LPVOID*>(&fpGetMessageA));
        MH_EnableHook(&GetMessageA);
        MH_CreateHook(&GetMessageW, &ScreenshotInput::MessageFilterHook::Hook_GetMessageW, reinterpret_cast<LPVOID*>(&fpGetMessageW));
        MH_EnableHook(&GetMessageW);
        MH_CreateHook(&PeekMessageA, &ScreenshotInput::MessageFilterHook::Hook_PeekMessageA, reinterpret_cast<LPVOID*>(&fpPeekMessageA));
        MH_EnableHook(&PeekMessageA);
        MH_CreateHook(&PeekMessageW, &ScreenshotInput::MessageFilterHook::Hook_PeekMessageW, reinterpret_cast<LPVOID*>(&fpPeekMessageW));
        MH_EnableHook(&PeekMessageW);
    }
    //MessageBox(NULL, "hooks done", "okay", MB_OK | MB_ICONINFORMATION);
    hooksinited = true;
    //MH_EnableHook(MH_ALL_HOOKS);
}


void vibrateController(int controllerId, WORD strength)
{
    XINPUT_VIBRATION vibration = {};
    vibration.wLeftMotorSpeed = strength;   // range: 0 - 65535
    vibration.wRightMotorSpeed = strength;

    // Activate vibration
    XInputSetState(controllerId, &vibration);

    // Keep vibration on for 1 second
    Sleep(50); // milliseconds

    // Stop vibration
    vibration.wLeftMotorSpeed = 0;
    vibration.wRightMotorSpeed = 0;
    XInputSetState(controllerId, &vibration);
}
void Focuser()
{
    if (hwnd != GetFocus())
    { 
		SetFocus(hwnd);
        PostMessage(hwnd,WM_MOUSEACTIVATE, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN));
    }
  //  if (hwnd != GetFocus())
  //      SetForegroundWindow(hwnd);
     return;
}
bool SendMouseClick(int x, int y, int z, int many) {
    // Create a named mutex
    if (sendfocus == 1)
        Focuser();
    POINT heer;
     
    unsigned int sig = ScreenshotInput_MOUSE_SIGNATURE;

    heer.x = x;
    heer.y = y;
    if (getcursorposhook == 2)
        ClientToScreen(hwnd, &heer);
    if (userealmouse == 0) 
        {
        LPARAM clickPos = MAKELPARAM(heer.x, heer.y);
        if ( z == 1){
            
            unsigned int wParamDown = VK_LBUTTON | sig; // Add signature
            unsigned int  wParamUp = 0 | sig;            // Add signature

            PostMessage(hwnd, WM_LBUTTONDOWN, wParamDown, clickPos);
            PostMessage(hwnd, WM_LBUTTONUP, wParamUp, clickPos);
            keystatesend = VK_LEFT;
        }
        if (z == 2) {
            
            unsigned int  wParamDown = VK_RBUTTON | sig;
            unsigned int  wParamUp = 0 | sig;

            PostMessage(hwnd, WM_RBUTTONDOWN, wParamDown, clickPos);
            PostMessage(hwnd, WM_RBUTTONUP, wParamUp, clickPos);
        }
        if (z == 3) {
            unsigned int  wParamDown = VK_LBUTTON | sig;

            unsigned int mouseMkFlags = VK_LBUTTON | sig;
            PostMessage(hwnd, WM_LBUTTONDOWN, mouseMkFlags, clickPos);
            keystatesend = VK_LEFT;
            musLB = true;
        }
        if (z == 4)
        {
            unsigned int wParamUp = 0 | sig;
            PostMessage(hwnd, WM_LBUTTONUP, wParamUp, clickPos);
            musLB = false;

        }
        if (z == 5) {
            unsigned int wParamDown = VK_RBUTTON | sig;

            PostMessage(hwnd, WM_RBUTTONDOWN, wParamDown, clickPos);
            keystatesend = VK_RIGHT;
            musRB = true;
        }
        if (z == 6)
        {
            unsigned int wParamUp = 0 | sig;
            PostMessage(hwnd, WM_RBUTTONUP, wParamUp, clickPos);
            musRB = false;

        }
        if (z == 20 || z == 21) //WM_mousewheel need desktop coordinates
        {

            ClientToScreen(hwnd, &heer);
            LPARAM clickPos = MAKELPARAM(heer.x, heer.y);
            WPARAM wParam = 0;
            if (z == 20) {
                wParam = MAKEWPARAM(0, -120); 
                rawmouseWd = true;
            }
            if (z == 21) {
                wParam = MAKEWPARAM(0, 120);
                rawmouseWu = true;
            }
            wParam |= sig; //
            PostMessage(hwnd, WM_MOUSEWHEEL, wParam, clickPos);
        }
        if (z == 30) //WM_LBUTTONDBLCLK
        {
            unsigned int  wParam = 0 | sig;
            PostMessage(hwnd, WM_LBUTTONDBLCLK, wParam, clickPos);
        }
        else if (z == 8 || z == 10 || z == 11) //only mousemove
        {
            unsigned int  wParam = 0 | sig;
            PostMessage(hwnd, WM_MOUSEMOVE, wParam, clickPos);
            //PostMessage(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));

        }
        return true;
    }
    else //realmouse
    {
        Mutexlock(true);
        ClientToScreen(hwnd, &heer); //need desktop
        double screenWidth = GetSystemMetrics(SM_CXSCREEN) - 1;
        double screenHeight = GetSystemMetrics(SM_CYSCREEN) - 1;
        double fx = x * (65535.0f / screenWidth);
        double fy = y * (65535.0f / screenHeight);

        INPUT input[3] = {};

        // Move the mouse to the specified position
        input[0].type = INPUT_MOUSE;
        input[0].mi.dx = static_cast<LONG>(fx);
        input[0].mi.dy = static_cast<LONG>(fy);
        input[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        if (z == 1) { //left button press
            // Simulate mouse left button down
            input[1].type = INPUT_MOUSE;
            input[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            keystatesend = VK_LEFT;

            // Simulate mouse left button up
            input[2].type = INPUT_MOUSE;
            input[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        }
        else if (z == 2) { //right button press
            // Simulate mouse left button down
            input[1].type = INPUT_MOUSE;
            input[1].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;

            // Simulate mouse left button up
            input[2].type = INPUT_MOUSE;
            input[2].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        }
        else if (z == 3)
        { //right button press, drag and release
            // Simulate mouse left button down
            input[1].type = INPUT_MOUSE;
            input[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            keystatesend = VK_LEFT;
        }
        else if (z == 5)
        { //right button press, drag and release
            // Simulate mouse left button up
            input[1].type = INPUT_MOUSE;
            input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

            input[2].type = INPUT_MOUSE;
            input[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        }
        else if (z == 6 || z == 8 || z == 10 || z == 11) //only mousemove but 6 no mutex. only make mutex while 10 both make and release
        { //right button press, drag and release
            // Simulate mouse left button down
        }
        else if (z == 7)
        { //right button press, drag and release
            // Simulate mouse left button up
            input[1].type = INPUT_MOUSE;
            input[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;

            input[2].type = INPUT_MOUSE;
            input[2].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        }
        else if (z == 4)
        { //right button press, drag and release
            // Simulate mouse left button up
        }
        //z is 3 or anything just move mouse
        SendInput(many, input, sizeof(INPUT));
        Mutexlock(false);
        
    }
    // Convert screen coordinates to absolute values

    return true;
}

std::string UGetExecutableFolder() {
    TCHAR path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    std::string exePath(path);

    // Remove the executable name to get the folder
    size_t lastSlash = exePath.find_last_of("\\/");
    return exePath.substr(0, lastSlash);
}

std::wstring WGetExecutableFolder() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring exePath(path);
    size_t lastSlash = exePath.find_last_of(L"\\/");

    if (lastSlash == std::wstring::npos)
        return L"";
    return exePath.substr(0, lastSlash);
}

bool FindSubImage24(
    const BYTE* largeData, int largeW, int largeH, int strideLarge,
    const BYTE* smallData, int smallW, int smallH, int strideSmall,
    POINT& foundAt, int Xstart, int Ystart
) {
    for (int y = Ystart; y <= largeH - smallH; ++y) {
        for (int x = Xstart; x <= largeW - smallW; ++x) {
            bool match = true;
            for (int j = 0; j < smallH && match; ++j) {
                const BYTE* pLarge = largeData + (y + j) * strideLarge + x * 3;
                const BYTE* pSmall = smallData + j * strideSmall;
                if (memcmp(pLarge, pSmall, smallW * 3) != 0) {
                    match = false;
                }
            }
            if (match) {
                foundAt.x = x;
                foundAt.y = y;
                return true;
            }
        }
    }
    return false;
}
struct HandleData
{
    unsigned long pid;
    HWND hwnd;
};

BOOL IsMainWindow(HWND handle)
{
    // Is top level & visible & not one of ours
    if (GetWindow(handle, GW_OWNER) == nullptr && IsWindowVisible(handle) && handle != pointerWindow)
        return TRUE;
    else return FALSE;
}

BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
{
    HandleData& data = *(HandleData*)lParam;

    DWORD pid = 0;
    GetWindowThreadProcessId(handle, &pid);

    if (data.pid != pid || !IsMainWindow(handle))
        return TRUE; // Keep searching

    data.hwnd = handle;

    return FALSE;
}

void UpdateMainHwnd(bool logOutput)
{
    // Go through all the top level windows, select the first that's visible & belongs to the process

    HandleData data{ GetCurrentProcessId(), nullptr };
    EnumWindows(EnumWindowsCallback, (LPARAM)&data);

    const auto hwndd = (intptr_t)data.hwnd;

    if (logOutput)
    {
        if (hwndd == 0)
			MessageBoxA(NULL, "UpdateMainHwnd did not find a valid hwnd!", "Error", MB_OK | MB_ICONERROR);
    }

    if (data.hwnd != nullptr)
        hwnd = data.hwnd;
}

//HWND GetMainWindowHandle(DWORD targetPID) {
//    HWND hwnd = nullptr;
//    struct HandleData {
//        DWORD pid;
 //       HWND hwnd;
 //   } data = { targetPID, nullptr };
//
 //   auto EnumWindowsCallback = [](HWND hWnd, LPARAM lParam) -> BOOL {
 //       HandleData* pData = reinterpret_cast<HandleData*>(lParam);
 //       DWORD windowPID = 0;
  //      GetWindowThreadProcessId(hWnd, &windowPID);
  //      if (windowPID == pData->pid && GetWindow(hWnd, GW_OWNER) == nullptr && IsWindowVisible(hWnd)) {
        //    POINT reshere = windowpos(hWnd, ignorerect, false); // ignore splash windows
         //   if (reshere.y < 300) 
		//		return TRUE; 
      //      else {
    //            pData->hwnd = hWnd;
    //            return FALSE; // Stop enumeration
        //    }
    //    }
    //    return TRUE; // Continue
    //    };

  //  EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
  //  return data.hwnd;
//}

int CalculateStride(int width) {
    return ((width * 3 + 3) & ~3);
}


bool Save24BitBMP(std::wstring filename, const BYTE* pixels, int width, int height) { //for testing purposes
    int stride = ((width * 3 + 3) & ~3);
    int imageSize = stride * height;

    BITMAPFILEHEADER bfh = {};
    bfh.bfType = 0x4D42;  // "BM"
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + imageSize;

    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = -height;  // bottom-up BMP (positive height)
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = imageSize;

    HANDLE hFile = CreateFileW(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written;
    WriteFile(hFile, &bfh, sizeof(bfh), &written, NULL);
    WriteFile(hFile, &bih, sizeof(bih), &written, NULL);
    WriteFile(hFile, pixels, imageSize, &written, NULL);
    CloseHandle(hFile);

    return true;
}

bool IsTriggerPressed(BYTE triggerValue) {
    BYTE threshold = 175;
    return triggerValue > threshold;
}
bool LoadBMP24Bit(std::wstring filename, std::vector<BYTE>& pixels, int& width, int& height, int& stride) {
    HBITMAP hbm = (HBITMAP)LoadImageW(NULL, filename.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!hbm) return false;

    //BITMAP scaledbmp;
    BITMAP bmp;
    GetObject(hbm, sizeof(BITMAP), &bmp);
    width = bmp.bmWidth - 1;
    height = bmp.bmHeight - 1;
    stride = CalculateStride(width);

    pixels.resize(stride * height);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* pBits = nullptr;
    HDC hdc = GetDC(NULL);
    GetDIBits(hdc, hbm, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);

    if (hdc) DeleteDC(hdc);
    if (hbm) DeleteObject(hbm);
    return true;
}

bool SaveWindow10x10BMP(HWND hwnd, std::wstring filename, int x, int y) {
    HDC hdcWindow = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcWindow);
    
    // Size: 10x10
    int width = 10;
    int height = 10;
    int stride = ((width * 3 + 3) & ~3);
    std::vector<BYTE> pixels(stride * height);

    // Create a 24bpp bitmap
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;


    //stretchblt

    BYTE* pBits = nullptr;

    HBITMAP hbm24 = CreateDIBSection(hdcWindow, &bmi, DIB_RGB_COLORS, (void**)&pBits, 0, 0);
    if (!hbm24) {
        DeleteDC(hdcMem);
        ReleaseDC(hwnd, hdcWindow);
        return false;
    }

    HGDIOBJ oldbmp = SelectObject(hdcMem, hbm24);

    BitBlt(hdcMem, 0, 0, width, height, hdcWindow, x, y, SRCCOPY);

    // Prepare to retrieve bits
    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = -height; // top-down for easier use
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    GetDIBits(hdcMem, hbm24, 0, height, pixels.data(), (BITMAPINFO*)&bih, DIB_RGB_COLORS);

    // Save
    bool ok = Save24BitBMP(filename.c_str(), pixels.data(), width, height);

    // Cleanup
    SelectObject(hdcMem, oldbmp);
    if (hbm24) DeleteObject(hbm24);
    if (hdcMem)DeleteDC(hdcMem);
    if (hdcWindow) ReleaseDC(hwnd, hdcWindow);

    return ok;
}
HBRUSH transparencyBrush;

void DrawRedX(HDC hdc, int x, int y) //blue
{
    HPEN hPen = CreatePen(PS_SOLID, 3, RGB(0, 0, 255));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    MoveToEx(hdc, x - 15, y - 15, NULL);
    LineTo(hdc, x + 15, y + 15);

    MoveToEx(hdc, x + 15, y - 15, NULL);
    LineTo(hdc, x - 15, y + 15);

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    return;
}
void DrawBlueCircle(HDC hdc, int x, int y) //red
{
    // Create a NULL brush (hollow fill)
    HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

    HPEN hPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    Ellipse(hdc, x - 15, y - 15, x + 15, y + 15);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}
void DrawGreenTriangle(HDC hdc, int x, int y)
{
    // Use a NULL brush for hollow
    HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

    HPEN hPen = CreatePen(PS_SOLID, 3, RGB(0, 255, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    POINT pts[3];
    pts[0].x = x; pts[0].y = y - 10;        // top center
    pts[1].x = x - 10; pts[1].y = y + 10;   // bottom left
    pts[2].x = x + 10; pts[2].y = y + 10;   // bottom right

    Polygon(hdc, pts, 3);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

void DrawPinkSquare(HDC hdc, int x, int y)
{
    // Create a NULL brush (hollow fill)
    HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

    HPEN hPen = CreatePen(PS_SOLID, 3, RGB(255, 192, 203));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    // Draw hollow rectangle (square) 20x20
    Rectangle(hdc, x - 15, y - 15, x + 15, y + 15);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

int moveH = 0;
int moveV = 0; //intro mainly to visualize cursor window size
bool otherway = false;
void DrawToHDC(HDC hdcWindow, int X, int Y, int showmessage)
{


    //RECT fill{ WoldX, WoldY, WoldX + cursorWidth, WoldY + cursorHeight };
    if (drawfakecursor == 2 || drawfakecursor == 3) {
        if (scanoption || drawfakecursor == 3)
        {
            //erase
            RECT rect;
            GetClientRect(pointerWindow, &rect);   // client coordinates
            FillRect(hdcWindow, &rect, transparencyBrush);

        }
        else{
            RECT rect = { WoldX, WoldY, WoldX + 32, WoldY + 32 }; //need bmp width height
            WoldX = X - Xoffset;
            WoldY = Y - Yoffset;
            FillRect(hdcWindow, &rect, transparencyBrush);
        }
    }


    if (scanoption == 1)
    {
        EnterCriticalSection(&critical);
        POINT pos = { fakecursorW.x, fakecursorW.y };
        POINT Apos = { PointA.x, PointA.y };
        POINT Bpos = { PointB.x, PointB.y };
        POINT Xpos = { PointX.x, PointX.y };
        POINT Ypos = { PointY.x, PointY.y };
        //hCursorW = hCursor;
        LeaveCriticalSection(&critical);

        //draw spots
        if (Apos.x != 0 && Apos.y != 0)
            DrawRedX(hdcWindow, Apos.x, Apos.y);

        if (Bpos.x != 0 && Bpos.y != 0)
            DrawBlueCircle(hdcWindow, Bpos.x, Bpos.y);

        if (Xpos.x != 0 && Xpos.y != 0)
            DrawGreenTriangle(hdcWindow, Xpos.x, Xpos.y);

        if (Ypos.x != 0 && Ypos.y != 0)
            DrawPinkSquare(hdcWindow, Ypos.x, Ypos.y);

    }
    if (showmessage == 99) //intro
    {
        RECT rect;
        GetClientRect(pointerWindow, &rect);   // client coordinates
        FillRect(hdcWindow, &rect, transparencyBrush);

        TextOut(hdcWindow, rect.right / 2, rect.bottom / 2, TEXT("LOADING..."), 10);

        DrawPinkSquare(hdcWindow, 20 + moveV, 20 + moveV);
        DrawGreenTriangle(hdcWindow, rect.right - moveV - 20, rect.bottom - moveV - 20);
        DrawBlueCircle(hdcWindow, rect.right - moveV - 20, 20 + moveV);
        DrawRedX(hdcWindow, 20 + moveV, rect.bottom - moveV - 20);

        for (int y = 0; y < 20; y++)
        {
            for (int x = 0; x < 20; x++)
            {
                int val = colorfulSword[y][x];
                if (val != 0)
                {
                    HBRUSH hBrush = CreateSolidBrush(colors[val]);
                    RECT rect = { X + x , Y + y , X + x + 1, Y + y + 1 };
                    FillRect(hdcWindow, &rect, hBrush);
                    DeleteObject(hBrush);
                }
            }
        }

        if (moveV < rect.bottom - 20 && moveV < rect.right - 20 && moveV >= 0)
        { 
            if (!otherway)
                moveV += 10;
            else moveV -= 10;
        }
        
        else if (!otherway) 
        {
            moveV -= 10;
            otherway = true;
        }
        else if (otherway)
        {
            moveV += 10;
            otherway = false;
        }
        //showmessage will auto expire and go to 0 in mainthread
	}
    if (showmessage == 1)
    {
        TextOut(hdcWindow, X, Y, TEXT("BMP MODE"), 8);
        TextOut(hdcWindow, X, Y + 17, TEXT("only mapping searches"), 21);
    }
    else if (showmessage == 2)
    {
        TextOut(hdcWindow, X, Y, TEXT("CURSOR MODE"), 11);
        TextOut(hdcWindow, X, Y + 17, TEXT("mapping searches + cursor"), 25);
    }
    else if (showmessage == 3)
    {
        TextOut(hdcWindow, X, Y, TEXT("EDIT MODE"), 9);
        TextOut(hdcWindow, X, Y + 15, TEXT("tap a button to bind it to coordinate"), 37);
        TextOut(hdcWindow, X, Y + 30, TEXT("A,B,X,Y,R2,R3,L2,L3 can be mapped"), 32);
    }
    else if (showmessage == 4)
    {
        TextOut(hdcWindow, 20, 5, TEXT("Z0 no scale, Z1 stretch, Z2 4:3 stretch, Z3 16:9"), 49);

        TCHAR buf[32];
        wsprintf(buf, TEXT("Z0: X: %d Y: %d"), X, Y);
        TextOut(hdcWindow, 20, 20, TEXT(buf), lstrlen(buf));


		POINT staticpoint = GetStaticFactor({ X, Y }, 1, true);
        TextOut(hdcWindow, staticpoint.x, staticpoint.y, TEXT("1"), 1);

        staticpoint = GetStaticFactor({ X, Y }, 2, true);
        TextOut(hdcWindow, staticpoint.x, staticpoint.y, TEXT("2"), 1);

        staticpoint = GetStaticFactor({ X, Y }, 3, true);
        TextOut(hdcWindow, staticpoint.x, staticpoint.y, TEXT("3"), 1);

        for (int y = 0; y < 20; y++)
        {
            for (int x = 0; x < 20; x++)
            {
                int val = colorfulSword[y][x];
                if (val != 0)
                {
                    HBRUSH hBrush = CreateSolidBrush(colors[val]);
                    RECT rect = { X + x , Y + y , X + x + 1, Y + y + 1 };
                    FillRect(hdcWindow, &rect, hBrush);
                    DeleteObject(hBrush);
                }
            }
        }
    }
    else if (showmessage == 10)
    {
        TextOut(hdcWindow, X, Y, TEXT("BUTTON MAPPED"), 13);
    }
    else if (showmessage == 11)
    {
        TextOut(hdcWindow, X, Y, TEXT("WAIT FOR MESSAGE EXPIRE!"), 24);
    }
    else if (showmessage == 12)
    {
        TextOut(hdcWindow, 20, 20, TEXT("DISCONNECTED!"), 14); //14
    }
    else if (showmessage == 69)
    {
        TextOut(hdcWindow, X, Y, TEXT("SHUTTING DOWN"), 13);
    }
    else if (showmessage == 70)
    {
        TextOut(hdcWindow, X, Y, TEXT("STARTING!"), 10);
    }
    else if (showmessage == 71)
    {
        TextOut(hdcWindow, X, Y, TEXT("WINDOW?"), 7);
    }
    if (nodrawcursor == false)
    { 
        if (hCursor != 0 && onoroff == true)
        {
            gotcursoryet = true;
            if (X - Xoffset < 0 || Y - Yoffset < 0)
                DrawIconEx(hdcWindow, X, Y, hCursor, 32, 32, 0, NULL, DI_NORMAL);//need bmp width height
            else
                DrawIconEx(hdcWindow, X - Xoffset, Y - Yoffset, hCursor, 32, 32, 0, NULL, DI_NORMAL);//need bmp width height

        }
        else if (onoroff == true && (alwaysdrawcursor == 1 || gotcursoryet == false))
        {
            for (int y = 0; y < 20; y++)
            {
                for (int x = 0; x < 20; x++)
                {
                    int val = colorfulSword[y][x];
                    if (val != 0)
                    {
                        HBRUSH hBrush = CreateSolidBrush(colors[val]);
                        RECT rect = { X + x , Y + y , X + x + 1, Y + y + 1 };
                        FillRect(hdcWindow, &rect, hBrush);
                        DeleteObject(hBrush);
                    }
                }
            }
        }
    }
    return;
}




void DblBufferAndCallDraw(HDC cursorhdc, int X, int Y, int showmessage) {

    POINT res = windowpos(hwnd, ignorerect, false);
    int width = res.x;
    int height = res.y;

    HDC hdcMem = CreateCompatibleDC(cursorhdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(cursorhdc, width, height);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hbmMem);

    //BitBlt(hdcMem, 0, 0, width, height, cursorhdc, 0, 0, SRCCOPY);
   // SetBkMode(hdcMem, TRANSPARENT);

    DrawToHDC(hdcMem, X, Y, showmessage);

    BitBlt(cursorhdc, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);

    // cleanup
    SelectObject(hdcMem, oldBmp);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}
void GetGameHDCAndCallDraw(HWND hwnd){
    if (scanoption)
        EnterCriticalSection(&critical);
    HDC hdc = GetDC(hwnd);
    if (hdc)
    {
        DrawToHDC(hdc, Xf, Yf, showmessage);
        ReleaseDC(hwnd, hdc);
    }
    
    if (scanoption)
        LeaveCriticalSection(&critical);
    return;
  }

bool CaptureWindow24Bit(HWND hwnd, SIZE& capturedwindow, std::vector<BYTE>& pixels, int& strideOut, bool draw, bool stretchblt) 
{
    if (scanoption && drawfakecursor == 1)
        EnterCriticalSection(&critical);
    HDC hdcWindow = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcWindow);


    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int width = rcClient.right - rcClient.left;
    int height = rcClient.bottom - rcClient.top;
    capturedwindow.cx = width;
    capturedwindow.cy = height;

    int stride = ((width * 3 + 3) & ~3);
    strideOut = stride;
    pixels.resize(stride * height);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // top-down
    bmi.bmiHeader.biPlanes = 1; 
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* pBits = nullptr;
    HBITMAP hbm24  = CreateDIBSection(hdcWindow, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
    if (hbm24)   
    {
        HGDIOBJ oldBmp = SelectObject(hdcMem, hbm24);
        BitBlt(hdcMem, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);
        GetDIBits(hdcMem, hbm24, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);
        SelectObject(hdcMem, oldBmp);
        DeleteObject(hbm24);
       // hbm24 = nullptr;

    } //hbm24 not null

    if (hdcMem) DeleteDC(hdcMem);
    if (hdcWindow) ReleaseDC(hwnd, hdcWindow);

    if (scanoption && drawfakecursor == 1)
        LeaveCriticalSection(&critical);
    return true;
} //function end
// Helper: Get stick magnitude
float GetStickMagnitude(SHORT x, SHORT y) {
    return sqrtf(static_cast<float>(x) * x + static_cast<float>(y) * y);
}

// Helper: Clamp value to range [-1, 1]
float Clamp(float v) {
    if (v < -1.0f) return -1.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}
// #define DEADZONE 8000
// #define MAX_SPEED 30.0f        // Maximum pixels per poll
// #define ACCELERATION 2.0f      // Controls non-linear ramp (higher = more acceleration)
float radial_deadzone = 0.10f; // Circular/Radial Deadzone (0.0 to 0.3)
float axial_deadzone = 0.00f; // Square/Axial Deadzone (0.0 to 0.3)
const float max_threshold = 0.03f; // Max Input Threshold, an "outer deadzone" (0.0 to 0.15)
const float curve_slope = 0.16f; // The linear portion of the response curve (0.0 to 1.0)
const float curve_exponent = 5.00f; // The exponential portion of the curve (1.0 to 10.0)
float sensitivity = 20.00f; // Base sensitivity / max speed (1.0 to 30.0)
float accel_multiplier = 1.90f; // Look Acceleration Multiplier (1.0 to 3.0)

POINT CalculateUltimateCursorMove(
    SHORT stickX, SHORT stickY,
    float c_deadzone,
    float s_deadzone,
    float max_threshold,
    float curve_slope,
    float curve_exponent,
    float sensitivity,
    float accel_multiplier
) {
    static double mouseDeltaAccumulatorX = 0.0;
    static double mouseDeltaAccumulatorY = 0.0;

    double normX = static_cast<double>(stickX) / 32767.0;
    double normY = static_cast<double>(stickY) / 32767.0;

    double magnitude = std::sqrt(normX * normX + normY * normY);
    if (magnitude < c_deadzone) {
        return { 0, 0 }; // Inside circular deadzone
    }
    if (std::abs(normX) < s_deadzone) {
        normX = 0.0; // Inside axial deadzone for X
    }
    if (std::abs(normY) < s_deadzone) {
        normY = 0.0; // Inside axial deadzone for Y
    }
    magnitude = std::sqrt(normX * normX + normY * normY);
    if (magnitude < 1e-6) {
        return { 0, 0 };
    }

    double effectiveRange = 1.0 - max_threshold - c_deadzone;
    if (effectiveRange < 1e-6) effectiveRange = 1.0;

    double remappedMagnitude = (magnitude - c_deadzone) / effectiveRange;
    remappedMagnitude = (std::max)(0.0, (std::min)(1.0, remappedMagnitude));

    double curvedMagnitude = curve_slope * remappedMagnitude + (1.0 - curve_slope) * std::pow(remappedMagnitude, curve_exponent);

    double finalSpeed = sensitivity * accel_multiplier;

    double dirX = normX / magnitude;
    double dirY = normY / magnitude;
    double finalMouseDeltaX = dirX * curvedMagnitude * finalSpeed;
    double finalMouseDeltaY = dirY * curvedMagnitude * finalSpeed;

    mouseDeltaAccumulatorX += finalMouseDeltaX;
    mouseDeltaAccumulatorY += finalMouseDeltaY;
    LONG integerDeltaX = static_cast<LONG>(mouseDeltaAccumulatorX);
    LONG integerDeltaY = static_cast<LONG>(mouseDeltaAccumulatorY);

    mouseDeltaAccumulatorX -= integerDeltaX;
    mouseDeltaAccumulatorY -= integerDeltaY;

    return { integerDeltaX, -integerDeltaY };
}

void PostKeyFunction(HWND hwnd, int keytype, bool press) {
    DWORD mykey = 0;
	DWORD presskey = WM_KEYDOWN;


    
    if (!press){
		presskey = WM_KEYUP; // Key up event 
    }

    //standard keys for dpad
    if (keytype == -1)
        mykey = VK_UP;
    if (keytype == -2)
        mykey = VK_DOWN;
    if (keytype == -3)
        mykey = VK_LEFT;
    if (keytype == -4)
        mykey = VK_RIGHT;

    if (keytype == 3)
        mykey = VK_ESCAPE;
    if (keytype == 4)
        mykey = VK_RETURN;
    if (keytype == 5)
        mykey = VK_TAB;
    if (keytype == 6)
        mykey = VK_SHIFT;
    if (keytype == 7)
        mykey = VK_CONTROL;
    if (keytype == 8)
        mykey = VK_SPACE;

    if (keytype == 9)
        mykey = 0x4D; //M

    if (keytype == 10)
        mykey = 0x57; //W

    if (keytype == 11)
        mykey = 0x53; //S

    if (keytype == 12)
        mykey = 0x41; //A

    if (keytype == 13)
        mykey = 0x44; //D

    if (keytype == 14)
        mykey = 0x45; //E

    if (keytype == 15)
        mykey = 0x46; //F

    if (keytype == 16)
        mykey = 0x47; //G

    if (keytype == 17)
        mykey = 0x48; //H

    if (keytype == 18)
        mykey = 0x49; //I

    if (keytype == 19)
        mykey = 0x51; //Q

    if (keytype == 20)
        mykey = VK_OEM_PERIOD;

    if (keytype == 21)
        mykey = 0x52; //R

    if (keytype == 22)
        mykey = 0x54; //T

    if (keytype == 23)
        mykey = 0x42; //B

    if (keytype == 24)
        mykey = 0x43; //C

    if (keytype == 25)
        mykey = 0x4B; //K

    if (keytype == 26)
        mykey = 0x55; //U

    if (keytype == 27)
        mykey = 0x56; //V

    if (keytype == 28)
        mykey = 0x57; //W

    if (keytype == 30)
        mykey = 0x30; //0

    if (keytype == 31)
        mykey = 0x31; //1

    if (keytype == 32)
        mykey = 0x32; //2

    if (keytype == 33)
        mykey = 0x33; //3

    if (keytype == 34)
        mykey = 0x34; //4

    if (keytype == 35)
        mykey = 0x35; //5

    if (keytype == 36)
        mykey = 0x36; //6

    if (keytype == 37)
        mykey = 0x37; //7

    if (keytype == 38)
        mykey = 0x38; //8

    if (keytype == 39)
        mykey = 0x39; //9

    if (keytype == 40)
        mykey = VK_UP; 

    if (keytype == 41)
        mykey = VK_DOWN; 

    if (keytype == 42)
        mykey = VK_LEFT; 

    if (keytype == 43)
        mykey = VK_RIGHT; 

    if (keytype == 44)
        mykey = 0x58; //X

    if (keytype == 45)
        mykey = 0x5A; //Z

    if (keytype == 46)
        mykey = 0x4C; //L



    if (keytype == 51)
        mykey = VK_F1;

    if (keytype == 52)
        mykey = VK_F2;

    if (keytype == 53)
        mykey = VK_F3;

    if (keytype == 54)
        mykey = VK_F4;

    if (keytype == 55)
        mykey = VK_F5;

    if (keytype == 56)
        mykey = VK_F6;

    if (keytype == 57)
        mykey = VK_F7;

    if (keytype == 58)
        mykey = VK_F8;
    if (keytype == 59)
        mykey = VK_F9;

    if (keytype == 60)
        mykey = VK_F10;

    if (keytype == 61)
        mykey = VK_F11;

    if (keytype == 62)
        mykey = VK_F12;

    if (keytype == 63){ //control+C
        mykey = VK_CONTROL;
    }


    if (keytype == 70)
        mykey = VK_NUMPAD0;

    if (keytype == 71)
        mykey = VK_NUMPAD1;

    if (keytype == 72)
        mykey = VK_NUMPAD2;

    if (keytype == 73)
        mykey = VK_NUMPAD3;

    if (keytype == 74)
        mykey = VK_NUMPAD4;

    if (keytype == 75)
        mykey = VK_NUMPAD5;

    if (keytype == 76)
        mykey = VK_NUMPAD6;

    if (keytype == 77)
        mykey = VK_NUMPAD7;

    if (keytype == 78)
        mykey = VK_NUMPAD8;

    if (keytype == 79)
        mykey = VK_NUMPAD9;

    if (keytype == 80)
        mykey = VK_SUBTRACT;

    if (keytype == 81)
        mykey = VK_ADD;

    UINT scanCode = MapVirtualKey(mykey, MAPVK_VK_TO_VSC);
    unsigned int lParam;

    if (press)
    { 

        lParam = 1;                  // repeat count 1 or 0?
        lParam |= (scanCode << 16);         // scan code
        lParam |= (0 << 30);                // previous state
        lParam |= (0 << 31);                // transition state (0 = key down)
    }
    else
    {
        BOOL isExtended = (mykey == VK_LEFT || mykey == VK_RIGHT ||
            mykey == VK_UP || mykey == VK_DOWN ||
            mykey == VK_INSERT || mykey == VK_DELETE ||
            mykey == VK_HOME || mykey == VK_END ||
            mykey == VK_PRIOR || mykey == VK_NEXT ||
            mykey == VK_RCONTROL || mykey == VK_RMENU);

        unsigned int lparam = 0;
        lparam |= 1; // Repeat count (always 1 for key up)
       // lparam |= (mykey.MakeCode << 16); // Scan code
        lparam |= (1 << 30); // Previous key state (always 1 for key up)
        lparam |= (1 << 31); // Transition state (always 1 for key up)

        keystatesend = 0; 
    }
    
    LPARAM ScreenshotInput_KEYBOARD_SIGNATUR = 0x10000000;
    PostMessage(hwnd, presskey, ScreenshotInput_KEYBOARD_SIGNATUR |= mykey, lParam);
 //   for (HWND ahwnd : g_windows)
	//  { //multiwindow support
  //      if (IsWindow(ahwnd) && hwnd != hwnd)
   //     {
  //          PostMessage(ahwnd, presskey, mykey, lParam);
   //     }
  //  }
   // PostMessage(hwnd, WM_INPUT, VK_RIGHT, lParam);
    if (rawinputhook == 1 || rawinputhook == 2)
        GenerateRawKey(mykey, press);
    if (keytype == 63) {
        PostMessage(hwnd, presskey, ScreenshotInput_KEYBOARD_SIGNATUR |= 0x43, lParam); //game specific key combo. Warlords Battlecry 3 convert button
    }
    return;

}

void BmpInputAction(int X, int Y, int type) //moveclickorboth
{

    if (type == 0) //click and move
    {
        Xf = X;
        Yf = Y;
        SendMouseClick(X, Y, 8, 1);
        Sleep(5);
        SendMouseClick(X, Y, 3, 2);
        Sleep(5);
        SendMouseClick(X, Y, 4, 2);
        movedmouse = true;
    }
    else if (type == 1) //only move
    {
        Xf = X;
        Yf = Y;
        
        SendMouseClick(X, Y, 8, 1);
        movedmouse = true;
    }
    else if (type == 2) //only click
    {
        SendMouseClick(X, Y, 8, 1);
        Sleep(2);
        SendMouseClick(X, Y, 3, 2);
        Sleep(2);
        SendMouseClick(X, Y, 4, 2);
        Sleep(2);
        SendMouseClick(Xf, Yf, 8, 1);
    }
}
void Bmpfound(const char key[3], int X, int Y, int i, bool onlysearch, bool found, int store)
{
    int input = 0;
    if (strcmp(key, "\\A") == 0)
    {
        if (found)
        {
           // EnterCriticalSection(&critical);

           // LeaveCriticalSection(&critical);
            if (onlysearch)
            {
                EnterCriticalSection(&critical);
                startsearchA = i;
                input = scanAtype;
                staticPointA[i].x = X;
                staticPointA[i].y = Y;
                PointA.x = X;
                PointA.y = Y;
                LeaveCriticalSection(&critical);
            }
            else
            {
                input = scanAtype;
                if (store) {
                    staticPointA[i].x = X;
                    staticPointA[i].y = Y;

                }
                if (startsearchA < numphotoA - 1)
                    startsearchA = i + 1;
                else startsearchA = 0;
            }
        }
        else
        {
            if (onlysearch)
            {
                EnterCriticalSection(&critical);
                startsearchA = 0;
                PointA.x = 0;
                PointA.y = 0;
                LeaveCriticalSection(&critical);
            }
        }
    }
    if (strcmp(key, "\\B") == 0)
    {
        if (found)
        {
            //EnterCriticalSection(&critical);

            //LeaveCriticalSection(&critical);
            if (onlysearch)
            {
                EnterCriticalSection(&critical);
                startsearchB = i;
                PointB.x = X;
                staticPointB[i].x = X;
                staticPointB[i].y = Y;
                PointB.y = Y;
                LeaveCriticalSection(&critical);
            }
            else
            {
                input = scanBtype;
                if (store) {
                    staticPointB[i].x = X;
                    staticPointB[i].y = Y;
                }
                if (startsearchB < numphotoB - 1)
                    startsearchB = i + 1;
                else startsearchB = 0;
            }
        }
        else
        {
            if (onlysearch)
            {
                EnterCriticalSection(&critical);
                startsearchB = 0;
                PointB.x = 0;
                PointB.y = 0;
                LeaveCriticalSection(&critical);
            }
        }
    }
    if (strcmp(key, "\\X") == 0)
    {
        if (found)
        {
           // EnterCriticalSection(&critical);

            //LeaveCriticalSection(&critical);
            if (onlysearch)
            {
                EnterCriticalSection(&critical);
                startsearchX = i;
                PointX.x = X;
                PointX.y = Y;
                staticPointX[i].x = X;
                staticPointX[i].y = Y;
                LeaveCriticalSection(&critical);
            }
            else
            {
                input = scanXtype;
                startsearchX = i + 1;
                if (store) {
                    staticPointX[i].x = X;
                    staticPointX[i].y = Y;
                }
                if (startsearchX < numphotoX - 1)
                    startsearchX = i + 1;
                else startsearchX = 0;
            }
        }
        else
        {
            if (onlysearch)
            {
                EnterCriticalSection(&critical);
                startsearchX = 0;
                PointX.x = 0;
                PointX.y = 0;
                LeaveCriticalSection(&critical);
            }
        }
    }
    if (strcmp(key, "\\Y") == 0)
    {
        //EnterCriticalSection(&critical);

        //LeaveCriticalSection(&critical);
        if (found)
        {
            if (onlysearch)
            {
                EnterCriticalSection(&critical);
                startsearchX = i;
                staticPointY[i].x = X;
                staticPointY[i].y = Y;
                PointY.x = X;
                PointY.y = Y;
                LeaveCriticalSection(&critical);
            }
            else
            {
                input = scanYtype;
                startsearchY = i + 1;
                if (store) {
                    staticPointY[i].x = X;
                    staticPointY[i].y = Y;
                }
                if (startsearchY < numphotoY - 1)
                    startsearchY = i + 1;
                else startsearchY = 0;
            }
        }
        else
        {
            if (onlysearch)
            {
                EnterCriticalSection(&critical);
                startsearchY = 0;
                //input = scanYtype;
                PointY.x = 0;
                PointY.y = 0;
                LeaveCriticalSection(&critical);
            }
        }
    }
    if (strcmp(key, "\\C") == 0)
    {
        if (found && !onlysearch)
        {
            startsearchC = i + 1;
            input = Ctype;
        }
    }
    if (strcmp(key, "\\D") == 0)
    {
        if (found && !onlysearch)
        {
            startsearchD = i + 1;
            input = Dtype;
        }
    }
    if (strcmp(key, "\\E") == 0)
    {
        if (found && !onlysearch)
        {
            startsearchE = i + 1;
            input = Etype;
        }
    }
    if (strcmp(key, "\\F") == 0)
    {
        if (found && !onlysearch)
        {
            startsearchF = i + 1;
            input = Ftype;
        }
    }
    if (!onlysearch)
    {
        if (found)
        {	//input sent in this function
            BmpInputAction(X, Y, input);
        }
    }
    return;
}

POINT CheckStatics(const char abc[3], int numtocheck)
{
    POINT newpoint{0,0};
    if (strcmp(abc, "\\A") == 0)
    {
        if (staticPointA[numtocheck].x != 0)
        {
           // 
            newpoint.x = staticPointA[numtocheck].x;
            newpoint.y = staticPointA[numtocheck].y;
        }
    }
    if (strcmp(abc, "\\B") == 0)
    {
        if (staticPointB[numtocheck].x != 0)
        {
            newpoint.x = staticPointB[numtocheck].x;
            newpoint.y = staticPointB[numtocheck].y;
        }
    }
    if (strcmp(abc, "\\X") == 0)
    {
        if (staticPointX[numtocheck].x != 0)
        {
            newpoint.x = staticPointX[numtocheck].x;
            newpoint.y = staticPointX[numtocheck].y;
        }
    }
    if (strcmp(abc, "\\Y") == 0)
    {
        if (staticPointY[numtocheck].x != 0)
        {
            newpoint.x = staticPointY[numtocheck].x;
            newpoint.y = staticPointY[numtocheck].y;
        }
    }
    return newpoint;
}
bool ButtonScanAction(const char key[3], int mode, int serchnum, int startsearch, bool onlysearch, POINT currentpoint, int checkarray)
{
    //HBITMAP hbbmdsktop;
    //criticals
    //EnterCriticalSection(&critical);
    bool found = false;
    //LeaveCriticalSection(&critical);
    //mode // onlysearch
    POINT pt = { 0,0 };
    POINT noeder = { 0,0 };
    //MessageBox(NULL, "some kind of error", "startsearch searchnum error", MB_OK | MB_ICONINFORMATION);
    int numbmp = 0;
    if (mode != 2)
    {
        int numphoto = 0;
        //if (checkarray == 1)
		//{ //always check static first?
            noeder = CheckStatics(key, startsearch);
            if (noeder.x != 0)
            {
                Bmpfound(key, noeder.x, noeder.y, startsearch, onlysearch, true, checkarray); //or not found
                found = true;
                return true;
            }
        //}
        if (!found)
        {
            for (int i = startsearch; i < serchnum; i++) //memory problem here somewhere
            {
                if (checkarray == 1)
                {
                    noeder = CheckStatics(key, i);
                    if (noeder.x != 0)
                    {
                        Bmpfound(key, noeder.x, noeder.y, i, onlysearch, true, checkarray); //or not found
                        found = true;
                        return true;
                    }
                }
                // if (checkarray)

                // MessageBox(NULL, "some kind of error", "startsearch searchnum error", MB_OK | MB_ICONINFORMATION);
                std::string path = UGetExecutableFolder() + key + std::to_string(i) + ".bmp";
                std::wstring wpath(path.begin(), path.end());
                // HDC soke;
                if (LoadBMP24Bit(wpath.c_str(), smallPixels, smallW, smallH, strideSmall) && !found)
                {
                    // MessageBox(NULL, "some kind of error", "loaded bmp", MB_OK | MB_ICONINFORMATION);
                     // Capture screen
                   // HBITMAP hbbmdsktop;
                    if (CaptureWindow24Bit(hwnd, screenSize, largePixels, strideLarge, false, resize))
                    {
                        if (onlysearch)
                        {
                            if (FindSubImage24(largePixels.data(), screenSize.cx, screenSize.cy, strideLarge, smallPixels.data(), smallW, smallH, strideSmall, pt, currentpoint.x, currentpoint.y))
                            {
                                numphoto = i;
                                found = true;
                                break;
                            }
                        }
                        if (found == false)
                        {
                            if (FindSubImage24(largePixels.data(), screenSize.cx, screenSize.cy, strideLarge, smallPixels.data(), smallW, smallH, strideSmall, pt, 0, 0))
                            {
                                // MessageBox(NULL, "some kind of error", "found spot", MB_OK | MB_ICONINFORMATION);
                                found = true;
                                numphoto = i;
                                break;
                            }
                        }//found
                    } //hbmdessktop
                }//loadbmp
            }
        }
        if (!found)
        {
            for (int i = 0; i < serchnum; i++) //memory problem here somewhere
            {
                if (checkarray == 1)
                {
                    noeder = CheckStatics(key, i);
                    if (noeder.x != 0)
                    {
                        //MessageBox(NULL, "some kind of error", "startsearch searchnum error", MB_OK | MB_ICONINFORMATION);
                        Bmpfound(key, noeder.x, noeder.y, i, onlysearch, true, checkarray); //or not found
                        found = true;
                        return true;
                    }
                }
                
                std::string path = UGetExecutableFolder() + key + std::to_string(i) + ".bmp";
                std::wstring wpath(path.begin(), path.end());
                if (LoadBMP24Bit(wpath.c_str(), smallPixels, smallW, smallH, strideSmall) && !found)
                {
                    //ShowMemoryUsageMessageBox();
                    if (CaptureWindow24Bit(hwnd, screenSize, largePixels, strideLarge, false, resize))
                        
                    {// MessageBox(NULL, "some kind of error", "captured desktop", MB_OK | MB_ICONINFORMATION);
                        //ShowMemoryUsageMessageBox();
                        if (FindSubImage24(largePixels.data(), screenSize.cx, screenSize.cy, strideLarge, smallPixels.data(), smallW, smallH, strideSmall, pt, 0, 0))
                            {
                                // MessageBox(NULL, "some kind of error", "found spot", MB_OK | MB_ICONINFORMATION);
                                found = true;
                                numphoto = i;
                                break;
                            }

                    } //hbmdessktop
                }//loadbmp
            }

        }
        EnterCriticalSection(&critical);
        bool foundit = found;
        LeaveCriticalSection(&critical);
        Bmpfound(key, pt.x, pt.y, numphoto, onlysearch, found, checkarray); //or not found
        if (found == true)
            return true;
        else return false;
    }

    if (mode == 2 && showmessage == 0 && onlysearch == false) //mode 2 button mapping //showmessage var to make sure no double map or map while message
    {
		//RepaintWindow(hwnd, NULL, FALSE); 
        Sleep(100); //to make sure red flicker expired
        std::string path = UGetExecutableFolder() + key + std::to_string(serchnum) + ".bmp";
        std::wstring wpath(path.begin(), path.end());
        SaveWindow10x10BMP(hwnd, wpath.c_str(), Xf, Yf);
       // MessageBox(NULL, "Mapped spot!", key, MB_OK | MB_ICONINFORMATION);
        showmessage = 10;
        return true;
    }
    else if (mode == 2 && onlysearch == false)
        showmessage = 11;//wait for mesage expire
        return true;
}
//int akkumulator = 0;    

#define WM_MOVE_pointerWindow (WM_APP + 1)

void GetWindowDimensions(HWND pointerWindow)
{
    HWND tHwnd = hwnd;
    if (pointerWindow == tHwnd)
        return;

    if (IsWindow(tHwnd))
    {
        POINT pos = windowpos(hwnd, ignorerect, true);
        POINT res = windowpos(hwnd, ignorerect, false);

        SetWindowPos(pointerWindow, HWND_TOPMOST,
            pos.x,
            pos.y,
            res.x,
            res.y,
            SWP_NOACTIVATE);
        ShowWindow(pointerWindow, SW_SHOW);
    }
}
// Window procedure
bool neederase = false;
LRESULT CALLBACK FakeCursorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_MOVE_pointerWindow:
    {
        GetWindowDimensions(hWnd);
        break;
    }
    case WM_PAINT:
    {
        return 1;
        break;
    }
    case WM_ERASEBKGND:
    {
        return 1;
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    default:
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool scanloop = true;
DWORD WINAPI ScanThread(LPVOID, int Aisstatic, int Bisstatic, int Xisstatic, int Yisstatic) 
{
    int scantick = 0;
    Sleep(3000);
    int Astatic = Aisstatic;
    int Bstatic = Bisstatic;
    int Xstatic = Xisstatic;
    int Ystatic = Yisstatic;
    scanrunning = true;
    while (scanloop)
    { 
        EnterCriticalSection(&critical);
        POINT Apos = { PointA.x, PointA.y };
        POINT Bpos = { PointB.x, PointB.y };
        POINT Xpos = { PointX.x, PointX.y };
        POINT Ypos = { PointY.x, PointY.y };
        int startsearchAW = startsearchA;
        int startsearchBW = startsearchB;
        int startsearchXW = startsearchX;
        int startsearchYW = startsearchY;
        POINT PointAW = PointA;
        POINT PointBW = PointB;
        POINT PointXW = PointX;
        POINT PointYW = PointY;
        LeaveCriticalSection(&critical);



        if (scantick < 3)
            scantick++;
        else scantick = 0;
        if (scanoption == 1)
        {
            if (scantick == 0 && numphotoA > 0)
            {
                if (Astatic == 1)
                {
                    if (staticPointA[startsearchAW].x == 0 && staticPointA[startsearchAW].y == 0)
                        ButtonScanAction("\\A", 1, numphotoA, startsearchAW, true, PointAW, Astatic);
                    else 
                        Bmpfound("\\A", staticPointA[startsearchAW].x, staticPointA[startsearchAW].y, startsearchAW, true, true, Astatic);
                }
                else ButtonScanAction("\\A", 1, numphotoA, startsearchAW, true, PointAW, Astatic);
            }

            if (scantick == 1 && numphotoB > 0)
            {
                if (Bstatic == 1)
                {
                    if (staticPointB[startsearchBW].x == 0 && staticPointB[startsearchBW].y == 0)
                        //MessageBoxA(NULL, "heisann", "A", MB_OK);
                        ButtonScanAction("\\B", 1, numphotoB, startsearchBW, true, PointAW, Bstatic);
                    else
                        Bmpfound("\\B", staticPointB[startsearchBW].x, staticPointB[startsearchBW].y, startsearchBW, true, true, Bstatic);
                }
                else ButtonScanAction("\\B", 1, numphotoB, startsearchBW, true, PointBW, Bstatic);
                
            }
            if (scantick == 2 && numphotoX > 0)
            {
                if (Xstatic == 1)
                {
                    if (staticPointX[startsearchXW].x == 0 && staticPointX[startsearchXW].y == 0)
                        ButtonScanAction("\\X", 1, numphotoX, startsearchXW, true, PointXW, Xstatic);
                    else Bmpfound("\\X", staticPointX[startsearchXW].x, staticPointX[startsearchXW].y, startsearchXW, true, true, Xstatic);
                }
                else ButtonScanAction("\\X", 1, numphotoX, startsearchXW, true, PointXW, Xstatic);
                
            }
            if (scantick == 3 && numphotoY > 0)
            {
                if (Ystatic == 1)
                {
                    if (staticPointY[startsearchYW].x == 0 && staticPointY[startsearchYW].y == 0)
                        //MessageBoxA(NULL, "heisann", "A", MB_OK);
                        ButtonScanAction("\\Y", 1, numphotoY, startsearchYW, true, PointYW, Ystatic);
                    else
                        Bmpfound("\\Y", staticPointY[startsearchYW].x, staticPointY[startsearchYW].y, startsearchYW, true, true, Ystatic);
                }
                ButtonScanAction("\\Y", 1, numphotoY, startsearchYW, true, PointYW, Ystatic);
            }
            Sleep(10);
        }
        //else Sleep(1000);
    }
    return 0;
}
//TODO: width/height probably needs to change


// DrawFakeCursorFix. cursor offset scan and cursor size fix


// Thread to create and run the window
DWORD WINAPI WindowThreadProc(LPVOID) {
   // Sleep(5000);
    //EnterCriticalSection(&critical);
    transparencyBrush = (HBRUSH)CreateSolidBrush(transparencyKey);
    WNDCLASSW wc = { };
    wc.lpfnWndProc = FakeCursorWndProc;
    wc.hInstance = GetModuleHandleW(0);
    wc.hbrBackground = transparencyBrush;

    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    const std::wstring classNameStr = std::wstring{ L"GtoMnK_Pointer_Window" } + std::to_wstring(std::rand());

    wc.lpszClassName = classNameStr.c_str();
    wc.style = CS_OWNDC | CS_NOCLOSE;

    RegisterClassW(&wc);

    pointerWindow = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_NOINHERITLAYOUT | WS_EX_NOPARENTNOTIFY |
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        wc.lpszClassName, L"classNameStr.c_str()",
        WS_POPUP | WS_VISIBLE,
        0, 0, 800, 600,
        nullptr, nullptr, GetModuleHandle(NULL), nullptr);




    if(!pointerWindow) {
       MessageBoxA(NULL, "Failed to create pointer window", "Error", MB_OK | MB_ICONERROR);
       
    }
    SetLayeredWindowAttributes(pointerWindow, transparencyKey, 0, LWA_COLORKEY);
 
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
	//LeaveCriticalSection(&critical);
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message != WM_QUIT)
        {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
		}

    }
    return 0;
}



void pollbuttons(WORD buttons, RECT rect)
{
    if (oldA == true)
    {
        if (buttons & XINPUT_GAMEPAD_A && onoroff == true)
        {
            // keep posting?
        }
        else {
            oldA = false;
            PostKeyFunction(hwnd, Atype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_A && onoroff == true)
    {
        oldA = true;

        if (!scanoption) {
            startsearch = startsearchA;
            ButtonScanAction("\\A", mode, numphotoA, startsearch, false, { 0,0 }, AuseStatic);//buttonacton will be occupied by scanthread
        }
        else
        {
            EnterCriticalSection(&critical);
            POINT Cpoint;
            Cpoint.x = PointA.x;
            Cpoint.y = PointA.y;

            //LeaveCriticalSection(&critical);
            if (Cpoint.x != 0 && Cpoint.y != 0)
            {   
                if (ShoulderNextbmp == 0)
                {
                    if (startsearchA < numphotoA - 1)
                        startsearchA++; //dont want it to update before input done
                    else startsearchA = 0;
                    PointA.x = 0;
                    PointA.y = 0;
                }
                BmpInputAction(Cpoint.x, Cpoint.y, scanAtype);
                foundit = true;

            }
            LeaveCriticalSection(&critical);
        }

        EnterCriticalSection(&critical);
        if (foundit == false && Atype != 0)
        {
            PostKeyFunction(hwnd, Atype, true);
        }
        LeaveCriticalSection(&critical);
        if (mode == 2 && showmessage != 11)
        {
            numphotoA++;
            Sleep(500);
        }
    }



    if (oldB == true)
    {
        if (buttons & XINPUT_GAMEPAD_B && onoroff == true)
        {
            // keep posting?
        }
        else {
            oldB = false;
            PostKeyFunction(hwnd, Btype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_B && onoroff == true)
    {


        oldB = true;
        if (!scanoption) {
            startsearch = startsearchB;
            ButtonScanAction("\\B", mode, numphotoB, startsearch, false, { 0,0 }, BuseStatic);
        }
        else
        {
            EnterCriticalSection(&critical);
            POINT Cpoint;
            Cpoint.x = PointB.x;
            Cpoint.y = PointB.y;

            if (Cpoint.x != 0 && Cpoint.y != 0)
            {
                if (ShoulderNextbmp == 0)
                {
                    if (startsearchB < numphotoB - 1)
                        startsearchB++; //dont want it to update before input done
                    else startsearchB = 0;
                    PointB.x = 0;
                    PointB.y = 0;
                }
                BmpInputAction(Cpoint.x, Cpoint.y, scanBtype);
            }
            LeaveCriticalSection(&critical);
        }
        EnterCriticalSection(&critical);
        if (foundit == false)
        {
            PostKeyFunction(hwnd, Btype, true);
        }
        LeaveCriticalSection(&critical);
        if (mode == 2 && showmessage != 11)
        {
            numphotoB++;
            Sleep(500);
        }

    }



    if (oldX == true)
    {
        if (buttons & XINPUT_GAMEPAD_X && onoroff == true)
        {
            // keep posting?
        }
        else {
            oldX = false;
            PostKeyFunction(hwnd, Xtype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_X && onoroff == true)
    {
        oldX = true;


        if (!scanoption)
        {
            startsearch = startsearchX;
            ButtonScanAction("\\X", mode, numphotoX, startsearch, false, { 0,0 }, XuseStatic);
        }
        else
        {
            EnterCriticalSection(&critical);
            POINT Cpoint;
            Cpoint.x = PointX.x;
            Cpoint.y = PointX.y;

            movedmouse = true;
            if (Cpoint.x != 0 && Cpoint.y != 0)
            {
                if (ShoulderNextbmp == 0)
                {
                    if (startsearchX < numphotoX - 1)
                        startsearchX++; //dont want it to update before input done
                    else startsearchX = 0;
                    PointX.x = 0;
                    PointX.y = 0;
                }
                BmpInputAction(Cpoint.x, Cpoint.y, scanXtype);
                
            }
            LeaveCriticalSection(&critical);
        }
        EnterCriticalSection(&critical);
        if (foundit == false)
        {
            PostKeyFunction(hwnd, Xtype, true);
        }
        LeaveCriticalSection(&critical);
        if (mode == 2 && showmessage != 11)
        {
            numphotoX++;
            Sleep(500);
        }
    }



    if (oldY == true)
    {
        if (buttons & XINPUT_GAMEPAD_Y && onoroff == true)
        {
            // keep posting?
        }
        else {
            oldY = false;
            PostKeyFunction(hwnd, Ytype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_Y && onoroff == true)
    {
        oldY = true;


        if (!scanoption) {
            startsearch = startsearchY;
            ButtonScanAction("\\Y", mode, numphotoY, startsearch, false, { 0,0 }, false);
        }
        else
        {
            EnterCriticalSection(&critical);
            POINT Cpoint;
            Cpoint.x = PointY.x;
            Cpoint.y = PointY.y;

            if (Cpoint.x != 0 && Cpoint.y != 0)
            {
                if (ShoulderNextbmp == 0)
                { 
                    if (startsearchY < numphotoY - 1)
                        startsearchY++; //dont want it to update before input done
                    else startsearchY = 0;
                    PointY.x = 0;
                    PointY.y = 0;
                }
                BmpInputAction(Cpoint.x, Cpoint.y, scanYtype);
                
            }
            LeaveCriticalSection(&critical);
        }
        if (mode == 2 && showmessage != 11)
        { //mapping mode
            numphotoY++;
            Sleep(500);
        }
        EnterCriticalSection(&critical);
        if (foundit == false)
        {
            PostKeyFunction(hwnd, Ytype, true);
        }
        LeaveCriticalSection(&critical);
    }



    if (oldC == true)
    {
        if (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER && onoroff == true)
        {
            // keep posting?
        }
        else {
            oldC = false;
            PostKeyFunction(hwnd, Ctype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER && onoroff == true)
    {
        oldC = true;
        
        if (ShoulderNextbmp == 1)
        {
            EnterCriticalSection(&critical);
            if (startsearchA < numphotoA - 1)
                startsearchA++; //dont want it to update before input done
            else startsearchA = 0;
            if (startsearchB < numphotoB - 1)
                startsearchB++; //dont want it to update before input done
            else startsearchB = 0;
            if (startsearchX < numphotoX - 1)
                startsearchX++; //dont want it to update before input done
            else startsearchX = 0;
            LeaveCriticalSection(&critical);
        }
        startsearch = startsearchC;
        if (!scanoption)
            ButtonScanAction("\\C", mode, numphotoC, startsearch, false, { 0,0 }, false);
        EnterCriticalSection(&critical);
        if (foundit == false)
        {
            PostKeyFunction(hwnd, Ctype, true);
        }
        LeaveCriticalSection(&critical);
        if (mode == 2 && showmessage == 0)
        {
            numphotoC++;
            Sleep(500);
        }
    }



    if (oldD == true)
    {
        if (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER && onoroff == true)
        {
            // keep posting?
        }
        else {
            oldD = false;
            PostKeyFunction(hwnd, Dtype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER && onoroff == true)
    {
        oldD = true;

        startsearch = startsearchD;
        if (!scanoption)
            ButtonScanAction("\\D", mode, numphotoD, startsearch, false, { 0,0 }, false);
        EnterCriticalSection(&critical);
        if (ShoulderNextbmp)
        {
            
            if (startsearchA > 0)
                startsearchA--;
            else startsearchA = numphotoA - 1;
            if (startsearchB > 0)
                startsearchB--;
            else startsearchB = numphotoB -1;
            if (startsearchX > 0)
                startsearchX--;
            else startsearchX = numphotoX -1;
            if (startsearchY > 0)
                startsearchY--;
            else startsearchY = numphotoY -1;
            
        }
        if (foundit == false)
        {
            PostKeyFunction(hwnd, Dtype, true);
        }
        LeaveCriticalSection(&critical);
        if (mode == 2 && showmessage != 11)
        {
            numphotoD++;
            Sleep(500);
        }
    }




    if (oldE == true)
    {
        if (buttons & XINPUT_GAMEPAD_RIGHT_THUMB && onoroff == true)
        {
            // keep posting?
        }
        else {
            oldE = false;
            PostKeyFunction(hwnd, Etype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_RIGHT_THUMB && onoroff == true)
    {
        oldE = true;

        startsearch = startsearchE;
        if (!scanoption)
            ButtonScanAction("\\E", mode, numphotoE, startsearch, false, { 0,0 }, false);
        EnterCriticalSection(&critical);
        if (foundit == false)
        {
            PostKeyFunction(hwnd, Etype, true);
        }
        LeaveCriticalSection(&critical);
        if (mode == 2 && showmessage != 11)
        {
            numphotoE++;
            Sleep(500);
        }
    }



    if (oldF == true)
    {
        if (buttons & XINPUT_GAMEPAD_LEFT_THUMB && onoroff == true)
        {
            // keep posting?
        }
        else {
            oldF = false;
            PostKeyFunction(hwnd, Ftype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_LEFT_THUMB && onoroff == true)
    {
        oldF = true;

        startsearch = startsearchF;
        if (!scanoption)
            ButtonScanAction("\\F", mode, numphotoF, startsearch, false, { 0,0 }, false);
        EnterCriticalSection(&critical);
        if (foundit == false)
        {
            PostKeyFunction(hwnd, Ftype, true);
        }
        LeaveCriticalSection(&critical);
        if (mode == 2 && showmessage != 11)
        {
            numphotoF++;
            Sleep(500);
        }
    }




    if (oldup)
    {
        if (buttons & XINPUT_GAMEPAD_DPAD_UP && onoroff == true)
        {
            //post keep?
            if (scrolloutsidewindow >= 3 && quickMW == 1) {

                SendMouseClick(Xf, Yf, 20, 1);

            }
        }
        else {
            oldup = false;
            PostKeyFunction(hwnd, uptype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_DPAD_UP && onoroff == true)
    {

        scroll.x = rect.left + (rect.right - rect.left) / 2;
        if (scrolloutsidewindow == 0)
            scroll.y = rect.top + 1;
        if (scrolloutsidewindow == 1)
            scroll.y = rect.top - 1;
        scrollmap = true;
        if (scrolloutsidewindow == 2) {
            oldup = true;
            PostKeyFunction(hwnd, uptype, true);
        }
        if (scrolloutsidewindow >= 3) {
            oldup = true;
            scrollmap = false;

            SendMouseClick(Xf, Yf, 20, 1);


        }
    }




    else if (olddown)
    {
        if (buttons & XINPUT_GAMEPAD_DPAD_DOWN && onoroff == true)
        {
            //post keep?
            if (scrolloutsidewindow >= 3 && quickMW == 1) {

                SendMouseClick(Xf, Yf, 21, 1);

            }
        }
        else {
            olddown = false;
            PostKeyFunction(hwnd, downtype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_DPAD_DOWN && onoroff == true)
    {

        scroll.x = rect.left + (rect.right - rect.left) / 2;
        if (scrolloutsidewindow == 0)
            scroll.y = rect.bottom - 1;
        if (scrolloutsidewindow == 1)
            scroll.y = rect.bottom + 1;
        scrollmap = true;
        if (scrolloutsidewindow == 2) {
            olddown = true;
            PostKeyFunction(hwnd, downtype, true);
        }
        if (scrolloutsidewindow >= 3) {
            olddown = true;
            scrollmap = false;

            SendMouseClick(Xf, Yf, 21, 1);


        }
    }





    else if (oldleft)
    {
        if (buttons & XINPUT_GAMEPAD_DPAD_LEFT && onoroff == true)
        {
            //post keep?
        }
        else {
            oldleft = false;
            PostKeyFunction(hwnd, lefttype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_DPAD_LEFT && onoroff == true)
    {
        if (scrolloutsidewindow == 0)
            scroll.x = rect.left + 1;
        if (scrolloutsidewindow == 1)
            scroll.x = rect.left - 1;
        scroll.y = rect.top + (rect.bottom - rect.top) / 2;

        scrollmap = true;
        if (scrolloutsidewindow == 2 || scrolloutsidewindow == 4) {
            oldleft = true;
            PostKeyFunction(hwnd, lefttype, true);
        }

    }





    else if (oldright)
    {
        if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT && onoroff == true)
        {
            //post keep?
        }
        else {
            oldright = false;
            PostKeyFunction(hwnd, righttype, false);
        }
    }
    else if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT && onoroff == true)
    {
        if (scrolloutsidewindow == 0)
            scroll.x = rect.right - 1;
        if (scrolloutsidewindow == 1)
            scroll.x = rect.right + 1;
        scroll.y = rect.top + (rect.bottom - rect.top) / 2;
        scrollmap = true;
        if (scrolloutsidewindow == 2 || scrolloutsidewindow == 4) {
            oldright = true;
            PostKeyFunction(hwnd, righttype, true);
        }
    }
    else
    {
        scrollmap = false;
    }




    if (buttons & XINPUT_GAMEPAD_START && (showmessage == 0 || showmessage == 4))
    {
        Sleep(100);
        if (onoroff == true && buttons & XINPUT_GAMEPAD_LEFT_SHOULDER && buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
        {
            //MessageBox(NULL, "Bmp mode", "shutdown", MB_OK | MB_ICONINFORMATION);
            showmessage = 69;
        }
        else if (onoroff == false && buttons & XINPUT_GAMEPAD_LEFT_SHOULDER && buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
        {
            //MessageBox(NULL, "Bmp mode", "starting", MB_OK | MB_ICONINFORMATION);
            showmessage = 70;
        }
        else if (mode == 0 && Modechange == 1 && onoroff == true)
        {
            mode = 1;

            // MessageBox(NULL, "Bmp + Emulated cursor mode", "Move the flickering red dot and use right trigger for left click. left trigger for right click", MB_OK | MB_ICONINFORMATION);
            showmessage = 2;
        }
        else if (mode == 1 && Modechange == 1 && onoroff == true)
        {
            mode = 2;
            //MessageBox(NULL, "Edit Mode", "Button mapping. will map buttons you click with the flickering red dot as an input coordinate", MB_OK | MB_ICONINFORMATION);
            showmessage = 3;


        }
        else if (mode == 2 && Modechange == 1 && onoroff == true)
        {
            mode = 3;
            // MessageBox(NULL, "Bmp mode", "only send input on bmp match", MB_OK | MB_ICONINFORMATION);
            showmessage = 4;
            Sleep(1000);
        }
        else if (mode == 3 && Modechange == 1 && onoroff == true)
        {
            // mode = 0;
            // MessageBox(NULL, "Bmp mode", "only send input on bmp match", MB_OK | MB_ICONINFORMATION);
            showmessage = 1;
        }

        else if (onoroff == true) { //assume modechange not allowed. send escape key instead
            keystatesend = VK_ESCAPE;
        }
        // Sleep(1000); //have time to release button. this is no hurry anyway

    }
}
int HowManyBmps(std::wstring path, bool andstatics)
{
    int start = -1;
  
    int x = 0;
    std::wstring filename;
    while (x < 50 && start == -1)
    {
        filename = path + std::to_wstring(x) + L".bmp";
        if (HBITMAP hbm = (HBITMAP)LoadImageW(NULL, filename.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION))
        {
            x++;
            DeleteObject(hbm);

        } 
        else{
            start = x;
        }
    }

    //searching statics
    x = 0;
    int inistart = -1;
    while (x < 50 && inistart == -1)
    {
        std::string iniPath = UGetExecutableFolder() + "\\Xinput.ini";
        std::string iniSettings = "Statics";

        std::string name(path.end() - 1, path.end());
        std::string string = name.c_str() + std::to_string(x) + "X";
       
       
        int sjekkX = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str()); //simple test if settings read but write it wont work.
        if (sjekkX != 0)
        {
            string = name.c_str() + std::to_string(x) + "X";
            x++;
        } 
        else inistart = x;
    }
    if (!andstatics || inistart == 0)
        return start;
    else return start + inistart;
}



bool initovector()
{
    std::string iniPath = UGetExecutableFolder() + "\\Xinput.ini";
    std::string iniSettings = "Statics";
    std::string name = "A";
    int y = -1;
    int sjekkx = 0;
    bool test = false;
    int x = -1;
    int scalemethod = 0;
    POINT inipoint;
    while (x < 50 && y == -1)
    {
        x++;
        std::string string = name + std::to_string(x) + "X";
        sjekkx = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());
        if (sjekkx != 0)
        {
            test = true;
            inipoint.x = sjekkx;

            string = name + std::to_string(x) + "Y";
            inipoint.y = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());

            string = name + std::to_string(x) + "Z";
            scalemethod = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());
            if (scalemethod != 0)
                inipoint = GetStaticFactor(inipoint, scalemethod, true);
            staticPointA[x + numphotoYbmps].y = inipoint.y;
            staticPointA[x + numphotoYbmps].x = inipoint.x;
        }
        else y = 10;
    }
    y = -1;
    name = "B";
    sjekkx = 0;
    x = -1;
    while (x < 50 && y == -1)
    {
        x++;
        std::string string = name + std::to_string(x) + "X";
        sjekkx = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());
        if (sjekkx != 0)
        {
            test = true;
            inipoint.x = sjekkx;

            string = name + std::to_string(x) + "Y";
            inipoint.y = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());

            string = name + std::to_string(x) + "Z";
            scalemethod = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());
            if (scalemethod != 0)
                inipoint = GetStaticFactor(inipoint, scalemethod, true);
            staticPointB[x + numphotoYbmps].y = inipoint.y;
            staticPointB[x + numphotoYbmps].x = inipoint.x;
        }
        
        else y = 10;
    }
    y = -1;
    name = "X";
    sjekkx = 0;
    x = -1;
    while (x < 50 && y == -1)
    {
        x++;
        std::string string = name.c_str() + std::to_string(x) + "X";
        //MessageBoxA(NULL, "no bmps", "aaahaAAAHAA", MB_OK);
        sjekkx = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());
        if (sjekkx != 0)
        {
            test = true;
            inipoint.x = sjekkx;

            string = name + std::to_string(x) + "Y";
            inipoint.y = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());

            string = name + std::to_string(x) + "Z";
            scalemethod = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());
            if (scalemethod != 0)
                inipoint = GetStaticFactor(inipoint, scalemethod, true);
            staticPointX[x + numphotoYbmps].y = inipoint.y;
            staticPointX[x + numphotoYbmps].x = inipoint.x;

        }
        
        else y = 10;
    }
    y = -1;
    name = "Y";
    sjekkx = 0;
    x = -1;
    while (x < 50 && y == -1)
    {
        x++;
        std::string string = name.c_str() + std::to_string(x) + "X";
        sjekkx = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());
        if (sjekkx != 0)
        {
            test = true;
            inipoint.x = sjekkx;

            string = name + std::to_string(x) + "Y";
            inipoint.y = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());

            string = name + std::to_string(x) + "Z";
            scalemethod = GetPrivateProfileInt(iniSettings.c_str(), string.c_str(), 0, iniPath.c_str());
            if (scalemethod != 0)
                inipoint = GetStaticFactor(inipoint, scalemethod, true);
            staticPointY[x + numphotoYbmps].y = inipoint.y;
            staticPointY[x + numphotoYbmps].x = inipoint.x;
        }
        else y = 10;
    }
    if (test == true)
        return true;
    else return false; //no points
}


bool enumeratebmps()
{


        std::wstring path = WGetExecutableFolder() + L"\\A";
        numphotoA = HowManyBmps(path, true);
        numphotoAbmps = HowManyBmps(path, false);
        path = WGetExecutableFolder() + L"\\B";
        numphotoB = HowManyBmps(path, true);
        numphotoBbmps = HowManyBmps(path, false);
        path = WGetExecutableFolder() + L"\\X";
        numphotoX = HowManyBmps(path, true);
        numphotoXbmps = HowManyBmps(path, false);
        path = WGetExecutableFolder() + L"\\Y";
        numphotoY = HowManyBmps(path, true);
        numphotoYbmps = HowManyBmps(path, false);
        path = WGetExecutableFolder() + L"\\C";
        numphotoC = HowManyBmps(path, false);
        path = WGetExecutableFolder() + L"\\D";
        numphotoD = HowManyBmps(path, false);
        path = WGetExecutableFolder() + L"\\E";
        numphotoE = HowManyBmps(path, false);
        path = WGetExecutableFolder() + L"\\F";
        numphotoF = HowManyBmps(path, false);
  
        if (numphotoA < 0 && numphotoB < 0 && numphotoX < 0 && numphotoY < 00)
        {
            
            return false;
        }
            
        return true;
}
bool readsettings(){
    char buffer[16]; //or only 4 maybe

    // settings reporting
    std::string iniPath = UGetExecutableFolder() + "\\Xinput.ini";
    std::string iniSettings = "Settings";

    //controller settings
    controllerID = GetPrivateProfileInt(iniSettings.c_str(), "Controllerid", -9000, iniPath.c_str()); //simple test if settings read
    AxisLeftsens = GetPrivateProfileInt(iniSettings.c_str(), "AxisLeftsens", -9000, iniPath.c_str());
    AxisRightsens = GetPrivateProfileInt(iniSettings.c_str(), "AxisRightsens", 9000, iniPath.c_str());
    AxisUpsens = GetPrivateProfileInt(iniSettings.c_str(), "AxisUpsens", 9000, iniPath.c_str());
    AxisDownsens = GetPrivateProfileInt(iniSettings.c_str(), "AxisDownsens", -9000, iniPath.c_str());
    scrollspeed3 = GetPrivateProfileInt(iniSettings.c_str(), "Scrollspeed", 150, iniPath.c_str());
    righthanded = GetPrivateProfileInt(iniSettings.c_str(), "Righthanded", 1, iniPath.c_str());
    scanoption = GetPrivateProfileInt(iniSettings.c_str(), "Scan", 0, iniPath.c_str());
    GetPrivateProfileString(iniSettings.c_str(), "Radial_Deadzone", "0.2", buffer, sizeof(buffer), iniPath.c_str());
    radial_deadzone = std::stof(buffer); //sensitivity

    //window set pos
    posX = GetPrivateProfileInt(iniSettings.c_str(), "posX", 0, iniPath.c_str());
    posY = GetPrivateProfileInt(iniSettings.c_str(), "posY", 0, iniPath.c_str());
    resX = GetPrivateProfileInt(iniSettings.c_str(), "resX", 0, iniPath.c_str());
    resY = GetPrivateProfileInt(iniSettings.c_str(), "resY", 0, iniPath.c_str());

    GetPrivateProfileString(iniSettings.c_str(), "Axial_Deadzone", "0.0", buffer, sizeof(buffer), iniPath.c_str());
    axial_deadzone = std::stof(buffer); //sensitivity

    GetPrivateProfileString(iniSettings.c_str(), "Sensitivity", "20.0", buffer, sizeof(buffer), iniPath.c_str());
    sensitivity = std::stof(buffer); //sensitivity //accel_multiplier

    GetPrivateProfileString(iniSettings.c_str(), "Accel_Multiplier", "1.7", buffer, sizeof(buffer), iniPath.c_str());
    accel_multiplier = std::stof(buffer);

    Xoffset = GetPrivateProfileInt(iniSettings.c_str(), "Xoffset", 0, iniPath.c_str());
    Yoffset = GetPrivateProfileInt(iniSettings.c_str(), "Yoffset", 0, iniPath.c_str());

    //mode
    InitialMode = GetPrivateProfileInt(iniSettings.c_str(), "Initial Mode", 1, iniPath.c_str());
    Modechange = GetPrivateProfileInt(iniSettings.c_str(), "Allow modechange", 1, iniPath.c_str());

    sendfocus = GetPrivateProfileInt(iniSettings.c_str(), "Sendfocus", 0, iniPath.c_str());
    responsetime = GetPrivateProfileInt(iniSettings.c_str(), "Responsetime", 0, iniPath.c_str());
    doubleclicks = GetPrivateProfileInt(iniSettings.c_str(), "Doubleclicks", 0, iniPath.c_str());
    scrollenddelay = GetPrivateProfileInt(iniSettings.c_str(), "DelayEndScroll", 50, iniPath.c_str());
    quickMW = GetPrivateProfileInt(iniSettings.c_str(), "MouseWheelContinous", 0, iniPath.c_str());

    //clicknotmove 2000 + keyboard key code
    //movenotclick 1000 + keyboard key code
    Atype = GetPrivateProfileInt(iniSettings.c_str(), "A", 0, iniPath.c_str());
    Btype = GetPrivateProfileInt(iniSettings.c_str(), "B", 0, iniPath.c_str());
    Xtype = GetPrivateProfileInt(iniSettings.c_str(), "X", 1, iniPath.c_str());
    Ytype = GetPrivateProfileInt(iniSettings.c_str(), "Y", 0, iniPath.c_str());
    Ctype = GetPrivateProfileInt(iniSettings.c_str(), "RightShoulder", 2, iniPath.c_str());
    Dtype = GetPrivateProfileInt(iniSettings.c_str(), "LeftShoulder", 0, iniPath.c_str());
    Etype = GetPrivateProfileInt(iniSettings.c_str(), "RightThumb", 0, iniPath.c_str());
    Ftype = GetPrivateProfileInt(iniSettings.c_str(), "LeftThumb", 0, iniPath.c_str());
    AuseStatic = GetPrivateProfileInt(iniSettings.c_str(), "ScanAstatic", 0, iniPath.c_str());
    BuseStatic = GetPrivateProfileInt(iniSettings.c_str(), "ScanBstatic", 0, iniPath.c_str());
    XuseStatic = GetPrivateProfileInt(iniSettings.c_str(), "ScanXstatic", 0, iniPath.c_str());
    YuseStatic = GetPrivateProfileInt(iniSettings.c_str(), "ScanYstatic", 0, iniPath.c_str());
    //setting bmp search type like flag values
    if (Atype > 1999)
    {
        bmpAtype = 2;
        scanAtype = 2;
        Atype = Atype - 2000;

    }
    else if (Atype > 999)
    {
        bmpAtype = 1;
        scanAtype = 1;
        Atype = Atype - 1000;
    }
    else
    {
        bmpAtype = 0;
        scanAtype = 0;
    }

    if (Btype > 1999)
    {
        bmpBtype = 2;
        scanBtype = 2;
        Btype = Btype - 2000;
    }
    else if (Btype > 999)
    {
        bmpBtype = 1;
        scanBtype = 1;
        Btype = Btype - 1000;
    }
    else
    {
        bmpBtype = 0;
        scanBtype = 0;
    }

    if (Xtype > 1999)
    {
        scanXtype = 2;
        bmpXtype = 2;
        Xtype = Xtype - 2000;
    }
    else if (Xtype > 999)
    {
        bmpXtype = 1;
        scanXtype = 1;
        Xtype = Xtype - 1000;
    }
    else
    {
        bmpXtype = 0;
        scanXtype = 0;
    }

    if (Ytype > 1999)
    {
        bmpYtype = 2;
        scanYtype = 2;
        Ytype = Ytype - 2000;
    }
    else if (Ytype > 999)
    {
        bmpYtype = 1;
        scanYtype = 1;
        Ytype = Ytype - 1000;
    }
    else
    {
        bmpYtype = 0;
        scanYtype = 0;
    }

    if (Ctype > 1999)
    {
        bmpCtype = 2;
        Ctype = Ctype - 2000;
    }
    else if (Ctype > 999)
    {
        bmpCtype = 1;
        Ctype = Ctype - 1000;
    }
    else
    {
        bmpCtype = 0;
    }

    if (Dtype > 1999)
    {
        bmpDtype = 2;
        Dtype = Dtype - 2000;
    }
    else if (Dtype > 999)
    {
        bmpDtype = 1;
        Dtype = Dtype - 1000;
    }
    else
    {
        bmpDtype = 0;
    }

    if (Etype > 1999)
    {
        bmpEtype = 2;
        Etype = Etype - 2000;
    }
    else if (Etype > 999)
    {
        bmpEtype = 1;
        Etype = Etype - 1000;
    }
    else
    {
        bmpEtype = 0;
    }

    if (Ftype > 1999)
    {
        Ftype = Ftype - 2000;
    }
    else if (Ftype > 999)
    {
        bmpFtype = 1;
        Ftype = Ftype - 1000;
    }
    else
    {
        bmpFtype = 0;
    }

    uptype = GetPrivateProfileInt(iniSettings.c_str(), "Upkey", -1, iniPath.c_str());
    downtype = GetPrivateProfileInt(iniSettings.c_str(), "Downkey", -2, iniPath.c_str());
    lefttype = GetPrivateProfileInt(iniSettings.c_str(), "Leftkey", -3, iniPath.c_str());
    righttype = GetPrivateProfileInt(iniSettings.c_str(), "Rightkey", -4, iniPath.c_str());
    ShoulderNextbmp = GetPrivateProfileInt(iniSettings.c_str(), "ShouldersNextBMP", 0, iniPath.c_str());

    //hooks
    drawfakecursor = GetPrivateProfileInt(iniSettings.c_str(), "DrawFakeCursor", 1, iniPath.c_str());
    alwaysdrawcursor = GetPrivateProfileInt(iniSettings.c_str(), "DrawFakeCursorAlways", 1, iniPath.c_str());
    userealmouse = GetPrivateProfileInt(iniSettings.c_str(), "UseRealMouse", 0, iniPath.c_str());   //scrolloutsidewindow
    ignorerect = GetPrivateProfileInt(iniSettings.c_str(), "IgnoreRect", 0, iniPath.c_str());

    scrolloutsidewindow = GetPrivateProfileInt(iniSettings.c_str(), "Scrollmapfix", 1, iniPath.c_str());   //scrolloutsidewindow
    mode = InitialMode;
    if (drawfakecursor == 3)
        DrawFakeCursorFix = true;
    if (controllerID == -9999)
    {
        return false;
        controllerID = 0; //default controller  

    }
    return true;
}   
void ThreadFunction(HMODULE hModule)
{
    Sleep(2000);

    if (readsettings() == false)
    {
        //messagebox? settings not read
    }
    //Sleep(findwindowdelay * 1000);
    
   
    //hwnd = GetMainWindowHandle(GetCurrentProcessId());
    UpdateMainHwnd(true);
    RECT rect;
    bool Aprev = false;
    if (scanoption == 1)
    { //starting bmp conttinous scanner
        std::thread tree(ScanThread, g_hModule, AuseStatic, BuseStatic, XuseStatic, YuseStatic);
        tree.detach();
    }
    if (drawfakecursor == 0)
    {
		drawfakecursor = 1;
        nodrawcursor = true;
    }
	bool window = false;
	bool gotwindow = false;
    showmessage = 99;
    while (loop == true)
    {
        rawmouseWu = false; //reset
		rawmouseWd = false; //reset
        movedmouse = false; //reset
        keystatesend = 0; //reset keystate
		foundit = false; //reset foundit the bmp search found or not
        if (hwnd == NULL)
        {
			Sleep(1000); //no enumeration spamming
           // hwnd = GetMainWindowHandle(GetCurrentProcessId());
            UpdateMainHwnd(true);
            showmessage = 99; //back to intro
            counter = 0;
        }
        
        else
		{ // maybe inject hooks here instead. also getcursorpos first, and set Xf Yf to pos to let Xf and Yf countinue where real cursor pos left off
            //POINT screenpos = getcursorpos(&pos)  get window pos: POINT windowpos = windowpos( hwnd, ignorerect, true) width: POINT Windowres = windowpos( hwnd, ignorerect, false)
            //if in bounds: if (screenpos.x > windowpos.x && screenpos.y > windowpos.y && screenpos.x < windowpos.x + windowres.x && screenpos.y < windowpos.y + windowres.y) //then in bounds
                //Xf = screenpos.x - windowpos.x; Yf = screenpos.y - windowpos.y; //set fake cursor to real cursor pos. looks correct

            //postmessage WM_Mousehover Xf Yf on standard values if real was outside window? is that opposite message of WM_Mouseleave?
           // UpdateMainHwnd(true); //hwnd check?
			gotwindow = true;
            if (!IsMainWindow(hwnd))
            { 
                if (scanloop)
                    scanloop = false; //stop scan thread while no hwnd //not started yet
                hwnd = NULL; //force update
                while (hwnd == NULL) // cant proceed while no hwnd
                {
                    if (gotwindow && hooksinited)
                    { 
                        MH_DisableHook(MH_ALL_HOOKS);
					    gotwindow = false;
						
                    }

                    ShowWindow(pointerWindow, SW_SHOW);
                    Sleep(1000); //no enumeration spamming and time for scanner shutdown
                    //MessageBoxA(NULL, "Error. lost gamewindow", "where did it go?", MB_OK);
                    UpdateMainHwnd(true);
                    if (hwnd)
                    {
                        SendMessage(pointerWindow, WM_MOVE_pointerWindow, 0, 0); //focus to avoid alt tab issues
                        if (scanoption == 1)
                        { 
                            scanloop = true; //restarting scanner
                            std::thread treerestartn(ScanThread, g_hModule, AuseStatic, BuseStatic, XuseStatic, YuseStatic);
                            treerestartn.detach();
                        }
                        showmessage = 99; //back to intro
                        counter = 0;
                    }
                    else
                    {
                        SetWindowPos(pointerWindow, HWND_TOPMOST,
                        0,
                        0,
                        300,
                        300,
                        SWP_NOACTIVATE);
                    }
				}
            }
            if (hooksoninit == 2 && hooksinited == false)
            {
                SetupHook();
            }
            if (gotwindow == false && hooksinited)
            {
                MH_EnableHook(MH_ALL_HOOKS);
                gotwindow = true;
            }

            if (!inithere)
            {
                if (!enumeratebmps()) //always this before initovector
                {
                    if (scanoption)
                    { 
                        MessageBoxA(NULL, "Error. scanoption without bmps", "No BMPS found", MB_OK);
						scanloop = false; //force stop scan thread
                        scanoption = 0;
                    }

                }

				//resize window if set in ini
                if (resX != 0)
                {
                    SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                    SetWindowPos(hwnd, NULL, posX, posY, resX, resY, SWP_NOZORDER | SWP_NOACTIVATE);
                }

                staticPointA.assign(numphotoA + 1, POINT{ 0, 0 });
                staticPointB.assign(numphotoB + 1, POINT{ 0, 0 });
                staticPointX.assign(numphotoX + 1, POINT{ 0, 0 });
                staticPointY.assign(numphotoY + 1, POINT{ 0, 0 });
                initovector();
                inithere = true;
            }
			//   
			POINT poscheck = windowpos(hwnd, ignorerect, true);
            POINT rescheck = windowpos(hwnd, ignorerect, false);
            if (drawfakecursor == 2 || drawfakecursor == 3)
            {//fake cursor window creation
                
                if (!window) 
                { 
                    std::thread two(WindowThreadProc, g_hModule);
                    two.detach();
					Sleep(100); //give time to create window
                    EnterCriticalSection(&critical);
                    while (!pointerWindow) { 
                        MessageBoxA(NULL, "No pointerwindow", "ohno,try drawfakecursor 1 instead", MB_OK);
                        Sleep(1000); 
                    }
                    if (pointerWindow){
                        //Sleep(500);
                        PointerWnd = GetDC(pointerWindow);
					    SendMessage(pointerWindow, WM_MOVE_pointerWindow, 0, 0); //focus to avoid alt tab issues
                        LeaveCriticalSection(&critical);
                    }
				    window = true;
                }
                else if (poscheck.x != oldposcheck.x || oldposcheck.y != oldposcheck.y || oldrescheck.y != oldrescheck.y || oldrescheck.x != oldrescheck.x)
                {
                    EnterCriticalSection(&critical);
                    if (pointerWindow)
                        SendMessage(pointerWindow, WM_MOVE_pointerWindow, 0, 0); 
					//clearing points and reinit on window move or resize
                    staticPointA.clear();
                    staticPointB.clear();
                    staticPointX.clear();
                    staticPointY.clear();
                    initovector(); //this also call for scaling if needed
                    LeaveCriticalSection(&critical);
                    Sleep(1000); //pause renderiing
                }

                oldposcheck.x = poscheck.x;
                oldposcheck.y = poscheck.y;

                oldrescheck.x = rescheck.x;
                oldrescheck.y = rescheck.y;

			}
			
            if (ignorerect == 1)
            {
                    // These are the actual visible edges of the window in client coordinates
                    POINT upper = windowpos(hwnd, ignorerect, true);
                    POINT lower = windowpos(hwnd, ignorerect, false);

                    //used in getcursrorpos
                    rectignore.x = upper.x;
                    rectignore.y = upper.y; //+titlebar if any?

                    rect.right = lower.x + upper.x; //+ upper?
                    rect.bottom = lower.y + upper.y;//+ upper?
                    rect.left = 0; //upper?
                    rect.top = 0;

            }
            else 
            {
                GetClientRect(hwnd, &rect);
			}
            XINPUT_STATE state;
            ZeroMemory(&state, sizeof(XINPUT_STATE));
            // Check controller 0
            DWORD dwResult = XInputGetState(controllerID, &state);
            bool leftPressed = IsTriggerPressed(state.Gamepad.bLeftTrigger);
            bool rightPressed = IsTriggerPressed(state.Gamepad.bRightTrigger);

            
            if (dwResult == ERROR_SUCCESS)
            {
                
                //criticals for windowthread // now only scanthread.
                // //window rendering handled by current thread
                EnterCriticalSection(&critical);
                fakecursorW.x = Xf;
                fakecursorW.y = Yf;
				showmessageW = showmessage;
                LeaveCriticalSection(&critical);
			
                // Controller is connected
                WORD buttons = state.Gamepad.wButtons;
                pollbuttons(buttons, rect); //all buttons exept triggers and axises
                if (showmessage == 12) //was disconnected last scan?
                {
                    showmessage = 0;
				}

                if (mode > 0 && onoroff == true)
                { 
                    //fake cursor poll
                    int Xaxis = 0;
                    int Yaxis = 0;
					int scrollXaxis = 0;
					int scrollYaxis = 0;    
                    POINT resw = windowpos(hwnd, ignorerect, false);
                    int width = resw.x;
                    int height = resw.y;
                    int Yscroll = 0;
                    int Xscroll = 0;
                    bool didscroll = false;
                    
				 
                    if (righthanded == 1) {
                        Xaxis = state.Gamepad.sThumbRX;
                        Yaxis = state.Gamepad.sThumbRY;
                        scrollXaxis = state.Gamepad.sThumbLX;
						scrollYaxis = state.Gamepad.sThumbLY;   
                    }
                    else
                    {
                        Xaxis = state.Gamepad.sThumbLX;
                        Yaxis = state.Gamepad.sThumbLY;
						scrollXaxis = state.Gamepad.sThumbRX;   
						scrollYaxis = state.Gamepad.sThumbRY;   
                    }

                    if (scrolloutsidewindow == 2 || scrolloutsidewindow == 3 || scrolloutsidewindow == 4)
                    {
                        
                        if (oldscrollleftaxis) 
                        {
                            if (scrollXaxis < AxisLeftsens) //left
                            {
                                if (scrolloutsidewindow == 3)
                                { //keep
									scrollXaxis = scrollXaxis - AxisLeftsens; //zero input
                                    doscrollyes = true; 
                                    Xscroll = + scrollXaxis / scrollspeed3;
                                    didscroll = true;
                                }
                               // PostKeyFunction(hwnd, 42, true);
                            }
                            else 
                            { //stop
                                oldscrollleftaxis = false;
                                if (scrolloutsidewindow == 2)
                                    PostKeyFunction(hwnd, 42, false);
                                if (scrolloutsidewindow == 4)
                                    PostKeyFunction(hwnd, 12, false);
                            }
                        }
                        else if (scrollXaxis < AxisLeftsens) //left
                        {
                            if (scrolloutsidewindow == 2)
                                PostKeyFunction(hwnd, 42, true);
                            if (scrolloutsidewindow == 4)
                                PostKeyFunction(hwnd, 12, true);
                            if (scrolloutsidewindow == 3 && doscrollyes == false)
                            {//start
                                tick = 0;
                                SendMouseClick(Xf, Yf, 8, 1); 
                                SendMouseClick(Xf, Yf, 5, 2); //4 skal vere 3 
                            }
                            oldscrollleftaxis = true;
                            //keystatesend = VK_LEFT;
                        }


                        if (oldscrollrightaxis)
                        {
                            if (scrollXaxis > AxisRightsens) //right
                            {
                                if (scrolloutsidewindow == 3)
                                { //keep
                                    doscrollyes = true;
									scrollXaxis = scrollXaxis - AxisRightsens; //zero input   
                                    Xscroll = scrollXaxis / scrollspeed3;
                                    didscroll = true;
                                }
                            }
                            else {
                                oldscrollrightaxis = false;
                                if (scrolloutsidewindow == 2)
                                    PostKeyFunction(hwnd, 43, false);
                                if (scrolloutsidewindow == 4)
                                    PostKeyFunction(hwnd, 13, false);
                            }
                        }
                        else if (scrollXaxis > AxisRightsens) //right
                        {
                            if (scrolloutsidewindow == 2)
                                PostKeyFunction(hwnd, 43, true);
                            if (scrolloutsidewindow == 4)
                                PostKeyFunction(hwnd, 13, true);
                            if (scrolloutsidewindow == 3 && doscrollyes == false)
                            {//start
                                tick = 0;
                                SendMouseClick(Xf, Yf, 8, 1); 
                                SendMouseClick(Xf, Yf, 5, 2); //4 skal vere 3
                            }
                            oldscrollrightaxis = true;
                            //keystatesend = VK_RIGHT;

                        }



                        if (oldscrolldownaxis)
                        {
                            if (scrollYaxis < AxisDownsens)
                            {
                              //  PostKeyFunction(hwnd, 41, true);
                                if (scrolloutsidewindow == 3)
                                { //keep
                                    scrollYaxis = scrollYaxis - AxisDownsens; //zero input
                                    doscrollyes = true;
                                    Yscroll = scrollYaxis / scrollspeed3;
                                    didscroll = true;
                                }
                            }
                            else {
                                oldscrolldownaxis = false;
                                if (scrolloutsidewindow == 2)
                                    PostKeyFunction(hwnd, 41, false);
                                if (scrolloutsidewindow == 4)
                                    PostKeyFunction(hwnd, 11, false);
                            }
                        }
                        else if (scrollYaxis < AxisDownsens) //down
                        { //start
                            if (scrolloutsidewindow == 2)
                                PostKeyFunction(hwnd, 41, true);
                            if (scrolloutsidewindow == 4)
                                PostKeyFunction(hwnd, 11, true);
                            if (scrolloutsidewindow == 3 && doscrollyes == false)
                            {//start
                                tick = 0;
                                SendMouseClick(Xf, Yf, 8, 1); 
                                SendMouseClick(Xf, Yf, 5, 2); //4 skal vere 3
                            }
                            oldscrolldownaxis = true;
                        }





                        if (oldscrollupaxis)
                        {
                            if (scrollYaxis > AxisUpsens)
                            {
                               // PostKeyFunction(hwnd, 40, true);
                                if (scrolloutsidewindow == 3)
                                { //keep
									scrollYaxis = scrollYaxis - AxisUpsens; //zero input
                                    doscrollyes = true;
                                    
                                    Yscroll = scrollYaxis / scrollspeed3; //150
                                    didscroll = true;
                                }
                            }
                            else {
                                oldscrollupaxis = false;
                                if (scrolloutsidewindow == 2)
                                    PostKeyFunction(hwnd, 40, false);
                                if (scrolloutsidewindow == 4)
                                    PostKeyFunction(hwnd, 10, false);
                            }
                        }
                        else if (scrollYaxis > AxisUpsens) //up
                        {
                            if (scrolloutsidewindow == 2)
                                PostKeyFunction(hwnd, 40, true);
                            if (scrolloutsidewindow == 4)
                                PostKeyFunction(hwnd, 10, true);
                            if (scrolloutsidewindow == 3 && doscrollyes == false)
                            {//start
                                tick = 0;
                                SendMouseClick(Xf, Yf, 8, 1); 
                                SendMouseClick(Xf, Yf, 5, 2); //4 skal vere 3
                            }
                            oldscrollupaxis = true;
                        }   
                    }


                    //mouse left click and drag scrollfunction //scrolltype 3

                    if (doscrollyes) {
                        SendMouseClick(Xf + Xscroll, Yf - Yscroll, 8, 1); //4 skal vere 3
                        if (!didscroll && tick == scrollenddelay)
                        { 
							//MessageBox(NULL, "Scroll stopped", "Info", MB_OK | MB_ICONINFORMATION);
                            doscrollyes = false;
                            SendMouseClick(Xf, Yf, 6, 2); //4 skal vere 3
                        }
                    }



                    if (scrolloutsidewindow < 2 && scrollmap == false) //was 2
                    {
                        if (scrollXaxis < AxisLeftsens - 10000) //left
                        {
                            if (scrolloutsidewindow == 0)
                                scroll.x = rect.left + 1;
                            if (scrolloutsidewindow == 1)
                                scroll.x = rect.left - 1;
                            if (scrolloutsidewindow == 3)
                                scroll.x = (rect.left + (rect.right - rect.left) / 2) - 100;
                            scroll.y = rect.top + (rect.bottom - rect.top) / 2;

                            scrollmap = true;

                        }
                        else if (scrollXaxis > AxisRightsens + 10000) //right
                        {
                            if (scrolloutsidewindow == 0)
                                scroll.x = rect.right - 1;
                            if (scrolloutsidewindow == 1)
                                scroll.x = rect.right + 1;
                            if (scrolloutsidewindow == 3)
                                scroll.x = (rect.left + (rect.right - rect.left) / 2) + 100;
                            scroll.y = rect.top + (rect.bottom - rect.top) / 2;

                            scrollmap = true;

                        }
                        else if (scrollYaxis < AxisDownsens - 10000) //down
                        {
                            scroll.x = rect.left + (rect.right - rect.left) / 2;
                            if (scrolloutsidewindow == 0)
                                scroll.y = rect.bottom - 1;
                            if (scrolloutsidewindow == 1)
                                scroll.y = rect.bottom + 1;
                            if (scrolloutsidewindow == 3)
                                scroll.y = (rect.top + (rect.bottom - rect.top) / 2) + 100;
                            scrollmap = true;


                        }

                        else if (scrollYaxis > AxisUpsens + 10000) //up
                        {
                            scroll.x = rect.left + (rect.right - rect.left) / 2;
                            if (scrolloutsidewindow == 0)
                                scroll.y = rect.top + 1;
                            if (scrolloutsidewindow == 1)
                                scroll.y = rect.top - 1;
                            if (scrolloutsidewindow == 3)
                                scroll.y = (rect.top + (rect.bottom - rect.top) / 2) - 100;
                            scrollmap = true;
                        }   

                        else {
                            scrollmap = false;
                        }
                    }

                    delta = CalculateUltimateCursorMove(
                        Xaxis, Yaxis,
                        radial_deadzone, axial_deadzone, max_threshold,
                        curve_slope, curve_exponent,
                        sensitivity, accel_multiplier
                    );
                    if (delta.x != 0 || delta.y != 0) {
                        Xf += delta.x;
                        Yf += delta.y;
                        movedmouse = true;
                    }
                    
                    //may freeze cursor here. dont know why yet
                    if (Xf > width - 1) Xf = width - 1;
                    if (Yf > height - 1) Yf = height - 1;
                    if (Yf < 1) Yf = 1; 
                    if (Xf < 1) Xf = 1;

                    if (movedmouse == true) //fake cursor move message
                    {
                        if (userealmouse == 0)
                        {
                                SendMouseClick(Xf, Yf, 8, 1);
                        }

                    }

                if (leftPressed)
                { 

                    if (leftPressedold == false)
                    {
                     //save coordinates
                     startdrag.x = Xf;
                     startdrag.y = Yf;
                     leftPressedold = true;
                     rawmouseR = true;
                     if (userealmouse == 0 && scrolloutsidewindow == 3)
                     {
                         SendMouseClick(Xf, Yf, 5, 2); //4 skal vere 3
                         SendMouseClick(Xf, Yf, 6, 2); //double click
                     }
                     else if (userealmouse == 0)
                         SendMouseClick(Xf, Yf, 5, 2); //4 skal vere 3
                    }
                    
                }
                else if (leftPressedold)
                {
                    if (!leftPressed)
                    {
                        if (userealmouse == 0) {
                                SendMouseClick(Xf, Yf, 6, 2); //double click
                        }
                        else
                        {
                            if (abs(startdrag.x - Xf) <= 5)
                            {
                               
                                SendMouseClick(startdrag.x, startdrag.y, 2, 3); //4 4 move //5 release
                           
                            }
                            else
                            {
                               
                                SendMouseClick(startdrag.x, startdrag.y, 5, 2); //4 4 move //5 release
                                Sleep(30);
                                SendMouseClick(Xf, Yf, 8, 1);
                                Sleep(20);
                                SendMouseClick(Xf, Yf, 6, 2);
                            }
                        }
                        leftPressedold = false;
                        rawmouseR = false;
                    }   
                }
                if (rightPressed)
                {
                    if (rightPressedold == false)
                    {
                        //save coordinates
                        //start
                        if (hooksinited == false)
                            SetupHook();
                        startdrag.x = Xf;
                        startdrag.y = Yf;
                        rightPressedold = true;
                        rawmouseL = true;
                        if (userealmouse == 0)
                        {
                            DWORD currentTime = GetTickCount64();
                            if (currentTime - lastClickTime < GetDoubleClickTime() && doubleclicks == 1) //movedmouse?
                            {
                                SendMouseClick(Xf, Yf, 30, 2); //4 skal vere 3
                                
                            }
                            else 
                            {
                                SendMouseClick(Xf, Yf, 3, 2); //4 skal vere 3
                                
                            }
                            lastClickTime = currentTime;
                            
						}
                    }
                }
                else if (rightPressedold)
                {
                    if (!rightPressed)
                    {
                        if (userealmouse == 0) 
                            SendMouseClick(Xf, Yf, 4, 2);
                        else
                        { 
                            if (abs(startdrag.x - Xf) <= 5)
                            {
                             
                                SendMouseClick(startdrag.x, startdrag.y, 1, 3); //4 4 move //5 release
                            
                            }
                            else
                            {
                         
                                SendMouseClick(startdrag.x, startdrag.y, 3, 2); //4 4 move //5 release
                                Sleep(30);
                                SendMouseClick(Xf, Yf, 8, 1); //4 skal vere 3
                                Sleep(20);
                                SendMouseClick(Xf, Yf, 4, 2);
                            }
                        }
                        rightPressedold = false;
                        rawmouseL = false;
                    }
                } //if rightpress

                } //if mode above 0
            } //if controller
            else {
                showmessage = 12;
				//MessageBoxA(NULL, "Controller not connected", "Error", MB_OK | MB_ICONERROR);
            }


			//rendering. 3 methods. 1 direct draw. 2 partial on own window. 3 full redraw double buffer on own window
            if (drawfakecursor == 1)
                GetGameHDCAndCallDraw(hwnd); //calls DrawToHdc in here
            else if (drawfakecursor == 2)
                {
                if (scanoption)
                    DblBufferAndCallDraw(PointerWnd, Xf, Yf, showmessage); //full redraw
				else if (movedmouse) DrawToHDC(PointerWnd, Xf, Yf, showmessage); //partial, much faster. may not erase correctly on staticpoints and messages yet
                }
            else if (drawfakecursor == 3)
                DblBufferAndCallDraw(PointerWnd, Xf, Yf, showmessage); //full redraw


        } // if hwnd
        if (showmessage != 0 && showmessage != 12)
        {
            counter++;
            if (counter > 500)
            {
                if (showmessage == 1) {
                    mode = 0;
                    showmessage = 0;
                }
                if (showmessage == 69) { //disabling dll
                    onoroff = false;
                    MH_DisableHook(MH_ALL_HOOKS);
                }
                if (showmessage == 70) { //enabling dll
                    onoroff = true;
                    MH_EnableHook(MH_ALL_HOOKS);
                }
                if (mode != 3)
                    showmessage = 0;
                counter = 0;
            }
        }
      //  if (pointerWindow) {
      //      ReleaseDC(pointerWindow, PointerWnd);
      //  }
		if (rawinputhook == 1) //to make game poll rawinput
        { 
            INPUT input = { 0 }; 
            input.type = INPUT_MOUSE;
            input.mi.dx = 0;
            input.mi.dy = 0;
            input.mi.dwFlags = MOUSEEVENTF_MOVE; // relative move
            SendInput(1, &input, sizeof(INPUT));
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = VK_OEM_PERIOD;
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));
        }
        if (tick < scrollenddelay)
            tick++;
        
        if (mode == 0)
            Sleep(10);
        if (mode > 0) {
            Sleep(1 + responsetime);
        }
        if (showmessage == 99)
            Sleep(18); //30 fps?
    } //loop end but endless
    return;
}
void RemoveHook() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) 
{
    switch (ul_reason_for_call) 
    {
        case DLL_PROCESS_ATTACH:
        {
            g_hModule = hModule;

            std::string iniPath = UGetExecutableFolder() + "\\Xinput.ini";
            std::string iniSettings = "Hooks";

             //hook settings
            clipcursorhook = GetPrivateProfileInt(iniSettings.c_str(), "ClipCursorHook", 1, iniPath.c_str());
            getkeystatehook = GetPrivateProfileInt(iniSettings.c_str(), "GetKeystateHook", 1, iniPath.c_str());
            getasynckeystatehook = GetPrivateProfileInt(iniSettings.c_str(), "GetAsynckeystateHook", 1, iniPath.c_str());
            getcursorposhook = GetPrivateProfileInt(iniSettings.c_str(), "GetCursorposHook", 1, iniPath.c_str());
            setcursorposhook = GetPrivateProfileInt(iniSettings.c_str(), "SetCursorposHook", 1, iniPath.c_str());
            setcursorhook = GetPrivateProfileInt(iniSettings.c_str(), "SetCursorHook", 1, iniPath.c_str()); 
            rawinputhook = GetPrivateProfileInt(iniSettings.c_str(), "RawInputHook", 1, iniPath.c_str());
            GetCursorInfoHook = GetPrivateProfileInt(iniSettings.c_str(), "GetCursorInfoHook", 0, iniPath.c_str()); 
            GetKeyboardStateHook = GetPrivateProfileInt(iniSettings.c_str(), "GetKeyboardStateHook", 1, iniPath.c_str()); 
            setrecthook = GetPrivateProfileInt(iniSettings.c_str(), "SetRectHook", 0, iniPath.c_str()); 
            leftrect = GetPrivateProfileInt(iniSettings.c_str(), "SetRectLeft", 0, iniPath.c_str());
            toprect = GetPrivateProfileInt(iniSettings.c_str(), "SetRectTop", 0, iniPath.c_str());
            rightrect = GetPrivateProfileInt(iniSettings.c_str(), "SetRectRight", 800, iniPath.c_str());
            bottomrect = GetPrivateProfileInt(iniSettings.c_str(), "SetRectBottom", 600, iniPath.c_str());
            registerrawinputhook = GetPrivateProfileInt(iniSettings.c_str(), "RegisterRawInputHook", 1, iniPath.c_str());

            // [MessageFilter]
            g_filterRawInput = GetPrivateProfileInt(iniSettings.c_str(), "RawInputFilter", 0, iniPath.c_str());
            g_filterMouseMove = GetPrivateProfileInt(iniSettings.c_str(), "MouseMoveFilter", 0, iniPath.c_str());
            g_filterMouseActivate = GetPrivateProfileInt(iniSettings.c_str(), "MouseActivateFilter", 0, iniPath.c_str());
            g_filterWindowActivate = GetPrivateProfileInt(iniSettings.c_str(), "WindowActivateFilter", 0, iniPath.c_str());
            g_filterWindowActivateApp = GetPrivateProfileInt(iniSettings.c_str(), "WindowActivateAppFilter", 0, iniPath.c_str());
            g_filterMouseWheel = GetPrivateProfileInt(iniSettings.c_str(), "MouseWheelFilter", 0, iniPath.c_str());
            g_filterMouseButton = GetPrivateProfileInt(iniSettings.c_str(), "MouseButtonFilter", 0, iniPath.c_str());
            g_filterKeyboardButton = GetPrivateProfileInt(iniSettings.c_str(), "KeyboardButtonFilter", 0, iniPath.c_str());

            //hook at once or wait for input
            hooksoninit = GetPrivateProfileInt(iniSettings.c_str(), "hooksoninit", 1, iniPath.c_str());
			if (hooksoninit == 1) //if 2 then just after window found
            {
                SetupHook();
		    }
           InitializeCriticalSection(&critical);
           std::thread one (ThreadFunction, g_hModule);
           one.detach();
			//CloseHandle(one);
           break;
        }
        case DLL_PROCESS_DETACH:
        {
            DeleteCriticalSection(&critical);
            RemoveHook();
            exit(0);
            break;
        }
    }
    return true;
}