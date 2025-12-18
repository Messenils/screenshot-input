#pragma once

extern BOOL(WINAPI* fpGetMessageA)(LPMSG, HWND, UINT, UINT);
extern BOOL(WINAPI* fpGetMessageW)(LPMSG, HWND, UINT, UINT);
extern BOOL(WINAPI* fpPeekMessageA)(LPMSG, HWND, UINT, UINT, UINT);
extern BOOL(WINAPI* fpPeekMessageW)(LPMSG, HWND, UINT, UINT, UINT);

namespace ScreenshotInput
{
	class MessageFilterHook
	{

	public:
		static BOOL WINAPI Hook_GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
		static BOOL WINAPI Hook_GetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
		static BOOL WINAPI Hook_PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
		static BOOL WINAPI Hook_PeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);

		static bool IsKeyboardButtonFilterEnabled();
	};

}