/*
This file is imported from ProtoInput project, created by Ilyaki https://github.com/Ilyaki/ProtoInput
And is modified for ScreenshotInput usage. All credits to the original author.
*/

#include "pch.h"
#include <type_traits>
#include "MessageFilterHooks.h"
#include "MessageFilters.h"

// External filters variables
extern bool g_filterRawInput;
extern bool g_filterMouseMove;
extern bool g_filterMouseActivate;
extern bool g_filterWindowActivate;
extern bool g_filterWindowActivateApp;
extern bool g_filterMouseWheel;
extern bool g_filterMouseButton;
extern bool g_filterKeyboardButton;

// External function pointers (defined in dllmain.h/cpp)
extern BOOL(WINAPI* fpGetMessageA)(LPMSG, HWND, UINT, UINT);
extern BOOL(WINAPI* fpGetMessageW)(LPMSG, HWND, UINT, UINT);
extern BOOL(WINAPI* fpPeekMessageA)(LPMSG, HWND, UINT, UINT, UINT);
extern BOOL(WINAPI* fpPeekMessageW)(LPMSG, HWND, UINT, UINT, UINT);

namespace ScreenshotInput
{
	template<typename T>
	bool* IsFilterEnabled()
	{
		if constexpr (std::is_same_v<T, RawInputFilter>) return &g_filterRawInput;
		if constexpr (std::is_same_v<T, MouseMoveFilter>) return &g_filterMouseMove;
		if constexpr (std::is_same_v<T, MouseActivateFilter>) return &g_filterMouseActivate;
		if constexpr (std::is_same_v<T, WindowActivateFilter>) return &g_filterWindowActivate;
		if constexpr (std::is_same_v<T, WindowActivateAppFilter>) return &g_filterWindowActivateApp;
		if constexpr (std::is_same_v<T, MouseWheelFilter>) return &g_filterMouseWheel;
		if constexpr (std::is_same_v<T, MouseButtonFilter>) return &g_filterMouseButton;
		if constexpr (std::is_same_v<T, KeyboardButtonFilter>) return &g_filterKeyboardButton;
		static bool dummy = false; return &dummy;
	}

	template<typename X, typename... T>
	bool MessageFilterAllow(unsigned int message, unsigned int* lparam, unsigned int* wparam, intptr_t hwnd)
	{
		bool allow = true;
		if (*IsFilterEnabled<X>()) {
			if (message >= X::MessageMin() && message <= X::MessageMax()) {
				allow = X::Filter(message, lparam, wparam, hwnd);
			}
		}

		if constexpr (sizeof...(T) != 0) {
			return allow && MessageFilterAllow<T...>(message, lparam, wparam, hwnd);
		}
		else {
			return allow;
		}
	}

	// ---------------------------------------------------------
	// NEW: Improved FilterMessage to handle PM_NOREMOVE safety
	// ---------------------------------------------------------
	inline BOOL FilterMessage(MSG* lpMsg, bool remove)
	{
		if (!MessageFilterAllow<ScreenshotInput_MESSAGE_FILTERS>(lpMsg->message, (unsigned int*)&lpMsg->lParam, (unsigned int*)&lpMsg->wParam, (intptr_t)lpMsg->hwnd))
		{
			// Blocked!
			// Only zero out the message if we are removing it, otherwise we leave it
			// for the PeekMessage hook to handle (force-remove).
			if (remove)
			{
				lpMsg->message = WM_NULL;
				lpMsg->lParam = 0;
				lpMsg->wParam = 0;
			}
			return FALSE; // Return FALSE to indicate blocked
		}
		return TRUE; // Allowed
	}

	BOOL WINAPI MessageFilterHook::Hook_GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
	{
		const auto ret = fpGetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
		if (ret == -1 || ret == 0) return ret;

		// GetMessage always removes
		if (FilterMessage(lpMsg, true) == FALSE) return TRUE;
		return ret;
	}

	BOOL WINAPI MessageFilterHook::Hook_GetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
	{
		const auto ret = fpGetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
		if (ret == -1 || ret == 0) return ret;

		if (FilterMessage(lpMsg, true) == FALSE) return TRUE;
		return ret;
	}

	// ---------------------------------------------------------
	// FIXED: PeekMessage Hooks with Anti-Freeze Logic
	// ---------------------------------------------------------
	BOOL WINAPI MessageFilterHook::Hook_PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
	{
		BOOL ret = fpPeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);

		if (ret && lpMsg->message != WM_NULL)
		{
			bool remove = (wRemoveMsg & PM_REMOVE);
			if (FilterMessage(lpMsg, remove) == FALSE)
			{
				// Message blocked. 
				// If game used PM_NOREMOVE, the bad message is still in the queue.
				// We must force-remove it to prevent infinite loop.
				if (!remove)
				{
					MSG dummy;
					fpPeekMessageA(&dummy, hWnd, wMsgFilterMin, wMsgFilterMax, PM_REMOVE);
				}
				return FALSE; // Pretend we found nothing
			}
		}
		return ret;
	}

	BOOL WINAPI MessageFilterHook::Hook_PeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
	{
		BOOL ret = fpPeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);

		if (ret && lpMsg->message != WM_NULL)
		{
			bool remove = (wRemoveMsg & PM_REMOVE);
			if (FilterMessage(lpMsg, remove) == FALSE)
			{
				if (!remove)
				{
					MSG dummy;
					fpPeekMessageW(&dummy, hWnd, wMsgFilterMin, wMsgFilterMax, PM_REMOVE);
				}
				return FALSE;
			}
		}
		return ret;
	}

	bool MessageFilterHook::IsKeyboardButtonFilterEnabled()
	{
		return *IsFilterEnabled<KeyboardButtonFilter>();
	}
}