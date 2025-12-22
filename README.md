Software is free to use and share for non-profit purposes. 
Improvements and suggestions are welcome.

Description:
A dll/plugin that Emulates mouse input with an Xinput controller. Coordinates are decided by presaved .BMP 24 bit format files or an emulated cursor or both.  
When a button is pressed on the controller, the game window content will be copied to memory. 
Then the presaved bmp will be compared to each pixel position until it fits, similar to a puzzle.
If the presaved bmp do fit in a location, a (mouse click/emulated cursor move/or both) command (with SendInput or PostMessage) will be sent to the coordinates found.
next mapped spots can be rendered on gamewindow if Scan is enabled. also possible to scroll through them with shoulder buttons.

4 modes:
0 = no emulated cursor, only bmp searches
1 = emulated cursor + bmp search(controllable with left or right stick depending on setting)                
2 = bmp button mapping mode. will map button presses(X, Y, A, B, R2, L2, L3, R3 buttons) to emulated cursor position   
3 = static button mapping mode. Displaying coordinates. Note them and write them to ini to map static point 

Hooks:
  //does not trap real cursor if enabled
ClipCursorHook=1
  //keyboard input hook. if 1 then enabled
GetKeystateHook=1
  //keyboard input hook. if 1 then enabled
GetAsynckeystateHook=1
  //mouse position hook. if 1 then enabled
GetCursorposHook=1
  //mouse setting position hook. if 1 then position fake cursor instead
SetCursorposHook=1
  //not so commonly used mouse position hook. if 1 then enabled
GetCursorInfoHook=1
  //cursor image hook. if 1 then steal image for emulated cursor
SetCursorHook=1
  //keyboard state hook
GetKeyboardStateHook=1
  //raw input. if 1 enabled, also send system wide WM_Input to trigger function call. if 2 then no WM_Input.
rawinputhook=1
  //hook registering rawinput for background input not in focus
RegisterRawInputHook=1

guide:
1: Start a game with this dll.
1: Open Setting file named Xinput.ini ( was named screenshotinput.ini had to rename as it was too long)
2: Setting AllowModeChange lets you change modes with "start" button on controller
3: Set to "edit mode" to start mapping, saving bmps. 
4: In Setting button InputType, lets you decide if the mapped button shall just move cursor to coordinate just press or do both. multiple bmp matches supported
5: Setting UseRealMouse lets you decide to use system mouse with SendInput. or use PostMessage to attached game window
5: Enjoy.

Mapping:
Arrow keys = move getcursorposhook to window edge. Scrolls maps on strategy games
Left trigger = Right mouse button
Right trigger = Left mouse button
Right joy axis = mousemove ( if setting Righthanded is 1, else its the left joy axis)
X, Y, A, B, R2, L2, L3, R3 can be mapped to bmp
No map to keyboard yet

Settings: Xinput.ini
// Makes Xinput input possible by injecting in mouse and keyboard input poll functions
// Controllers have few buttons, so mapping coordinates to bmp searches. or static points is also possible.
// If scan mode is enabled then the next searched point will be rendered on screen.
// If also ShouldersNextBMP is enabled then you can scroll through coordinates with should buttons.
//ControllerID 0 to 3
//standard value: 0

//   mode settings
//"Initial Mode" is what mode to set on startup.

//   0 = only send input on bmp search(A,B,X,Y buttons)
//   1 = emulated cursor with red dot + bmp search(controllable with left stick)
//   2 = bmp button mapping mode. will map button presses(A,B,X,Y buttons) to red dot position
//   3 = static button mapping mode. will record coordinates. note the coordinates you want, and write them to ini

//"Allow modechange" 0 is false(not allow) and 1 is true(allow).
//   You can change mode with "start button" on controller if setting is on
//////////////////////////////////////////////////////////////////////////////////////////////////
[Hooks]

//does not trap real cursor if enabled
ClipCursorHook=1

//keyboard input hook. if 1 then enabled
GetKeystateHook=1

//keyboard input hook. if 1 then enabled
GetAsynckeystateHook=1

//mouse position hook. if 1 then enabled
GetCursorposHook=1

//mouse setting position hook. if 1 then position fake cursor instead
SetCursorposHook=1

//not so commonly used mouse position hook. if 1 then enabled
GetCursorInfoHook=1

//cursor image hook. if 1 then steal image for emulated cursor
SetCursorHook=1

//keyboard state hook
GetKeyboardStateHook=1

//raw input. if 1 enabled, also send system wide WM_Input to trigger function call. if 2 then not send system wide WM_Input.
//only one instance need to sent the WM_Input messages
rawinputhook=1

//hook registering rawinput for background input not in focus
RegisterRawInputHook=1

//filters stolen from protoinput. blocks real MKB
RawInputFilter=0
MouseMoveFilter=0
MouseActivateFilter=0
WindowActivateFilter=0
WindowActivateAppFilter=0
MouseWheelFilter=0
MouseButtonFilter=0
KeyboardButtonFilter=0

//hook at once if 1 or wait if 0. if 2 right after window found
hooksoninit=2

//////////////////////////////////////////////////////////////////////////////////////////////////
[Settings]
//Controller ID. Zero is the first device connected.
Controllerid=0

//Initial Mode to start the dll in. 
//0 is only BMPs and Statics without fake cursor
//1 is only BMPs and Statics with fake cursor
//2 is BMP mapping mode
//3 is static mapping mode
Initial Mode=1

	//if 1 then increment mode with modechange button
	//modechange button is XINPUT_GAMEPAD_START
Allow modechange=0

	//WASD send. closer to 0 is less deadzone. right and down is in - value. other positive. unless you want reverse stick.
AxisLeftsens=-9000
AxisRightsens=9000
AxisUpsens=9000
AxisDownsens=-9000

	//set game position and resolution. no hooks so may not work.
posX=0
posY=0
resX=0
resY=0

	//mouse sens
Sensitivity=3.0
Radial_Deadzone=0.1
	//mouse deadzones
Axial_Deadzone=0.0
Accel_Multiplier=1.4
	
	//amount of milliseconds sleep between each poll and render. higher values may be better for performance
	//default is 1. continous bmp scans are worse for performance
Responsetime=1

	//"A"  (A from A to F)
	//      A button input setting:
	//		bmp action value 0/1000/2000	
	//		0 is move and click, 1000 is only move, 2000 only click

	//		example 2010 do only click on bmp search, or press W key if bmp not found or available

	//		0: move and click both cursors
	//              1: Only move emulated cursor
	//              2: Only click with real cursor

	//		//system keys
	//              3: Escape
	//              4: Enter
	//              5: Tab
	//              6: Shift
	//              7: Control
	//              8: Space

	//              //normal keys
	//              10: W
	//              11: S
	//              12: A
	//              13: D
	//              14: E
	//              15: F
	//              16: G
	//              17: H
	//              18: I
	//              19: Q
	//              20: Period
	//              21: R
	//              22: T
	//              23: B
	//              24: C

	//              40: UP
	//              41: DOWN
	//              42: LEFT
	//              43: RIGHT
	//              46= L
	//              51-62: F1-F12
	//              63: Control+C
	//button mappings 
	//will press key only if bmp or static is not found or defined. else it does mouseclick on coordinates
	//bmp found action is also defined here. 0 = move and press. 1000 = only move. 2000 = only click
	//example: A=1008. if press A, on bmp or static found will only move. or press space if not found
A=8
B=4
X=1
Y=16
RightShoulder=14
LeftShoulder=19
RightThumb=3
LeftThumb=46
Upkey=12
Downkey=24
Leftkey=46
Rightkey=8


	//will scan for bmps continously to find next coordinate. 
	//the coordinate found will be rendered with the button image(O, trianlge, X,A, quad)
	//on correct button press then do action on found point
Scan=0


	//if 1 then bmps found will be remembered. coordinates will be saved and not searched again
ScanAstatic=1
ScanBstatic=1
ScanXstatic=1
ScanYstatic=1

	//will scroll bmp points. if scan enabled.
ShouldersNextBMP=0

	//swapping sticks so left stick work like right stick and opposite
Righthanded=0

	//drawfakecursor 0 does not draw cursor. 1 does draw direct in game window. 2 does partial draw(fast, but not working well, doesnt erase messages and points found) on own cursorwindow. 3 does full redraw(slow but sure that all old messages and points are erased). 
	//if scan=1 and value is 2, then default to 3
	//(hooks)setcursorhook is necessary to copy the games cursor bitmap used in cursor drawing. else we just draw out own
DrawFakeCursor=3

	//DrawFakeCursorAlways 1 does not skip cursor drawing if cursor=0.
DrawFakeCursorAlways=0

	//should always be zero. uses sendinput if enabled. not splittable	
UseRealMouse=0

	//scrollmapfix
	//0 = keyboard arrow buttons on stick
	//1 - 2 = stick set different getmousepos pos for mapscrolling on strategy games 
	//3 = scroll map by right click + drag with stick
	//4 = WASD on stick 
	//FPS games should use 4
Scrollmapfix=4

	//sends focus messages on every input
	//not recommended, but maybe necessary on some games
Sendfocus=1

	//linear cursor drawing offset counted in pixels
Xoffset=0
Yoffset=0

	//sends special doubleclick messages on double clicks. needed for some games
Doubleclicks=1

	//Scrollmapfix=3 uses mouse left click to scroll map in strategy games.
	//DelayEndScroll is the milliseconds to wait before releasing so scrolling is not mistaken for mouse click
DelayEndScroll=1

	//on Scrollmapfix=3 and 4 the mousewheel is mapped to Dpad:Up and Down. If MouseWheelContinous=1 then it keeps on posting each tick while held
MouseWheelContinous=1

	//some games have difficulties finding game window bounds. try 1 if window problems. if intro is not played within game window bounds
IgnoreRect=0

	//if you dont want the poor intro, set to 1
SkipIntro=0




//////////////////////////////////////////////////////////////////////////////////////////////////

//For mapping coordinates directly. (X)-> button (0)-> NumID (X)->dimension
//button is what button to map. currently only X,Y,A,B buttons possible here
//NumID, the program counts from zero always, and upward until no entry is found. the points read then set their ID on the + side of BMPs
//example below means if you got a X0.bmp, then the X0 static will be X1, if no bmp then it means only one point defined and X0 is X0 static
//there are 3 dimension here X and Y, + Z. Z is how the point scales on window resolution change. Z0 is simple stretch. Z1 is stretch with 4:3 aspect keep. Z2 is 16:9 aspect keep
//the coordinates entered here are calculated from a MAX constant 1024/768 resolution. so X0X=100 should map to 10% left. 
//To find static coordinates set "Initial Mode=3" to have a display of coordinates ingame, then you can also see if any of the Z scaling methods matches.
[Statics]
//X0X=100
//X0Y=100
//X0Z=0

//X1X=200
//X1Y=200
//X1Z=2

//X2X=300
//X2Y=300
//X2Z=1
//.........up to 50 each button
