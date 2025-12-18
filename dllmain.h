#pragma once
//setttings

//controller
int AxisLeftsens;
int AxisRightsens;
int AxisUpsens;
int AxisDownsens;
int scrollspeed3;


int InitialMode;
int Modechange;
int sendfocus;
int responsetime;
int doubleclicks;
int scrollenddelay;
int quickMW;
bool inithere = false;
int ShoulderNextbmp;

int scrolloutsidewindow;
int mode = InitialMode;
/////////////////
bool movedmouse;

HMODULE g_hModule = nullptr;

typedef BOOL(WINAPI* GetCursorPos_t)(LPPOINT lpPoint);
typedef BOOL(WINAPI* SetCursorPos_t)(int X, int Y); //GetKeyboardState

typedef SHORT(WINAPI* GetKeyboardState_t)(PBYTE lpKeyState);
typedef SHORT(WINAPI* GetAsyncKeyState_t)(int vKey);
typedef SHORT(WINAPI* GetKeyState_t)(int nVirtKey);
typedef BOOL(WINAPI* ClipCursor_t)(const RECT*);
typedef HCURSOR(WINAPI* SetCursor_t)(HCURSOR hCursor);

typedef BOOL(WINAPI* SetRect_t)(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom);
typedef BOOL(WINAPI* AdjustWindowRect_t)(LPRECT lprc, DWORD  dwStyle, BOOL bMenu);
typedef UINT(WINAPI* GetRawInputData_t)(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader);
typedef UINT(WINAPI* GetCursorInfo_t)(PCURSORINFO pci);
typedef HWND(WINAPI* CreateWindowExA_t)( DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
typedef HWND(WINAPI* CreateWindowExW_t)( DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int  nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam );

typedef BOOL(WINAPI* GetMessageA_t)(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
typedef BOOL(WINAPI* GetMessageW_t)(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
typedef BOOL(WINAPI* PeekMessageA_t)(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
typedef BOOL(WINAPI* PeekMessageW_t)(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);

//BOOL(WINAPI* fpGetMessageA)(LPMSG, HWND, UINT, UINT) = nullptr;
//BOOL(WINAPI* fpGetMessageW)(LPMSG, HWND, UINT, UINT) = nullptr;
//BOOL(WINAPI* fpPeekMessageA)(LPMSG, HWND, UINT, UINT, UINT) = nullptr;
//BOOL(WINAPI* fpPeekMessageW)(LPMSG, HWND, UINT, UINT, UINT) = nullptr;

GetMessageA_t fpGetMessageA = nullptr;
GetMessageW_t fpGetMessageW = nullptr;
PeekMessageA_t fpPeekMessageA = nullptr;
PeekMessageW_t fpPeekMessageW = nullptr;

typedef BOOL(WINAPI* RegisterRawInputDevices_t)( PRAWINPUTDEVICE pRawInputDevices, UINT uiNumDevices, UINT cbSize);
std::vector<HWND> g_windows;
    
// message filter hooks




bool rawmouseL = false;
bool rawmouseR = false;//0:scroll 1:left 2:right 3:up 4:down

std::vector<BYTE> keyState(256, 0);



CRITICAL_SECTION critical; //window thread
//CRITICAL_SECTION criticalA; //Scannning thread
GetKeyboardState_t fpGetKeyboardState = nullptr;
GetCursorPos_t fpGetCursorPos = nullptr;
GetCursorPos_t fpSetCursorPos = nullptr;
GetAsyncKeyState_t fpGetAsyncKeyState = nullptr;
GetKeyState_t fpGetKeyState = nullptr;
ClipCursor_t fpClipCursor = nullptr;
SetCursor_t fpSetCursor = nullptr;
SetRect_t fpSetRect = nullptr;
AdjustWindowRect_t fpAdjustWindowRect = nullptr;
GetRawInputData_t fpGetRawInputData = nullptr;
GetCursorInfo_t fpGetCursorInfo = nullptr;
CreateWindowExA_t fpCreateWindowExA = nullptr;
CreateWindowExW_t fpCreateWindowExW = nullptr;
RegisterRawInputDevices_t fpRegisterRawInputDevices = nullptr;




//POINT fakecursor;
POINT fakecursorW;
POINT startdrag;
POINT activatewindow;
POINT scroll;
bool loop = true;
HWND hwnd;
int showmessage = 0; //0 = no message, 1 = initializing, 2 = bmp mode, 3 = bmp and cursor mode, 4 = edit mode   
int showmessageW = 0; //0 = no message, 1 = initializing, 2 = bmp mode, 3 = bmp and cursor mode, 4 = edit mode 
int counter = 0;
bool scanrunning = false;

//syncronization control
HANDLE hMutex;

int getmouseonkey = 0;
int message = 0;
auto hInstance = nullptr;

POINT delta;

//hooks
bool hooksinited = false;
int keystatesend = 0; //key to send
int clipcursorhook = 0;
int getkeystatehook = 0;
int getasynckeystatehook = 0;
int getcursorposhook = 0;
int setcursorposhook = 0;
int setcursorhook = 0;
int rawinputhook = 0;
int GetCursorInfoHook = 0;
int GetKeyboardStateHook = 0;
bool registerrawinputhook = 0;
bool nodrawcursor = false;

int ignorerect = 0;
POINT rectignore = { 0,0 }; //for getcursorposhook
int setrecthook = 0;

int leftrect = 0;
int toprect = 0;
int rightrect = 0;
int bottomrect = 0;

int userealmouse = 0;

int resize = 1;
int numphotoA = -1;
int numphotoB = -1;
int numphotoX = -1;
int numphotoY = -1;
int numphotoC = -1;
int numphotoD = -1;
int numphotoE = -1;
int numphotoF = -1;

int numphotoAbmps;
int numphotoBbmps;
int numphotoXbmps;
int numphotoYbmps;

bool AuseStatic = 1;
bool BuseStatic = 1;
bool XuseStatic = 1;
bool YuseStatic = 1;
//int needatick = 0;
//bool movedmouse = false;
//std::vector<POINT> staticPointA;
//std::vector<POINT> staticPointB;
//std::vector<POINT> staticPointX;
//std::vector<POINT> staticPointY;

//fake cursor
int controllerID = 0;
int Xf = 100;
int Yf = 100;
int OldX = 0;
int OldY = 0;
int ydrag;
int xdrag;
int Xoffset = 0; //offset for cursor    
int Yoffset = 0;
bool scrollmap = false;
bool pausedraw = false;
bool gotcursoryet = false;
int drawfakecursor = 0;
int alwaysdrawcursor = 0; //always draw cursor even if setcursor set cursor NULL
HICON hCursor = 0;
DWORD lastClickTime;
HDC PointerWnd;
int WoldX, WoldY;
//bmp search
bool foundit = false;
int hooksoninit = 0;

int cursoroffsetx, cursoroffsety;
int offsetSET; //0:sizing 1:offset 2:done
int cursorWidth = 40;
int cursorHeight = 40;
HWND pointerWindow = nullptr;
bool DrawFakeCursorFix = false;
static int transparencyKey = RGB(0, 0, 1);

//setwindowpos
int posX;
int posY;
int resX;
int resY;
HCURSOR oldhCursor = NULL;
HCURSOR hCursorW = NULL;
bool nochange = false;

bool oldHadShowCursor = true;

const extern WPARAM ScreenshotInput_MOUSE_SIGNATURE = 0x10000000;
const extern LPARAM ScreenshotInput_KEYBOARD_SIGNATURE = 0x10000000;
bool g_filterRawInput = false;
bool g_filterMouseMove = false;
bool g_filterMouseActivate = false;
bool g_filterWindowActivate = false;
bool g_filterWindowActivateApp = false;
bool g_filterMouseWheel = false;
bool g_filterMouseButton = true;
bool g_filterKeyboardButton = false;

POINT oldposcheck;
POINT oldrescheck;

//scroll type 3
int tick = 0;
bool doscrollyes = false;

// 
//beautiful cursor
int colorfulSword[20][20] = {
{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{1,2,2,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{1,2,2,2,2,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
{1,2,2,2,2,2,2,1,1,0,0,0,0,0,0,0,0,0,0,0},
{1,2,2,2,2,2,2,2,2,1,1,0,0,0,0,0,0,0,0,0},
{1,2,2,2,2,2,2,2,2,2,2,1,1,0,0,0,0,0,0,0},
{1,2,2,2,2,2,2,2,2,2,2,2,2,1,1,0,0,0,0,0},
{1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,0,0,0},
{1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0},
{1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0},
{1,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,0,0,0,0},
{1,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0},
{1,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0},
{1,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0},
{1,2,2,2,2,2,2,1,1,2,2,2,1,0,0,0,0,0,0,0},
{1,2,2,2,2,2,1,0,0,1,2,2,2,2,1,0,0,0,0,0},
{1,2,2,2,2,1,0,0,0,0,1,2,2,2,1,0,0,0,0,0},
{1,1,2,2,1,0,0,0,0,0,0,1,2,2,2,1,0,0,0,0},
{1,2,2,1,0,0,0,0,0,0,0,1,2,2,2,1,0,0,0,0},
{1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0},
};
//temporary cursor on success

COLORREF colors[5] = {
    RGB(0, 0, 0),          // Transparent - won't be drawn
    RGB(140, 140, 140),    // Gray for blade
    RGB(255, 255, 255),    // White shine
    RGB(139, 69, 19),       // Brown handle
    RGB(50, 150, 255)     // Light blue accent

};

bool onoroff = true;

//remember old keystates
int oldscrollrightaxis = false; //reset 
int oldscrollleftaxis = false; //reset 
int oldscrollupaxis = false; //reset 
int oldscrolldownaxis = false; //reset 
bool Apressed = false;
bool Bpressed = false;
bool Xpressed = false;
bool Ypressed = false;
bool leftPressedold;
bool rightPressedold;
bool oldA = false;
bool oldB = false;
bool oldX = false;
bool oldY = false;
bool oldC = false;
bool oldD = false;
bool oldE = false;
bool oldF = false;
bool oldup = false;
bool olddown = false;
bool oldleft = false;
bool oldright = false;

bool musLB = false;
bool musRB = false;
bool rawmouseWu = false;
bool rawmouseWd = false;

int startsearch = 0;
int startsearchA = 0;
int startsearchB = 0;
int startsearchX = 0;
int startsearchY = 0;
int startsearchC = 0;
int startsearchD = 0;
int startsearchE = 0;
int startsearchF = 0;

int righthanded = 0;
int scanoption = 0;

int Atype = 0;
int Btype = 0;
int Xtype = 0;
int Ytype = 0;
int Ctype = 0;
int Dtype = 0;
int Etype = 0;
int Ftype = 0;

POINT PointA;
POINT PointB;  
POINT PointX;
POINT PointY;
int scantick = 0;
int findwindowdelay = 10;
int bmpAtype = 0;
int bmpBtype = 0;
int bmpXtype = 0;
int bmpYtype = 0;
int bmpCtype = 0;
int bmpDtype = 0;
int bmpEtype = 0;
int bmpFtype = 0;

int uptype = 0;
int downtype = 0;
int lefttype = 0;
int righttype = 0;


int x = 0;

HBITMAP hbm;

std::vector<BYTE> largePixels, smallPixels;
SIZE screenSize;
int strideLarge, strideSmall;
int smallW, smallH;

//int sovetid = 16;
int knappsovetid = 100;

int samekey = 0;
int samekeyA = 0;