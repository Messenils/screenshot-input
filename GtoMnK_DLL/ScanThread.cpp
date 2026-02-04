

#include <algorithm>
#include "pch.h"
#include <cmath>
#define NOMINMAX
#include <windows.h>
//#include <windowsx.h>
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
#include "input.h"
#include "MainThread.h"
#include "Mouse.h"
#include "logging.h"
#include "ScanThread.h"
#include "INISettings.h"
#include "FakeCursor.h"

#pragma comment(lib, "dwmapi.lib")




namespace GtoMnK {

	    //public
       // extern int drawfakecursor;
        int ScanThread::Aisstatic, ScanThread::Bisstatic, ScanThread::Xisstatic, ScanThread::Yisstatic;
        
        int ScanThread::numphotoA, ScanThread::numphotoB, ScanThread::numphotoX, ScanThread::numphotoY;
        int ScanThread::numphotoC, ScanThread::numphotoD, ScanThread::numphotoE, ScanThread::numphotoF;
        int ScanThread::numphotoAbmps, ScanThread::numphotoBbmps, ScanThread::numphotoXbmps, ScanThread::numphotoYbmps;
        int ScanThread::startsearchA, ScanThread::startsearchB, ScanThread::startsearchX, ScanThread::startsearchY;
        int ScanThread::startsearchC, ScanThread::startsearchD, ScanThread::startsearchE, ScanThread::startsearchF;
        POINT ScanThread::PointA, ScanThread::PointB, ScanThread::PointX, ScanThread::PointY;
	    CRITICAL_SECTION ScanThread::critical;
		std::vector<POINT> ScanThread::staticPointA, ScanThread::staticPointB, ScanThread::staticPointX, ScanThread::staticPointY;
        int ScanThread::scanAtype, ScanThread::scanBtype, ScanThread::scanXtype, ScanThread::scanYtype;
        int ScanThread::Ctype, ScanThread::Dtype, ScanThread::Etype, ScanThread::Ftype;
        bool ScanThread::scanloop = true;
        int ScanThread::scanoption;
        int ScanThread::ignorerect;
        int ScanThread::ShoulderNextBMP;
        int ScanThread::resize = 1; //support scaling

        //private
		bool PreScanningEnabled = false;
        bool Astatic;
        bool Bstatic;
        bool Xstatic;
        bool Ystatic;
        HBITMAP hbm;
        std::vector<BYTE> largePixels, smallPixels;
        SIZE screenSize;
        int strideLarge, strideSmall;
        int smallW, smallH;

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

        POINT windowpos(HWND window, int ignorerect, bool getpos)
        {

            if (ignorerect == 0)
            {
                POINT pos = { 0,0 };
                RECT recte;
                GetClientRect(window, &recte);
                //return pos if true. or res is false
                if (getpos) {
                    pos.x = recte.left;
                    pos.y = recte.top;
                    ClientToScreen(window, &pos);
                }
                else {
                    pos.x = recte.right; //- recte.left;
                    pos.y = recte.bottom; //- recte.top;
                }
                return pos;
            }

            else //different way to get window size/pos with DWM
            {
                POINT pos = { 0,0 };
                RECT frameBounds;
                HRESULT hr = DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &frameBounds, sizeof(frameBounds));
                if (SUCCEEDED(hr))
                {
                    //return pos if true. or res is false
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

        int CalculateStride(int width) {
            return ((width * 3 + 3) & ~3);
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

        //calling fuction in critical lock
        void BmpInputAction(int X, int Y, int type) //moveclickorboth
        { 

            if (type == 0) //click and move
            {
                Mouse::Xf = X;
                Mouse::Yf = Y;
                Input::SendMouseMove(X, Y);
                FakeCursor::NotifyUpdatedCursorPosition();
                Sleep(5);
                Input::SendAction("-1" , true);
                Sleep(5);
                Input::SendAction("-1", false);
                
            }
            else if (type == 1) //only move
            {
                Mouse::Xf = X;
                Mouse::Yf = Y;

                Input::SendMouseMove(X, Y);
                FakeCursor::NotifyUpdatedCursorPosition();
            }
            else if (type == 2) //only click
            {
				int Xhold = Mouse::Xf;
                int Yhold = Mouse::Yf;
                Mouse::Xf = X;
                Mouse::Yf = Y;
                Input::SendMouseMove(X, Y);
                FakeCursor::NotifyUpdatedCursorPosition();
                Sleep(5);
                Input::SendAction("-1", true);
                Sleep(5);
                Input::SendAction("-1", false);
                Sleep(5);
                Input::SendMouseMove(Mouse::Xf, Mouse::Yf);
                Mouse::Xf = Xhold;
                Mouse::Yf = Yhold;
                FakeCursor::NotifyUpdatedCursorPosition();
            }
        }

        void Bmpfound(const char key[3], int X, int Y, int i, bool onlysearch, bool found, int store)
        {
            int input = 0;
            if (strcmp(key, "\\A") == 0)
            {
                if (found)
                {

                    EnterCriticalSection(&ScanThread::critical);
                    FakeCursor::NotifyUpdatedCursorPosition();
                    LeaveCriticalSection(&ScanThread::critical);
                    if (onlysearch)
                    {
                        EnterCriticalSection(&GtoMnK::ScanThread::critical);
                        ScanThread::startsearchA = i;
                        input = ScanThread::scanAtype;
                        ScanThread::staticPointA[i].x = X;
                        ScanThread::staticPointA[i].y = Y;
                        ScanThread::PointA.x = X;
                        ScanThread::PointA.y = Y;
                        LeaveCriticalSection(&GtoMnK::ScanThread::critical);
                    }
                    else
                    {
                        input = ScanThread::scanAtype;
                        if (store) {
                            ScanThread::staticPointA[i].x = X;
                            ScanThread::staticPointA[i].y = Y;

                        }
                        if (ScanThread::startsearchA < ScanThread::numphotoA - 1)
                            ScanThread::startsearchA = i + 1;
                        else ScanThread::startsearchA = 0;
                    }
                }
                else
                {
                    if (onlysearch)
                    {
                        EnterCriticalSection(&ScanThread::critical);
                        ScanThread::startsearchA = 0;
                        ScanThread::PointA.x = 0;
                        ScanThread::PointA.y = 0;
                        LeaveCriticalSection(&ScanThread::critical);
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
                        EnterCriticalSection(&ScanThread::critical);
                        ScanThread::startsearchB = i;
                        ScanThread::PointB.x = X;
                        ScanThread::staticPointB[i].x = X;
                        ScanThread::staticPointB[i].y = Y;
                        ScanThread::PointB.y = Y;
                        LeaveCriticalSection(&ScanThread::critical); 
                    }
                    else
                    {
                        input = ScanThread::scanBtype;
                        if (store) {
                            ScanThread::staticPointB[i].x = X;
                            ScanThread::staticPointB[i].y = Y;
                        }
                        if (ScanThread::startsearchB < ScanThread::numphotoB - 1)
                            ScanThread::startsearchB = i + 1;
                        else ScanThread::startsearchB = 0;
                    }
                }
                else
                {
                    if (onlysearch)
                    {
                        EnterCriticalSection(&GtoMnK::ScanThread::critical);
                        ScanThread::startsearchB = 0;
                        ScanThread::PointB.x = 0;
                        ScanThread::PointB.y = 0;
                        LeaveCriticalSection(&GtoMnK::ScanThread::critical);
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
                        EnterCriticalSection(&GtoMnK::ScanThread::critical);
                        ScanThread::startsearchX = i;
                        ScanThread::PointX.x = X;
                        ScanThread::PointX.y = Y;
                        ScanThread::staticPointX[i].x = X;
                        ScanThread::staticPointX[i].y = Y;
                        LeaveCriticalSection(&GtoMnK::ScanThread::critical);
                    }
                    else
                    {
                        input = ScanThread::scanXtype;
                        ScanThread::startsearchX = i + 1;
                        if (store) {
                            ScanThread::staticPointX[i].x = X;
                            ScanThread::staticPointX[i].y = Y;
                        }
                        if (ScanThread::startsearchX < ScanThread::numphotoX - 1)
                            ScanThread::startsearchX = i + 1;
                        else ScanThread::startsearchX = 0;
                    }
                }
                else
                {
                    if (onlysearch)
                    {
                        EnterCriticalSection(&GtoMnK::ScanThread::critical);
                        ScanThread::startsearchX = 0;
                        ScanThread::PointX.x = 0;
                        ScanThread::PointX.y = 0;
                        LeaveCriticalSection(&GtoMnK::ScanThread::critical);
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
                        EnterCriticalSection(&GtoMnK::ScanThread::critical);
                        ScanThread::startsearchX = i;
                        ScanThread::staticPointY[i].x = X;
                        ScanThread::staticPointY[i].y = Y;
                        ScanThread::PointY.x = X;
                        ScanThread::PointY.y = Y;
                        LeaveCriticalSection(&GtoMnK::ScanThread::critical);
                    }
                    else
                    {
                        input = ScanThread::scanYtype;
                        ScanThread::startsearchY = i + 1;
                        if (store) {
                            ScanThread::staticPointY[i].x = X;
                            ScanThread::staticPointY[i].y = Y;
                        }
                        if (ScanThread::startsearchY < ScanThread::numphotoY - 1)
                            ScanThread::startsearchY = i + 1;
                        else ScanThread::startsearchY = 0;
                    }
                }
                else
                {
                    if (onlysearch)
                    {
                        EnterCriticalSection(&GtoMnK::ScanThread::critical);
                        ScanThread::startsearchY = 0;
                        //input = scanYtype;
                        ScanThread::PointY.x = 0;
                        ScanThread::PointY.y = 0;
                        LeaveCriticalSection(&GtoMnK::ScanThread::critical);
                    }
                }
            }
            if (strcmp(key, "\\C") == 0)
            {
                if (found && !onlysearch)
                {
                    ScanThread::startsearchC = i + 1;
                    input = ScanThread::Ctype;
                }
            }
            if (strcmp(key, "\\D") == 0)
            {
                if (found && !onlysearch)
                {
                    ScanThread::startsearchD = i + 1;
                    input = ScanThread::Dtype;
                }
            }
            if (strcmp(key, "\\E") == 0)
            {
                if (found && !onlysearch)
                {
                    ScanThread::startsearchE = i + 1;
                    input = ScanThread::Etype;
                }
            }
            if (strcmp(key, "\\F") == 0)
            {
                if (found && !onlysearch)
                {
                    ScanThread::startsearchF = i + 1;
                    input = ScanThread::Ftype;
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


        POINT GetStaticFactor(POINT pp, int doscale, bool isnotbmp)
        {
             FLOAT ny;
            POINT currentres = windowpos(hwnd, ScanThread::ignorerect, false);
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

        bool CaptureWindow24Bit(HWND hwnd, SIZE& capturedwindow, std::vector<BYTE>& pixels, int& strideOut, bool draw, bool stretchblt)
        {
            if (PreScanningEnabled && drawfakecursor == 1)
                EnterCriticalSection(&ScanThread::critical);
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
            HBITMAP hbm24 = CreateDIBSection(hdcWindow, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
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

            if (PreScanningEnabled && drawfakecursor == 1)
                LeaveCriticalSection(&ScanThread::critical);
            return true;
        } //function end

        POINT CheckStatics(const char abc[3], int numtocheck)
        {
            POINT newpoint{ 0,0 };
            if (strcmp(abc, "\\A") == 0)
            {
                if (ScanThread::staticPointA[numtocheck].x != 0)
                {
                    // 
                    newpoint.x = ScanThread::staticPointA[numtocheck].x;
                    newpoint.y = ScanThread::staticPointA[numtocheck].y;
                }
            }
            if (strcmp(abc, "\\B") == 0)
            {
                if (ScanThread::staticPointB[numtocheck].x != 0)
                {
                    newpoint.x = ScanThread::staticPointB[numtocheck].x;
                    newpoint.y = ScanThread::staticPointB[numtocheck].y;
                }
            }
            if (strcmp(abc, "\\X") == 0)
            {
                if (ScanThread::staticPointX[numtocheck].x != 0)
                {
                    newpoint.x = ScanThread::staticPointX[numtocheck].x;
                    newpoint.y = ScanThread::staticPointX[numtocheck].y;
                }
            }
            if (strcmp(abc, "\\Y") == 0)
            {
                if (ScanThread::staticPointY[numtocheck].x != 0)
                {
                    newpoint.x = ScanThread::staticPointY[numtocheck].x;
                    newpoint.y = ScanThread::staticPointY[numtocheck].y;
                }
            }
            return newpoint;
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
                else {
                    start = x;
                }
            }

            //searching statics
            x = 0;
            int inistart = -1;
            while (x < 50 && inistart == -1)
            {
                std::string iniPath = UGetExecutableFolder() + "\\GtoMnK.ini";
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
        bool ScanThread::initovector()
        {
            std::string iniPath = UGetExecutableFolder() + "\\GtoMnK.ini";
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
                    ScanThread::staticPointA[x + ScanThread::numphotoAbmps].y = inipoint.y;
                    ScanThread::staticPointA[x + ScanThread::numphotoAbmps].x = inipoint.x;
                }
                else y = 10;// break;
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
                    ScanThread::staticPointB[x + ScanThread::numphotoBbmps].y = inipoint.y;
                    ScanThread::staticPointB[x + ScanThread::numphotoBbmps].x = inipoint.x;
                }

                else y = 10;// break;
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
                    ScanThread::staticPointX[x + ScanThread::numphotoXbmps].y = inipoint.y;
                    ScanThread::staticPointX[x + ScanThread::numphotoXbmps].x = inipoint.x;

                }

                else y = 10;// break;
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
                    ScanThread::staticPointY[x + ScanThread::numphotoYbmps].y = inipoint.y;
                    ScanThread::staticPointY[x + ScanThread::numphotoYbmps].x = inipoint.x;
                }
                else y = 10; // break;
            }
            if (test == true)
                return true;
            else return false; //no points
        }

        bool ScanThread::enumeratebmps()
        {
            std::wstring path = WGetExecutableFolder() + L"\\A";
            ScanThread::numphotoA = HowManyBmps(path, true);
            ScanThread::numphotoAbmps = HowManyBmps(path, false);
            path = WGetExecutableFolder() + L"\\B";
            ScanThread::numphotoB = HowManyBmps(path, true);
            ScanThread::numphotoBbmps = HowManyBmps(path, false);
            path = WGetExecutableFolder() + L"\\X";
            ScanThread::numphotoX = HowManyBmps(path, true);
            ScanThread::numphotoXbmps = HowManyBmps(path, false);
            path = WGetExecutableFolder() + L"\\Y";
            ScanThread::numphotoY = HowManyBmps(path, true);
            ScanThread::numphotoYbmps = HowManyBmps(path, false);
            path = WGetExecutableFolder() + L"\\C";
            ScanThread::numphotoC = HowManyBmps(path, false);
            path = WGetExecutableFolder() + L"\\D";
            ScanThread::numphotoD = HowManyBmps(path, false);
            path = WGetExecutableFolder() + L"\\E";
            ScanThread::numphotoE = HowManyBmps(path, false);
            path = WGetExecutableFolder() + L"\\F";
            ScanThread::numphotoF = HowManyBmps(path, false);

            if (ScanThread::numphotoA < 0 && ScanThread::numphotoB < 0 && ScanThread::numphotoX < 0 && ScanThread::numphotoY < 0)
            {
                return false;
            }
            return true;
        }
        bool ButtonScanAction(const char key[3], int mode, int serchnum, int startsearch, bool onlysearch, POINT currentpoint, int checkarray)
        {
            bool found = false;
            POINT pt = { 0,0 };
            POINT noeder = { 0,0 };
            int numbmp = 0;
            if (mode != 2)
            {
                int numphoto = 0;
                if (checkarray == 1)
                { //always check static first?
                    noeder = CheckStatics(key, startsearch);
                    if (noeder.x != 0)
                    {
                        Bmpfound(key, noeder.x, noeder.y, startsearch, onlysearch, true, checkarray); //or not found
                        found = true;
                        return true;
                    }
                }
                if (!found)
                {
                    for (int i = startsearch; i < serchnum; i++)
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
                        std::string path = UGetExecutableFolder() + key + std::to_string(i) + ".bmp";
                        std::wstring wpath(path.begin(), path.end());
                        // HDC soke;
                        if (LoadBMP24Bit(wpath.c_str(), smallPixels, smallW, smallH, strideSmall) && !found)
                        {
                            if (CaptureWindow24Bit(hwnd, screenSize, largePixels, strideLarge, false, ScanThread::resize))
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
                    for (int i = 0; i < serchnum; i++) 
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
                        std::string path = UGetExecutableFolder() + key + std::to_string(i) + ".bmp";
                        std::wstring wpath(path.begin(), path.end());
                        if (LoadBMP24Bit(wpath.c_str(), smallPixels, smallW, smallH, strideSmall) && !found)
                        {
                            //ShowMemoryUsageMessageBox();
                            if (CaptureWindow24Bit(hwnd, screenSize, largePixels, strideLarge, false, ScanThread::resize))

                            {// MessageBox(NULL, "some kind of error", "captured desktop", MB_OK | MB_ICONINFORMATION);
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
                Bmpfound(key, pt.x, pt.y, numphoto, onlysearch, found, checkarray); //or not found
                if (found == true)
                    return true;
                else return false;
            }

            if (mode == 2 && MainThread::showmessage == 0 && onlysearch == false) //mode 2 button mapping //showmessage var to make sure no double map or map while message
            {
                Sleep(100); //to make sure red flicker expired
                std::string path = UGetExecutableFolder() + key + std::to_string(serchnum) + ".bmp";
                std::wstring wpath(path.begin(), path.end());
                SaveWindow10x10BMP(hwnd, wpath.c_str(), Mouse::Xf, Mouse::Yf);
               // MainThread::showmessage = 10; //signal is saving
                return true;
            }
           // else if (MainThread::showmessage != 0)
               // MainThread::showmessage = 11;//wait for mesage expire
            return true;
        }

        DWORD WINAPI ScanThreadMain(LPVOID, int Aisstatic, int Bisstatic, int Xisstatic, int Yisstatic)
        {
            int scantick = 0;
            Sleep(3000);
            Astatic = Aisstatic;
            Bstatic = Bisstatic;
            Xstatic = Xisstatic;
            Ystatic = Yisstatic;
            //scanrunning = true;
            ScanThread::scanloop = true;
            while (ScanThread::scanloop)
            {
                EnterCriticalSection(&ScanThread::critical);
                POINT Apos = { ScanThread::PointA.x, ScanThread::PointA.y };
                POINT Bpos = { ScanThread::PointB.x, ScanThread::PointB.y };
                POINT Xpos = { ScanThread::PointX.x, ScanThread::PointX.y };
                POINT Ypos = { ScanThread::PointY.x, ScanThread::PointY.y };
                int startsearchAW = ScanThread::startsearchA;
                int startsearchBW = ScanThread::startsearchB;
                int startsearchXW = ScanThread::startsearchX;
                int startsearchYW = ScanThread::startsearchY;
                POINT PointAW = ScanThread::PointA;
                POINT PointBW = ScanThread::PointB;
                POINT PointXW = ScanThread::PointX;
                POINT PointYW = ScanThread::PointY;
                LeaveCriticalSection(&ScanThread::critical);

                if (scantick < 3)
                    scantick++;
                else scantick = 0;
                if (PreScanningEnabled)
                {
                    if (scantick == 0 && ScanThread::numphotoA > 0)
                    {
                        if (Astatic == 1)
                        {
                            if (ScanThread::staticPointA[startsearchAW].x == 0 && ScanThread::staticPointA[startsearchAW].y == 0)
                                ButtonScanAction("\\A", 1, ScanThread::numphotoA, startsearchAW, true, PointAW, Astatic);
                            else
                                Bmpfound("\\A", ScanThread::staticPointA[startsearchAW].x, ScanThread::staticPointA[startsearchAW].y, startsearchAW, true, true, Astatic);
                        }
                        else ButtonScanAction("\\A", 1, ScanThread::numphotoA, startsearchAW, true, PointAW, Astatic);
                    }

                    if (scantick == 1 && ScanThread::numphotoB > 0)
                    {
                        if (Bstatic == 1)
                        {
                            if (ScanThread::staticPointB[startsearchBW].x == 0 && ScanThread::staticPointB[startsearchBW].y == 0)
                                //MessageBoxA(NULL, "heisann", "A", MB_OK);
                                ButtonScanAction("\\B", 1, ScanThread::numphotoB, startsearchBW, true, PointAW, Bstatic);
                            else
                                Bmpfound("\\B", ScanThread::staticPointB[startsearchBW].x, ScanThread::staticPointB[startsearchBW].y, startsearchBW, true, true, Bstatic);
                        }
                        else ButtonScanAction("\\B", 1, ScanThread::numphotoB, startsearchBW, true, PointBW, Bstatic);

                    }
                    if (scantick == 2 && ScanThread::numphotoX > 0)
                    {
                        if (Xstatic == 1)
                        {
                            if (ScanThread::staticPointX[startsearchXW].x == 0 && ScanThread::staticPointX[startsearchXW].y == 0)
                                ButtonScanAction("\\X", 1, ScanThread::numphotoX, startsearchXW, true, PointXW, Xstatic);
                            else Bmpfound("\\X", ScanThread::staticPointX[startsearchXW].x, ScanThread::staticPointX[startsearchXW].y, startsearchXW, true, true, Xstatic);
                        }
                        else ButtonScanAction("\\X", 1, ScanThread::numphotoX, startsearchXW, true, PointXW, Xstatic);

                    }
                    if (scantick == 3 && ScanThread::numphotoY > 0)
                    {
                        if (Ystatic == 1)
                        {
                            if (ScanThread::staticPointY[startsearchYW].x == 0 && ScanThread::staticPointY[startsearchYW].y == 0)
                                //MessageBoxA(NULL, "heisann", "A", MB_OK);
                                ButtonScanAction("\\Y", 1, ScanThread::numphotoY, startsearchYW, true, PointYW, Ystatic);
                            else
                                Bmpfound("\\Y", ScanThread::staticPointY[startsearchYW].x, ScanThread::staticPointY[startsearchYW].y, startsearchYW, true, true, Ystatic);
                        }
                        ButtonScanAction("\\Y", 1, ScanThread::numphotoY, startsearchYW, true, PointYW, Ystatic);
                    }
                    Sleep(10);
                }
                //else Sleep(1000);
            }
            return 0;
        }

        bool ScanThread::ButtonPressed(UINT buttonFlagg) 
        {
            bool returnedvalue = false;
            if (buttonFlagg == 0)
            {
                if (!PreScanningEnabled) {
                    returnedvalue = ButtonScanAction("\\A", mode, ScanThread::numphotoA, ScanThread::startsearchA, false, { 0,0 }, Astatic);//buttonacton will be occupied by scanthread
                }
                else 
                {
                    EnterCriticalSection(&ScanThread::critical);
                    POINT Cpoint;
                    Cpoint.x = ScanThread::PointA.x;
                    Cpoint.y = ScanThread::PointA.y;
                    if (Cpoint.x != 0 && Cpoint.y != 0)
                    {
                        if (ScanThread::ShoulderNextBMP == 0)
                        {
                            if (ScanThread::startsearchA < ScanThread::numphotoA - 1)
                                ScanThread::startsearchA++; //dont want it to update before input done
                            else ScanThread::startsearchA = 0;
                            ScanThread::PointA.x = 0;
                            ScanThread::PointA.y = 0;
                        }
                        BmpInputAction(Cpoint.x, Cpoint.y, scanAtype);
                        returnedvalue = true;

                    }
                    LeaveCriticalSection(&ScanThread::critical);
                }
                if (mode == 2 && MainThread::showmessage != 11)
                {
                    ScanThread::numphotoA++;
                    Sleep(500);
                }
            }
            if (buttonFlagg == 1)
            {
                if (!PreScanningEnabled) {
                    returnedvalue = ButtonScanAction("\\B", mode, ScanThread::numphotoB, ScanThread::startsearchB, false, { 0,0 }, Bstatic);//buttonacton will be occupied by scanthread
                }
                else
                {
                    EnterCriticalSection(&ScanThread::critical);
                    POINT Cpoint;
                    Cpoint.x = ScanThread::PointB.x;
                    Cpoint.y = ScanThread::PointB.y;
                    if (Cpoint.x != 0 && Cpoint.y != 0)
                    {
                        if (ScanThread::ShoulderNextBMP == 0)
                        {
                            if (ScanThread::startsearchB < ScanThread::numphotoB - 1)
                                ScanThread::startsearchB++; //dont want it to update before input done
                            else ScanThread::startsearchB = 0;
                            ScanThread::PointA.x = 0;
                            ScanThread::PointA.y = 0;
                        }
                        BmpInputAction(Cpoint.x, Cpoint.y, ScanThread::scanBtype);
                        returnedvalue = true;
                    }
                    LeaveCriticalSection(&ScanThread::critical);
                }
                if (mode == 2 && MainThread::showmessage != 11)
                {
                    ScanThread::numphotoB++;
                    Sleep(500);
                }
            }
            if (buttonFlagg == 2)
            {
                if (!PreScanningEnabled) {
                    returnedvalue = ButtonScanAction("\\X", mode, ScanThread::numphotoX, ScanThread::startsearchX, false, { 0,0 }, Xstatic);//buttonacton will be occupied by scanthread
                }
                else
                {
                    EnterCriticalSection(&ScanThread::critical);
                    POINT Cpoint;
                    Cpoint.x = ScanThread::PointX.x;
                    Cpoint.y = ScanThread::PointX.y;

                    if (Cpoint.x != 0 && Cpoint.y != 0)
                    {
                        if (ScanThread::ShoulderNextBMP == 0)
                        {
                            if (ScanThread::startsearchX < ScanThread::numphotoX - 1)
                                ScanThread::startsearchX++; //dont want it to update before input done
                            else ScanThread::startsearchX = 0;
                            ScanThread::PointX.x = 0;
                            ScanThread::PointX.y = 0;
                        }
                        BmpInputAction(Cpoint.x, Cpoint.y, ScanThread::scanXtype);
                        returnedvalue = true;

                    }
                    LeaveCriticalSection(&ScanThread::critical);
                }
                if (mode == 2 && MainThread::showmessage != 11)
                {
                    ScanThread::numphotoX++;
                    Sleep(500);
                }
            }
            if (buttonFlagg == 3)
            {
                if (!PreScanningEnabled) {
                    returnedvalue = ButtonScanAction("\\Y", mode, ScanThread::numphotoY, ScanThread::startsearchY, false, { 0,0 }, Ystatic);//buttonacton will be occupied by scanthread
                }
                else
                {
                    EnterCriticalSection(&ScanThread::critical);
                    POINT Cpoint;
                    Cpoint.x = ScanThread::PointY.x;
                    Cpoint.y = ScanThread::PointY.y;

                    //LeaveCriticalSection(&critical);
                    if (Cpoint.x != 0 && Cpoint.y != 0)
                    {
                        if (ScanThread::ShoulderNextBMP == 0)
                        {
                            if (ScanThread::startsearchY < ScanThread::numphotoY - 1)
                                ScanThread::startsearchY++; //dont want it to update before input done
                            else ScanThread::startsearchY = 0;
                            PointY.x = 0;
                            PointY.y = 0;
                        }
                        BmpInputAction(Cpoint.x, Cpoint.y, ScanThread::scanYtype);
                        returnedvalue = true;

                    }
                    LeaveCriticalSection(&critical);
                }
                if (mode == 2 && MainThread::showmessage != 11)
                {
                    ScanThread::numphotoY++;
                    Sleep(500);
                }
            }
            if (buttonFlagg == 5) //C
            {
                if (!PreScanningEnabled)
                {
                    EnterCriticalSection(&ScanThread::critical);
                    returnedvalue = ButtonScanAction("\\C", mode, ScanThread::numphotoC, ScanThread::startsearchC, false, { 0,0 }, false); //2 save bmps
                    LeaveCriticalSection(&ScanThread::critical);
                }
                else if (ScanThread::ShoulderNextBMP == 1)
                {
                    EnterCriticalSection(&ScanThread::critical);
                    if (ScanThread::startsearchA < ScanThread::numphotoA - 1)
                        ScanThread::startsearchA++; //dont want it to update before input done
                    else ScanThread::startsearchA = 0;
                    if (ScanThread::startsearchB < ScanThread::numphotoB - 1)
                        ScanThread::startsearchB++; //dont want it to update before input done
                    else ScanThread::startsearchB = 0;
                    if (ScanThread::startsearchX < ScanThread::numphotoX - 1)
                        ScanThread::startsearchX++; //dont want it to update before input done
                    else ScanThread::startsearchX = 0;
                    LeaveCriticalSection(&ScanThread::critical);
                }
                if (mode == 2 && MainThread::showmessage != 11)
                {
                    ScanThread::numphotoC++;
                    Sleep(500);
                }

            }
            if (buttonFlagg == CUSTOM_ID_LB) //D
            {
                if (!PreScanningEnabled)
                {
                    EnterCriticalSection(&ScanThread::critical);
                    returnedvalue = ButtonScanAction("\\D", mode, ScanThread::numphotoD, ScanThread::startsearchD, false, { 0,0 }, false); //2 save bmps
                    LeaveCriticalSection(&ScanThread::critical);
                }
                else if (ScanThread::ShoulderNextBMP == 1)
                {
				
                    EnterCriticalSection(&ScanThread::critical);
                    if (ScanThread::startsearchA > 0)
                        ScanThread::startsearchA--;
                    else ScanThread::startsearchA = ScanThread::numphotoA - 1;
                    if (ScanThread::startsearchB > 0)
                        ScanThread::startsearchB--;
                    else ScanThread::startsearchB = ScanThread::numphotoB - 1;
                    if (ScanThread::startsearchX > 0)
                        ScanThread::startsearchX--;
                    else ScanThread::startsearchX = ScanThread::numphotoX - 1;
                    if (ScanThread::startsearchY > 0)
                        ScanThread::startsearchY--; 
                    else ScanThread::startsearchY = ScanThread::numphotoY - 1;
                    LeaveCriticalSection(&ScanThread::critical);
                }
                //else MessageBoxA(NULL, "heisann2", "A", MB_OK);
                if (mode == 2 && MainThread::showmessage != 11)
                {
                    ScanThread::numphotoD++;
                    Sleep(500);
                }

            }
            if (buttonFlagg == CUSTOM_ID_RSB) //E
            {
               // MessageBoxA(NULL, "heisann", "RSB", MB_OK);
                if (!PreScanningEnabled)
                {
                    EnterCriticalSection(&critical);
                    returnedvalue = ButtonScanAction("\\E", mode, ScanThread::numphotoD, ScanThread::startsearchD, false, { 0,0 }, false); //2 save bmps
                    LeaveCriticalSection(&critical);
                }
                if (mode == 2 && MainThread::showmessage != 11)
                {
                    ScanThread::numphotoE++;
                    Sleep(500);
                }
            }
            if (buttonFlagg == CUSTOM_ID_LSB) //F
            {
                //MessageBoxA(NULL, "heisann", "LSB", MB_OK);
                if (!PreScanningEnabled)
                {
                    EnterCriticalSection(&critical);
                    returnedvalue = ButtonScanAction("\\F", mode, ScanThread::numphotoF, ScanThread::startsearchF, false, { 0,0 }, false); //2 save bmps
                    LeaveCriticalSection(&critical);
                }
                if (mode == 2 && MainThread::showmessage != 11)
                {
                    ScanThread::numphotoF++;
                    Sleep(500);
                }
            }
			return returnedvalue;

        }


        void ScanThread::StartScanThread(HMODULE hmodule, int Astatic, int Bstatic, int Xstatic, int Ystatic, bool prescan) {
            PreScanningEnabled = prescan;
            std::thread tree(ScanThreadMain, hmodule, Astatic, Bstatic, Xstatic, Ystatic);
            tree.detach();
        }

    
}