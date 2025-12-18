#pragma once
#include <WinUser.h>
#include "MessageFilterBase.h"
#include "ProtoMessageFilterIDs.h"
//#include "dllmain.h"

extern int Xf; // ScreenshotInput mouse X position
extern int Yf; // ScreenshotInput mouse Y position
extern const WPARAM ScreenshotInput_MOUSE_SIGNATURE;
extern const LPARAM ScreenshotInput_KEYBOARD_SIGNATURE;

namespace ScreenshotInput
{
#define ScreenshotInput_MESSAGE_FILTERS \
RawInputFilter, \
MouseMoveFilter, \
MouseActivateFilter, \
WindowActivateFilter, \
WindowActivateAppFilter, \
MouseWheelFilter, \
MouseButtonFilter, \
KeyboardButtonFilter

    class RawInputFilter : public ScreenshotInput::MessageFilterBase<RawInputFilterID, WM_INPUT> {
    public:
        static bool Filter(unsigned int message, unsigned int* lparam, unsigned int* wparam, intptr_t hwnd) {
            return false;
        }
    };

    class MouseMoveFilter : public ScreenshotInput::MessageFilterBase<MouseMoveFilterID, WM_MOUSEMOVE> {
    public:
        static bool Filter(unsigned int message, unsigned int* lparam, unsigned int* wparam, intptr_t hwnd) {
            *lparam = MAKELPARAM(Xf, Yf); // Use ScreenshotInput's mouse position.
            return true;
        }
    };

    class MouseActivateFilter : public ScreenshotInput::MessageFilterBase<MouseActivateFilterID, WM_MOUSEACTIVATE> {
    public:
        static bool Filter(unsigned int message, unsigned int* lparam, unsigned int* wparam, intptr_t hwnd) {
            return MA_ACTIVATE;
        }
    };

    class WindowActivateFilter : public ScreenshotInput::MessageFilterBase<WindowActivateFilterID, WM_ACTIVATE> {
    public:
        static bool Filter(unsigned int message, unsigned int* lparam, unsigned int* wparam, intptr_t hwnd) {
            *wparam = WA_ACTIVE;
            *lparam = 0;
            return true;
        }
    };

    class WindowActivateAppFilter : public ScreenshotInput::MessageFilterBase<WindowActivateAppFilterID, WM_ACTIVATEAPP> {
    public:
        static bool Filter(unsigned int message, unsigned int* lparam, unsigned int* wparam, intptr_t hwnd) {
            *wparam = TRUE;
            *lparam = 0;
            return true;
        }
    };

    class MouseWheelFilter : public ScreenshotInput::MessageFilterBase<MouseWheelFilterID, WM_MOUSEWHEEL, WM_MOUSEHWHEEL> {
    public:
        static bool Filter(unsigned int message, unsigned int* lparam, unsigned int* wparam, intptr_t hwnd) {
            if ((*wparam & ScreenshotInput_MOUSE_SIGNATURE) != 0) {
                *wparam &= ~ScreenshotInput_MOUSE_SIGNATURE;
                return true;
            }
            return false;
        }
    };

    class MouseButtonFilter : public ScreenshotInput::MessageFilterBase<MouseButtonFilterID, WM_LBUTTONDOWN, WM_XBUTTONUP> {
    public:
        static bool Filter(unsigned int message, unsigned int* lparam, unsigned int* wparam, intptr_t hwnd) {
            if ((*wparam & ScreenshotInput_MOUSE_SIGNATURE) != 0) {
                *wparam &= ~ScreenshotInput_MOUSE_SIGNATURE;
                return true;
            }
            return false;
        }
    };

    class KeyboardButtonFilter : public ScreenshotInput::MessageFilterBase<KeyboardButtonFilterID, WM_KEYDOWN, WM_SYSKEYUP> {
    public:
        static bool Filter(unsigned int message, unsigned int* lparam, unsigned int* wparam, intptr_t hwnd) {
            if ((*lparam & ScreenshotInput_KEYBOARD_SIGNATURE) != 0) {
                *lparam &= ~ScreenshotInput_KEYBOARD_SIGNATURE;
                return true;
            }
            return false;
        }
    };

}