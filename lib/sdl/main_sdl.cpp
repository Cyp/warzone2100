/*
	This file is part of Warzone 2100.
	Copyright (C) 2011-2019  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/**
 * @file main_sdl.cpp
 *
 * SDL backend code
 */

// Get platform defines before checking for them!
#include "lib/framework/wzapp.h"

#include <QtWidgets/QApplication>
// This is for the cross-compiler, for static QT 5 builds to avoid the 'plugins' crap on windows
#if defined(QT_STATICPLUGIN)
#include <QtCore/QtPlugin>
#endif
#include "lib/framework/input.h"
#include "lib/framework/utf.h"
#include "lib/framework/opengl.h"
#include "lib/ivis_opengl/pieclip.h"
#include "lib/ivis_opengl/piemode.h"
#include "lib/ivis_opengl/screen.h"
#include "lib/gamelib/gtime.h"
#include "src/warzoneconfig.h"
#include "src/game.h"
#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_clipboard.h>
#include "wz2100icon.h"
#include "cursors_sdl.h"
#include <algorithm>
#include <map>
#include <locale.h>
#include <atomic>

// This is for the cross-compiler, for static QT 5 builds to avoid the 'plugins' crap on windows
#if defined(QT_STATICPLUGIN)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif

#if defined(WZ_OS_MAC)
#include "cocoa_sdl_helpers.h"
#include "cocoa_wz_menus.h"
#endif

void mainLoop();
// used in crash reports & version info
const char *BACKEND = "SDL";

std::map<KEY_CODE, SDL_Keycode> KEY_CODE_to_SDLKey;
std::map<SDL_Keycode, KEY_CODE > SDLKey_to_KEY_CODE;

int realmain(int argc, char *argv[]);

// the main stub which calls realmain() aka, WZ's main startup routines
int main(int argc, char *argv[])
{
	return realmain(argc, argv);
}

// At this time, we only have 1 window and 1 GL context.
static SDL_Window *WZwindow = nullptr;
static SDL_GLContext WZglcontext = nullptr;

// The screen that the game window is on.
int screenIndex = 0;
// The logical resolution of the game in the game's coordinate system (points).
unsigned int screenWidth = 0;
unsigned int screenHeight = 0;
// The logical resolution of the SDL window in the window's coordinate system (points) - i.e. not accounting for the Game Display Scale setting.
unsigned int windowWidth = 0;
unsigned int windowHeight = 0;
// The current display scale factor.
unsigned int current_displayScale = 100;
float current_displayScaleFactor = 1.f;

static std::vector<screeninfo> displaylist;	// holds all our possible display lists

QCoreApplication *appPtr;				// Needed for qtscript

#if defined(WZ_OS_MAC)
// on macOS, SDL_WINDOW_FULLSCREEN_DESKTOP *must* be used (or high-DPI fullscreen toggling breaks)
const SDL_WindowFlags WZ_SDL_FULLSCREEN_MODE = SDL_WINDOW_FULLSCREEN_DESKTOP;
#else
const SDL_WindowFlags WZ_SDL_FULLSCREEN_MODE = SDL_WINDOW_FULLSCREEN;
#endif

std::atomic<Uint32> wzSDLAppEvent((Uint32)-1);
enum wzSDLAppEventCodes
{
	MAINTHREADEXEC
};

/* The possible states for keys */
enum KEY_STATE
{
	KEY_UP,
	KEY_DOWN,
	KEY_DRAG  // Only used by mouse keys
};

struct MOUSE_STATE
{
	KEY_STATE state;  /// Last key/mouse state
	unsigned lastdown;  /// Last mouse button down timestamp
};

// Clipboard routines
bool has_scrap(void);
bool put_scrap(char *src);
bool get_scrap(char **dst);

/// constant for the interval between 2 singleclicks for doubleclick event in ms
#define DOUBLE_CLICK_INTERVAL 250

/* The current state of the keyboard */
static KEY_STATE aKeyState[KEY_MAXSCAN];		// NOTE: SDL_NUM_SCANCODES is the max, but KEY_MAXSCAN is our limit

/* The current location of the mouse */
static Vector2i mousePos = {0, 0};
static bool mouseInWindow = true;

/* How far the mouse has to move to start a drag */
#define DRAG_THRESHOLD	5

/* Which button is being used for a drag */
static MOUSE_KEY_CODE dragKey;

/* The start of a possible drag by the mouse */
static Vector2i drag = {0, 0};

/* The current mouse button state */
static MOUSE_STATE aMouseState[MOUSE_END];
static Events events;

/* The current screen resizing state for this iteration through the game loop, in the game coordinate system */
struct ScreenSizeChange
{
	ScreenSizeChange(unsigned int oldWidth, unsigned int oldHeight, unsigned int newWidth, unsigned int newHeight)
	:	oldWidth(oldWidth)
	,	oldHeight(oldHeight)
	,	newWidth(newWidth)
	,	newHeight(newHeight)
	{ }
	unsigned int oldWidth;
	unsigned int oldHeight;
	unsigned int newWidth;
	unsigned int newHeight;
};
static ScreenSizeChange* currentScreenResizingStatus = nullptr;

/* The size of the input buffer */
#define INPUT_MAXSTR 256

/* The input string buffer */
struct InputKey
{
	UDWORD key;
	utf_32_char unicode;
};

static InputKey	pInputBuffer[INPUT_MAXSTR];
static InputKey	*pStartBuffer, *pEndBuffer;
static unsigned int CurrentKey = 0;			// Our Current keypress
bool GetTextEvents = false;
/**************************/
/***     Misc support   ***/
/**************************/

// See if we have TEXT in the clipboard
bool has_scrap(void)
{
	return SDL_HasClipboardText();
}

// When (if?) we decide to put text into the clipboard...
bool put_scrap(char *src)
{
	if (SDL_SetClipboardText(src))
	{
		debug(LOG_ERROR, "Could not put clipboard text because : %s", SDL_GetError());
		return false;
	}
	return true;
}

// Get text from the clipboard
bool get_scrap(char **dst)
{
	if (has_scrap())
	{
		char *cliptext = SDL_GetClipboardText();
		if (!cliptext)
		{
			debug(LOG_ERROR, "Could not get clipboard text because : %s", SDL_GetError());
			return false;
		}
		*dst = cliptext;
		return true;
	}
	else
	{
		// wasn't text or no text in the clipboard
		return false;
	}
}

void StartTextInput()
{
	if (!GetTextEvents)
	{
		SDL_StartTextInput();	// enable text events
		CurrentKey = 0;
		GetTextEvents = true;
		debug(LOG_INPUT, "SDL text events started");
	}
}

void StopTextInput()
{
	SDL_StopTextInput();	// disable text events
	CurrentKey = 0;
	GetTextEvents = false;
	debug(LOG_INPUT, "SDL text events stopped");
}

/* Put a character into a text buffer overwriting any text under the cursor */
WzString wzGetSelection()
{
	WzString retval;
	static char *scrap = nullptr;

	if (get_scrap(&scrap))
	{
		retval = WzString::fromUtf8(scrap);
	}
	return retval;
}

// Here we handle VSYNC enable/disabling
#if defined(WZ_WS_X11)

#if defined(__clang__)
// Some versions of the X11 headers (which are included by <GL/glx.h>)
// trigger `named variadic macros are a GNU extension [-Wvariadic-macros]`
// on Clang. https://lists.x.org/archives/xorg-devel/2015-January/045216.html
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wvariadic-macros"
#endif

#include <GL/glx.h> // GLXDrawable

#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

// X11 polution
#ifdef Status
#undef Status
#endif // Status
#ifdef CursorShape
#undef CursorShape
#endif // CursorShape
#ifdef Bool
#undef Bool
#endif // Bool

#ifndef GLX_SWAP_INTERVAL_EXT
#define GLX_SWAP_INTERVAL_EXT 0x20F1
#endif // GLX_SWAP_INTERVAL_EXT

// Need this global for use case of only having glXSwapIntervalSGI
static int swapInterval = -1;

void wzSetSwapInterval(int interval)
{
	typedef void (* PFNGLXQUERYDRAWABLEPROC)(Display *, GLXDrawable, int, unsigned int *);
	typedef void (* PFNGLXSWAPINTERVALEXTPROC)(Display *, GLXDrawable, int);
	typedef int (* PFNGLXGETSWAPINTERVALMESAPROC)(void);
	typedef int (* PFNGLXSWAPINTERVALMESAPROC)(unsigned);
	typedef int (* PFNGLXSWAPINTERVALSGIPROC)(int);
	PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT;
	PFNGLXQUERYDRAWABLEPROC glXQueryDrawable;
	PFNGLXGETSWAPINTERVALMESAPROC glXGetSwapIntervalMESA;
	PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA;
	PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI;

	if (interval < 0)
	{
		interval = 0;
	}

#if GLX_VERSION_1_2
	// Hack-ish, but better than not supporting GLX_SWAP_INTERVAL_EXT?
	GLXDrawable drawable = glXGetCurrentDrawable();
	Display *display =  glXGetCurrentDisplay();
	glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC) SDL_GL_GetProcAddress("glXSwapIntervalEXT");
	glXQueryDrawable = (PFNGLXQUERYDRAWABLEPROC) SDL_GL_GetProcAddress("glXQueryDrawable");

	if (glXSwapIntervalEXT && glXQueryDrawable && drawable)
	{
		unsigned clampedInterval;
		glXSwapIntervalEXT(display, drawable, interval);
		glXQueryDrawable(display, drawable, GLX_SWAP_INTERVAL_EXT, &clampedInterval);
		swapInterval = clampedInterval;
		return;
	}
#endif

	glXSwapIntervalMESA = (PFNGLXSWAPINTERVALMESAPROC) SDL_GL_GetProcAddress("glXSwapIntervalMESA");
	glXGetSwapIntervalMESA = (PFNGLXGETSWAPINTERVALMESAPROC) SDL_GL_GetProcAddress("glXGetSwapIntervalMESA");
	if (glXSwapIntervalMESA && glXGetSwapIntervalMESA)
	{
		glXSwapIntervalMESA(interval);
		swapInterval = glXGetSwapIntervalMESA();
		return;
	}

	glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC) SDL_GL_GetProcAddress("glXSwapIntervalSGI");
	if (glXSwapIntervalSGI)
	{
		if (interval < 1)
		{
			interval = 1;
		}
		if (glXSwapIntervalSGI(interval))
		{
			// Error, revert to default
			swapInterval = 1;
			glXSwapIntervalSGI(1);
		}
		else
		{
			swapInterval = interval;
		}
		return;
	}
	swapInterval = 0;
}

int wzGetSwapInterval()
{
	if (swapInterval >= 0)
	{
		return swapInterval;
	}

	typedef void (* PFNGLXQUERYDRAWABLEPROC)(Display *, GLXDrawable, int, unsigned int *);
	typedef int (* PFNGLXGETSWAPINTERVALMESAPROC)(void);
	typedef int (* PFNGLXSWAPINTERVALSGIPROC)(int);
	PFNGLXQUERYDRAWABLEPROC glXQueryDrawable;
	PFNGLXGETSWAPINTERVALMESAPROC glXGetSwapIntervalMESA;
	PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI;

#if GLX_VERSION_1_2
	// Hack-ish, but better than not supporting GLX_SWAP_INTERVAL_EXT?
	GLXDrawable drawable = glXGetCurrentDrawable();
	Display *display =  glXGetCurrentDisplay();
	glXQueryDrawable = (PFNGLXQUERYDRAWABLEPROC) SDL_GL_GetProcAddress("glXQueryDrawable");

	if (glXQueryDrawable && drawable)
	{
		unsigned interval;
		glXQueryDrawable(display, drawable, GLX_SWAP_INTERVAL_EXT, &interval);
		swapInterval = interval;
		return swapInterval;
	}
#endif

	glXGetSwapIntervalMESA = (PFNGLXGETSWAPINTERVALMESAPROC) SDL_GL_GetProcAddress("glXGetSwapIntervalMESA");
	if (glXGetSwapIntervalMESA)
	{
		swapInterval = glXGetSwapIntervalMESA();
		return swapInterval;
	}

	glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC) SDL_GL_GetProcAddress("glXSwapIntervalSGI");
	if (glXSwapIntervalSGI)
	{
		swapInterval = 1;
	}
	else
	{
		swapInterval = 0;
	}
	return swapInterval;
}

#elif defined(WZ_WS_WIN)

void wzSetSwapInterval(int interval)
{
	typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC)(int);
	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

	if (interval < 0)
	{
		interval = 0;
	}

	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) SDL_GL_GetProcAddress("wglSwapIntervalEXT");

	if (wglSwapIntervalEXT)
	{
		wglSwapIntervalEXT(interval);
	}
}

int wzGetSwapInterval()
{
	typedef int (WINAPI * PFNWGLGETSWAPINTERVALEXTPROC)(void);
	PFNWGLGETSWAPINTERVALEXTPROC wglGetSwapIntervalEXT;

	wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC) SDL_GL_GetProcAddress("wglGetSwapIntervalEXT");
	if (wglGetSwapIntervalEXT)
	{
		return wglGetSwapIntervalEXT();
	}
	return 0;
}

#elif !defined(WZ_OS_MAC)
// FIXME:  This can't be right?
void wzSetSwapInterval(int)
{
	return;
}

int wzGetSwapInterval()
{
	return 0;
}

#endif

std::vector<screeninfo> wzAvailableResolutions()
{
	return displaylist;
}

std::vector<unsigned int> wzAvailableDisplayScales()
{
	static const unsigned int wzDisplayScales[] = { 100, 125, 150, 200, 250, 300, 400, 500 };
	return std::vector<unsigned int>(wzDisplayScales, wzDisplayScales + (sizeof(wzDisplayScales) / sizeof(wzDisplayScales[0])));
}

void setDisplayScale(unsigned int displayScale)
{
	current_displayScale = displayScale;
	current_displayScaleFactor = (float)displayScale / 100.f;
}

unsigned int wzGetCurrentDisplayScale()
{
	return current_displayScale;
}

void wzShowMouse(bool visible)
{
	SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
}

int wzGetTicks()
{
	return SDL_GetTicks();
}

void wzFatalDialog(const char *msg)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "We have a problem!", msg, nullptr);
}

void wzScreenFlip()
{
	SDL_GL_SwapWindow(WZwindow);
}

void wzToggleFullscreen()
{
	Uint32 flags = SDL_GetWindowFlags(WZwindow);
	if (flags & WZ_SDL_FULLSCREEN_MODE)
	{
		SDL_SetWindowFullscreen(WZwindow, 0);
		wzSetWindowIsResizable(true);
	}
	else
	{
		SDL_SetWindowFullscreen(WZwindow, WZ_SDL_FULLSCREEN_MODE);
		wzSetWindowIsResizable(false);
	}
}

bool wzIsFullscreen()
{
	assert(WZwindow != nullptr);
	Uint32 flags = SDL_GetWindowFlags(WZwindow);
	if ((flags & SDL_WINDOW_FULLSCREEN) || (flags & SDL_WINDOW_FULLSCREEN_DESKTOP))
	{
		return true;
	}
	return false;
}

bool wzIsMaximized()
{
	assert(WZwindow != nullptr);
	Uint32 flags = SDL_GetWindowFlags(WZwindow);
	if (flags & SDL_WINDOW_MAXIMIZED)
	{
		return true;
	}
	return false;
}

void wzQuit()
{
	// Create a quit event to halt game loop.
	SDL_Event quitEvent;
	quitEvent.type = SDL_QUIT;
	SDL_PushEvent(&quitEvent);
}

void wzGrabMouse()
{
	SDL_SetWindowGrab(WZwindow, SDL_TRUE);
}

void wzReleaseMouse()
{
	SDL_SetWindowGrab(WZwindow, SDL_FALSE);
}

void wzDelay(unsigned int delay)
{
	SDL_Delay(delay);
}

/**************************/
/***    Thread support  ***/
/**************************/
WZ_THREAD *wzThreadCreate(int (*threadFunc)(void *), void *data)
{
	return (WZ_THREAD *)SDL_CreateThread(threadFunc, "wzThread", data);
}

int wzThreadJoin(WZ_THREAD *thread)
{
	int result;
	SDL_WaitThread((SDL_Thread *)thread, &result);
	return result;
}

void wzThreadDetach(WZ_THREAD *thread)
{
	SDL_DetachThread((SDL_Thread *)thread);
}

void wzThreadStart(WZ_THREAD *thread)
{
	(void)thread; // no-op
}

void wzYieldCurrentThread()
{
	SDL_Delay(40);
}

WZ_MUTEX *wzMutexCreate()
{
	return (WZ_MUTEX *)SDL_CreateMutex();
}

void wzMutexDestroy(WZ_MUTEX *mutex)
{
	SDL_DestroyMutex((SDL_mutex *)mutex);
}

void wzMutexLock(WZ_MUTEX *mutex)
{
	SDL_LockMutex((SDL_mutex *)mutex);
}

void wzMutexUnlock(WZ_MUTEX *mutex)
{
	SDL_UnlockMutex((SDL_mutex *)mutex);
}

WZ_SEMAPHORE *wzSemaphoreCreate(int startValue)
{
	return (WZ_SEMAPHORE *)SDL_CreateSemaphore(startValue);
}

void wzSemaphoreDestroy(WZ_SEMAPHORE *semaphore)
{
	SDL_DestroySemaphore((SDL_sem *)semaphore);
}

void wzSemaphoreWait(WZ_SEMAPHORE *semaphore)
{
	SDL_SemWait((SDL_sem *)semaphore);
}

void wzSemaphorePost(WZ_SEMAPHORE *semaphore)
{
	SDL_SemPost((SDL_sem *)semaphore);
}

// Asynchronously executes exec->doExecOnMainThread() on the main thread
// `exec` should be a subclass of `WZ_MAINTHREADEXEC`
//
// `exec` must be allocated on the heap since the main event loop takes ownership of it
// and will handle deleting it once it has been processed.
// It is not safe to access `exec` after calling wzAsyncExecOnMainThread.
//
// No guarantees are made about when execFunc() will be called relative to the
// calling of this function - this function may return before, during, or after
// execFunc()'s execution on the main thread.
void wzAsyncExecOnMainThread(WZ_MAINTHREADEXEC *exec)
{
	Uint32 _wzSDLAppEvent = wzSDLAppEvent.load();
	assert(_wzSDLAppEvent != ((Uint32)-1));
	if (_wzSDLAppEvent == ((Uint32)-1)) {
		// The app-defined event has not yet been registered with SDL
		return;
	}
	SDL_Event event;
	SDL_memset(&event, 0, sizeof(event));
	event.type = _wzSDLAppEvent;
	event.user.code = wzSDLAppEventCodes::MAINTHREADEXEC;
	event.user.data1 = exec;
	assert(event.user.data1 != nullptr);
	event.user.data2 = 0;
	SDL_PushEvent(&event);
	// receiver handles deleting `exec` on the main thread after doExecOnMainThread() has been called
}

/*!
** The keycodes we care about
**/
static inline void initKeycodes()
{
	KEY_CODE_to_SDLKey = {
		{KEY_ESC, SDLK_ESCAPE},
		{KEY_1, SDLK_1},
		{KEY_2, SDLK_2},
		{KEY_3, SDLK_3},
		{KEY_4, SDLK_4},
		{KEY_5, SDLK_5},
		{KEY_6, SDLK_6},
		{KEY_7, SDLK_7},
		{KEY_8, SDLK_8},
		{KEY_9, SDLK_9},
		{KEY_0, SDLK_0},
		{KEY_MINUS, SDLK_MINUS},
		{KEY_EQUALS, SDLK_EQUALS},
		{KEY_BACKSPACE, SDLK_BACKSPACE},
		{KEY_TAB, SDLK_TAB},
		{KEY_Q, SDLK_q},
		{KEY_W, SDLK_w},
		{KEY_E, SDLK_e},
		{KEY_R, SDLK_r},
		{KEY_T, SDLK_t},
		{KEY_Y, SDLK_y},
		{KEY_U, SDLK_u},
		{KEY_I, SDLK_i},
		{KEY_O, SDLK_o},
		{KEY_P, SDLK_p},
		{KEY_LBRACE, SDLK_LEFTBRACKET},
		{KEY_RBRACE, SDLK_RIGHTBRACKET},
		{KEY_RETURN, SDLK_RETURN},
		{KEY_LCTRL, SDLK_LCTRL},
		{KEY_A, SDLK_a},
		{KEY_S, SDLK_s},
		{KEY_D, SDLK_d},
		{KEY_F, SDLK_f},
		{KEY_G, SDLK_g},
		{KEY_H, SDLK_h},
		{KEY_J, SDLK_j},
		{KEY_K, SDLK_k},
		{KEY_L, SDLK_l},
		{KEY_SEMICOLON, SDLK_SEMICOLON},
		{KEY_QUOTE, SDLK_QUOTE},
		{KEY_BACKQUOTE, SDLK_BACKQUOTE},
		{KEY_LSHIFT, SDLK_LSHIFT},
		{KEY_LMETA, SDLK_LGUI},
		{KEY_LSUPER, SDLK_LGUI},
		{KEY_BACKSLASH, SDLK_BACKSLASH},
		{KEY_Z, SDLK_z},
		{KEY_X, SDLK_x},
		{KEY_C, SDLK_c},
		{KEY_V, SDLK_v},
		{KEY_B, SDLK_b},
		{KEY_N, SDLK_n},
		{KEY_M, SDLK_m},
		{KEY_COMMA, SDLK_COMMA},
		{KEY_FULLSTOP, SDLK_PERIOD},
		{KEY_FORWARDSLASH, SDLK_SLASH},
		{KEY_RSHIFT, SDLK_RSHIFT},
		{KEY_RMETA, SDLK_RGUI},
		{KEY_RSUPER, SDLK_RGUI},
		{KEY_KP_STAR, SDLK_KP_MULTIPLY},
		{KEY_LALT, SDLK_LALT},
		{KEY_SPACE, SDLK_SPACE},
		{KEY_CAPSLOCK, SDLK_CAPSLOCK},
		{KEY_F1, SDLK_F1},
		{KEY_F2, SDLK_F2},
		{KEY_F3, SDLK_F3},
		{KEY_F4, SDLK_F4},
		{KEY_F5, SDLK_F5},
		{KEY_F6, SDLK_F6},
		{KEY_F7, SDLK_F7},
		{KEY_F8, SDLK_F8},
		{KEY_F9, SDLK_F9},
		{KEY_F10, SDLK_F10},
		{KEY_NUMLOCK, SDLK_NUMLOCKCLEAR},
		{KEY_SCROLLLOCK, SDLK_SCROLLLOCK},
		{KEY_KP_7, SDLK_KP_7},
		{KEY_KP_8, SDLK_KP_8},
		{KEY_KP_9, SDLK_KP_9},
		{KEY_KP_MINUS, SDLK_KP_MINUS},
		{KEY_KP_4, SDLK_KP_4},
		{KEY_KP_5, SDLK_KP_5},
		{KEY_KP_6, SDLK_KP_6},
		{KEY_KP_PLUS, SDLK_KP_PLUS},
		{KEY_KP_1, SDLK_KP_1},
		{KEY_KP_2, SDLK_KP_2},
		{KEY_KP_3, SDLK_KP_3},
		{KEY_KP_0, SDLK_KP_0},
		{KEY_KP_FULLSTOP, SDLK_KP_PERIOD},
		{KEY_F11, SDLK_F11},
		{KEY_F12, SDLK_F12},
		{KEY_RCTRL, SDLK_RCTRL},
		{KEY_KP_BACKSLASH, SDLK_KP_DIVIDE},
		{KEY_RALT, SDLK_RALT},
		{KEY_HOME, SDLK_HOME},
		{KEY_UPARROW, SDLK_UP},
		{KEY_PAGEUP, SDLK_PAGEUP},
		{KEY_LEFTARROW, SDLK_LEFT},
		{KEY_RIGHTARROW, SDLK_RIGHT},
		{KEY_END, SDLK_END},
		{KEY_DOWNARROW, SDLK_DOWN},
		{KEY_PAGEDOWN, SDLK_PAGEDOWN},
		{KEY_INSERT, SDLK_INSERT},
		{KEY_DELETE, SDLK_DELETE},
		{KEY_KPENTER, SDLK_KP_ENTER},
		{KEY_IGNORE, SDL_Keycode(5190)}
	};

	std::map<KEY_CODE, SDL_Keycode>::iterator it;
	for (auto &pair : KEY_CODE_to_SDLKey)
	{
		SDLKey_to_KEY_CODE.insert({pair.second, pair.first});
	}
}

static inline KEY_CODE sdlKeyToKeyCode(SDL_Keycode key)
{
	auto it = SDLKey_to_KEY_CODE.find(key);
	if (it != SDLKey_to_KEY_CODE.end())
	{
		return it->second;
	}
	return (KEY_CODE)key;
}

static inline SDL_Keycode keyCodeToSDLKey(KEY_CODE code)
{
	auto it = KEY_CODE_to_SDLKey.find(code);
	if (it != KEY_CODE_to_SDLKey.end())
	{
		return it->second;
	}
	return (SDL_Keycode)code;
}

// Cyclic increment.
static InputKey *inputPointerNext(InputKey *p)
{
	return p + 1 == pInputBuffer + INPUT_MAXSTR ? pInputBuffer : p + 1;
}

// add count copies of the character code to the input buffer
static void inputAddBuffer(UDWORD key, utf_32_char unicode)
{
	/* Calculate what pEndBuffer will be set to next */
	InputKey	*pNext = inputPointerNext(pEndBuffer);

	if (pNext == pStartBuffer)
	{
		return;	// Buffer full.
	}

	// Add key to buffer.
	pEndBuffer->key = key;
	pEndBuffer->unicode = unicode;
	pEndBuffer = pNext;
}

void keyScanToString(KEY_CODE code, char *ascii, UDWORD maxStringSize)
{
	if (code == KEY_LCTRL)
	{
		// shortcuts with modifier keys work with either key.
		strcpy(ascii, "Ctrl");
		return;
	}
	else if (code == KEY_LSHIFT)
	{
		// shortcuts with modifier keys work with either key.
		strcpy(ascii, "Shift");
		return;
	}
	else if (code == KEY_LALT)
	{
		// shortcuts with modifier keys work with either key.
		strcpy(ascii, "Alt");
		return;
	}
	else if (code == KEY_LMETA)
	{
		// shortcuts with modifier keys work with either key.
#ifdef WZ_OS_MAC
		strcpy(ascii, "Cmd");
#else
		strcpy(ascii, "Meta");
#endif
		return;
	}

	if (code < KEY_MAXSCAN)
	{
		snprintf(ascii, maxStringSize, "%s", SDL_GetKeyName(keyCodeToSDLKey(code)));
		if (ascii[0] >= 'a' && ascii[0] <= 'z' && ascii[1] != 0)
		{
			// capitalize
			ascii[0] += 'A' - 'a';
			return;
		}
	}
	else
	{
		strcpy(ascii, "???");
	}
}

/* Initialise the input module */
void inputInitialise()
{
	std::fill(std::begin(aKeyState), std::end(aKeyState), KEY_UP);
	std::fill(std::begin(aMouseState), std::end(aMouseState), MOUSE_STATE{KEY_UP, 0});

	pStartBuffer = pInputBuffer;
	pEndBuffer = pInputBuffer;

	drag = mousePos = {screenWidth/2, screenHeight/2};
	dragKey = MOUSE_LMB;

}

/* Clear the input buffer */
void inputClearBuffer(void)
{
	pStartBuffer = pInputBuffer;
	pEndBuffer = pInputBuffer;
}

/* Return the next key press or 0 if no key in the buffer.
 * The key returned will have been remapped to the correct ascii code for the
 * windows key map.
 * All key presses are buffered up (including windows auto repeat).
 */
UDWORD inputGetKey(utf_32_char *unicode)
{
	UDWORD	 retVal;

	if (pStartBuffer == pEndBuffer)
	{
		return 0;	// Buffer empty.
	}

	retVal = pStartBuffer->key;
	if (unicode)
	{
		*unicode = pStartBuffer->unicode;
	}
	if (!retVal)
	{
		retVal = ' ';  // Don't return 0 if we got a virtual key, since that's interpreted as no input.
	}
	pStartBuffer = inputPointerNext(pStartBuffer);

	return retVal;
}

Events const &inputGetEvents()
{
	return events;
}

/*!
 * This is called once a frame so that the system can tell
 * whether a key was pressed this turn or held down from the last frame.
 */
void inputNewFrame()
{
	events.clear();
}

/*!
 * Release all keys (and buttons) when we lose focus
 */
void inputLoseFocus(void)
{
	/* Lost the window focus, have to take this as a global key up */
	for (auto &state : aKeyState)
	{
		state = KEY_UP;
	}
	for (auto &state : aMouseState)
	{
		state.state = KEY_UP;
	}
}

/* This returns true if the key is currently depressed */
bool keyDown(KEY_CODE code)
{
	ASSERT_OR_RETURN(false, code < KEY_MAXSCAN, "Invalid keycode of %d!", (int)code);
	return aKeyState[code] != KEY_UP;
}

/* Return the X coordinate of the mouse */
Uint16 mouseX(void)
{
	return mousePos.x;
}

/* Return the Y coordinate of the mouse */
Uint16 mouseY(void)
{
	return mousePos.y;
}

bool wzMouseInWindow()
{
	return mouseInWindow;
}

/* This returns true if the mouse key is currently depressed */
bool mouseDown(MOUSE_KEY_CODE code)
{
	return (aMouseState[code].state != KEY_UP) ||

	       // holding down LMB and RMB counts as holding down MMB
	       (code == MOUSE_MMB && aMouseState[MOUSE_LMB].state != KEY_UP && aMouseState[MOUSE_RMB].state != KEY_UP);
}

/* Check for a mouse drag, return the drag start coords if dragging */
bool mouseDrag(MOUSE_KEY_CODE code, UDWORD *px, UDWORD *py)
{
	if ((aMouseState[code].state == KEY_DRAG) ||
	    // dragging LMB and RMB counts as dragging MMB
	    (code == MOUSE_MMB && ((aMouseState[MOUSE_LMB].state == KEY_DRAG && aMouseState[MOUSE_RMB].state != KEY_UP) ||
	                           (aMouseState[MOUSE_LMB].state != KEY_UP && aMouseState[MOUSE_RMB].state == KEY_DRAG))))
	{
		*px = drag.x;
		*py = drag.y;
		return true;
	}

	return false;
}

static decltype(Event::flags) inputFlags()
{
	decltype(Event::flags) flags = 0;
	aKeyState[KEY_LCTRL]  == KEY_DOWN && (flags |= Event::LCtrl);
	aKeyState[KEY_RCTRL]  == KEY_DOWN && (flags |= Event::RCtrl);
	aKeyState[KEY_LALT]   == KEY_DOWN && (flags |= Event::LAlt);
	aKeyState[KEY_RALT]   == KEY_DOWN && (flags |= Event::RAlt);
	aKeyState[KEY_LSHIFT] == KEY_DOWN && (flags |= Event::LShift);
	aKeyState[KEY_RSHIFT] == KEY_DOWN && (flags |= Event::RShift);
	aKeyState[KEY_LMETA]  == KEY_DOWN && (flags |= Event::LMeta);
	aKeyState[KEY_RMETA]  == KEY_DOWN && (flags |= Event::RMeta);
	return flags;
}

/*!
 * Handle keyboard events
 */
static void inputHandleKeyEvent(SDL_KeyboardEvent *keyEvent)
{
	UDWORD code = 0, vk = 0;
	switch (keyEvent->type)
	{
	case SDL_KEYDOWN:
		switch (keyEvent->keysym.sym)
		{
		// our "editing" keys for text
		case SDLK_LEFT:
			vk = INPBUF_LEFT;
			break;
		case SDLK_RIGHT:
			vk = INPBUF_RIGHT;
			break;
		case SDLK_UP:
			vk = INPBUF_UP;
			break;
		case SDLK_DOWN:
			vk = INPBUF_DOWN;
			break;
		case SDLK_HOME:
			vk = INPBUF_HOME;
			break;
		case SDLK_END:
			vk = INPBUF_END;
			break;
		case SDLK_INSERT:
			vk = INPBUF_INS;
			break;
		case SDLK_DELETE:
			vk = INPBUF_DEL;
			break;
		case SDLK_PAGEUP:
			vk = INPBUF_PGUP;
			break;
		case SDLK_PAGEDOWN:
			vk = INPBUF_PGDN;
			break;
		case KEY_BACKSPACE:
			vk = INPBUF_BKSPACE;
			break;
		case KEY_TAB:
			vk = INPBUF_TAB;
			break;
		case	KEY_RETURN:
			vk = INPBUF_CR;
			break;
		case 	KEY_ESC:
			vk = INPBUF_ESC;
			break;
		default:
			break;
		}
		// Keycodes without character representations are determined by their scancode bitwise OR-ed with 1<<30 (0x40000000).
		CurrentKey = keyEvent->keysym.sym;
		if (vk)
		{
			// Take care of 'editing' keys that were pressed
			inputAddBuffer(vk, 0);
			debug(LOG_INPUT, "Editing key: 0x%x, %d SDLkey=[%s] pressed", vk, vk, SDL_GetKeyName(CurrentKey));
		}
		else
		{
			// add everything else
			inputAddBuffer(CurrentKey, 0);
		}

		debug(LOG_INPUT, "Key Code (pressed): 0x%x, %d, [%c] SDLkey=[%s]", CurrentKey, CurrentKey, (CurrentKey < 128) && (CurrentKey > 31) ? (char)CurrentKey : '?', SDL_GetKeyName(CurrentKey));

		code = sdlKeyToKeyCode(CurrentKey);
		if (code >= KEY_MAXSCAN)
		{
			break;
		}
		aKeyState[code] = KEY_DOWN;
#ifdef KeyPress
#undef KeyPress  // Aaargh, /usr/include/X11/X.h line 181
#endif
		events.emplace_back(Event::KeyPress, code, mousePos, keyEvent->timestamp, inputFlags());
		break;

	case SDL_KEYUP:
		code = keyEvent->keysym.sym;
		debug(LOG_INPUT, "Key Code (*Depressed*): 0x%x, %d, [%c] SDLkey=[%s]", code, code, (code < 128) && (code > 31) ? (char)code : '?', SDL_GetKeyName(code));
		code = sdlKeyToKeyCode(keyEvent->keysym.sym);
		if (code >= KEY_MAXSCAN)
		{
			break;
		}
		aKeyState[code] = KEY_UP;
#ifdef KeyRelease
#undef KeyRelease  // Aaargh, /usr/include/X11/X.h line 182
#endif
		events.emplace_back(Event::KeyRelease, code, mousePos, keyEvent->timestamp, inputFlags());
		break;
	default:
		break;
	}
}

/*!
 * Handle text events (if we were to use SDL2)
*/
void inputhandleText(SDL_TextInputEvent *Tevent)
{
	size_t *newtextsize = nullptr;
	int size = SDL_strlen(Tevent->text);
	if (size)
	{
		debug(LOG_INPUT, "Keyboard: text input \"%s\"", Tevent->text);
		auto utf32Buf = UTF8toUTF32(Tevent->text, newtextsize);
		for (size_t i = 0; utf32Buf[i]; ++i)
		{
			inputAddBuffer(CurrentKey, utf32Buf[i]);
			events.emplace_back(Event::TextUnicode, utf32Buf[i], mousePos, Tevent->timestamp, inputFlags());
		}
		free(utf32Buf);
	}
}

/*!
 * Handle mouse wheel events
 */
static void inputHandleMouseWheelEvent(SDL_MouseWheelEvent *wheel)
{
	if (wheel->x > 0 || wheel->y > 0)
	{
		events.emplace_back(Event::MousePress, MOUSE_WUP, mousePos, wheel->timestamp, inputFlags());
	}
	else if (wheel->x < 0 || wheel->y < 0)
	{
		events.emplace_back(Event::MousePress, MOUSE_WDN, mousePos, wheel->timestamp, inputFlags());
	}
}

/*!
 * Handle mouse button events (We can handle up to 5)
 */
static void inputHandleMouseButtonEvent(SDL_MouseButtonEvent *buttonEvent)
{
	mousePos = {buttonEvent->x/current_displayScaleFactor, buttonEvent->y / current_displayScaleFactor};

	MOUSE_KEY_CODE mouseKeyCode;
	switch (buttonEvent->button)
	{
	case SDL_BUTTON_LEFT: mouseKeyCode = MOUSE_LMB; break;
	case SDL_BUTTON_MIDDLE: mouseKeyCode = MOUSE_MMB; break;
	case SDL_BUTTON_RIGHT: mouseKeyCode = MOUSE_RMB; break;
	case SDL_BUTTON_X1: mouseKeyCode = MOUSE_X1; break;
	case SDL_BUTTON_X2: mouseKeyCode = MOUSE_X2; break;
	default: return;  // Unknown button.
	}

	switch (buttonEvent->type)
	{
	case SDL_MOUSEBUTTONDOWN:
	{
		bool doubleClick = buttonEvent->timestamp - aMouseState[mouseKeyCode].lastdown < DOUBLE_CLICK_INTERVAL;

		events.emplace_back(doubleClick? Event::MouseDoubleClick : Event::MousePress, mouseKeyCode, mousePos, buttonEvent->timestamp, inputFlags());

		aMouseState[mouseKeyCode].state = KEY_DOWN;
		aMouseState[mouseKeyCode].lastdown = doubleClick? 0 : buttonEvent->timestamp;

		if (mouseKeyCode < MOUSE_X1) // Assume they are draggin' with either LMB|RMB|MMB
		{
			if (aMouseState[dragKey].state == KEY_DRAG)
			{
				aMouseState[dragKey].state = KEY_DOWN;
			}
			drag = mousePos;
			dragKey = mouseKeyCode;
		}
		break;
	}
	case SDL_MOUSEBUTTONUP:
		events.emplace_back(Event::MouseRelease, mouseKeyCode, mousePos, buttonEvent->timestamp, inputFlags());

		aMouseState[mouseKeyCode].state = KEY_UP;
		break;
	default:
		break;
	}
}

/*!
 * Handle mousemotion events
 */
static void inputHandleMouseMotionEvent(SDL_MouseMotionEvent *motionEvent)
{
	switch (motionEvent->type)
	{
	case SDL_MOUSEMOTION:
	{
		/* store the current mouse position */
		mousePos = {motionEvent->x/current_displayScaleFactor, motionEvent->y / current_displayScaleFactor};

		/* now see if a drag has started */
		bool isDrag = aMouseState[dragKey].state == KEY_DRAG;
		isDrag |= aMouseState[dragKey].state == KEY_DOWN &&
		    (abs(drag.x - mousePos.x) > DRAG_THRESHOLD ||
		     abs(drag.y - mousePos.y) > DRAG_THRESHOLD);
		if (isDrag)
		{
			aMouseState[dragKey].state = KEY_DRAG;
			events.emplace_back(Event::MouseDrag, MOUSE_END, mousePos, drag, motionEvent->timestamp, inputFlags());
		}
		else
		{
			events.emplace_back(Event::MouseMove, MOUSE_END, mousePos, motionEvent->timestamp, inputFlags());
		}
		break;
	}
	default:
		break;
	}
}

static int copied_argc = 0;
static char** copied_argv = nullptr;

// This stage, we only setup keycodes, and copy argc & argv for later use initializing Qt stuff for the script engine.
void wzMain(int &argc, char **argv)
{
	initKeycodes();

#if defined(WZ_OS_MAC)
	// Create copies of argc and arv (for later use initializing QApplication for the script engine)
	copied_argv = new char*[argc+1];
	for(int i=0; i < argc; i++) {
		int len = strlen(argv[i]) + 1;
		copied_argv[i] = new char[len];
		memcpy(copied_argv[i], argv[i], len);
	}
	copied_argv[argc] = NULL;
	copied_argc = argc;
#else
	// For now, just initialize QApplication here
	// We currently rely on side-effects of QApplication's initialization on Windows (such as how DPI-awareness is enabled)
	// TODO: Implement proper Win32 API calls to replicate Qt's preparation for DPI awareness (or set in the manifest?)
	appPtr = new QApplication(argc, argv);
#endif
}

#define MIN_WZ_GAMESCREEN_WIDTH 640
#define MIN_WZ_GAMESCREEN_HEIGHT 480

void handleGameScreenSizeChange(unsigned int oldWidth, unsigned int oldHeight, unsigned int newWidth, unsigned int newHeight)
{
	screenWidth = newWidth;
	screenHeight = newHeight;

	pie_SetVideoBufferWidth(screenWidth);
	pie_SetVideoBufferHeight(screenHeight);
	pie_UpdateSurfaceGeometry();
	screen_updateGeometry();

	if (currentScreenResizingStatus == nullptr)
	{
		// The screen size change details are stored in scaled, logical units (points)
		// i.e. the values expect by the game engine.
		currentScreenResizingStatus = new ScreenSizeChange(oldWidth, oldHeight, screenWidth, screenHeight);
	}
	else
	{
		// update the new screen width / height, in case more than one resize message is processed this event loop
		currentScreenResizingStatus->newWidth = screenWidth;
		currentScreenResizingStatus->newHeight = screenHeight;
	}
}

void handleWindowSizeChange(unsigned int oldWidth, unsigned int oldHeight, unsigned int newWidth, unsigned int newHeight)
{
	windowWidth = newWidth;
	windowHeight = newHeight;

	// NOTE: This function receives the window size in the window's logical units, but not accounting for the interface scale factor.
	// Therefore, the provided old/newWidth/Height must be divided by the interface scale factor to calculate the new
	// *game* screen logical width / height.
	unsigned int oldScreenWidth = oldWidth / current_displayScaleFactor;
	unsigned int oldScreenHeight = oldHeight / current_displayScaleFactor;
	unsigned int newScreenWidth = newWidth / current_displayScaleFactor;
	unsigned int newScreenHeight = newHeight / current_displayScaleFactor;

	handleGameScreenSizeChange(oldScreenWidth, oldScreenHeight, newScreenWidth, newScreenHeight);

	// Update the viewport to use the new *drawable* size (which may be greater than the new window size
	// if SDL's built-in high-DPI support is enabled and functioning).
	int drawableWidth = 0, drawableHeight = 0;
	SDL_GL_GetDrawableSize(WZwindow, &drawableWidth, &drawableHeight);
	debug(LOG_WZ, "Logical Size: %d x %d; Drawable Size: %d x %d", screenWidth, screenHeight, drawableWidth, drawableHeight);
	glViewport(0, 0, drawableWidth, drawableHeight);
	glCullFace(GL_FRONT);
	glEnable(GL_CULL_FACE);
}


void wzGetMinimumWindowSizeForDisplayScaleFactor(unsigned int *minWindowWidth, unsigned int *minWindowHeight, float displayScaleFactor = current_displayScaleFactor)
{
	if (minWindowWidth != nullptr)
	{
		*minWindowWidth = (int)ceil(MIN_WZ_GAMESCREEN_WIDTH * displayScaleFactor);
	}
	if (minWindowHeight != nullptr)
	{
		*minWindowHeight = (int)ceil(MIN_WZ_GAMESCREEN_HEIGHT * displayScaleFactor);
	}
}

void wzGetMaximumDisplayScaleFactorsForWindowSize(unsigned int windowWidth, unsigned int windowHeight, float *horizScaleFactor, float *vertScaleFactor)
{
	if (horizScaleFactor != nullptr)
	{
		*horizScaleFactor = (float)windowWidth / (float)MIN_WZ_GAMESCREEN_WIDTH;
	}
	if (vertScaleFactor != nullptr)
	{
		*vertScaleFactor = (float)windowHeight / (float)MIN_WZ_GAMESCREEN_HEIGHT;
	}
}

float wzGetMaximumDisplayScaleFactorForWindowSize(unsigned int windowWidth, unsigned int windowHeight)
{
	float maxHorizScaleFactor = 0.f, maxVertScaleFactor = 0.f;
	wzGetMaximumDisplayScaleFactorsForWindowSize(windowWidth, windowHeight, &maxHorizScaleFactor, &maxVertScaleFactor);
	return std::min(maxHorizScaleFactor, maxVertScaleFactor);
}

// returns: the maximum display scale percentage (sourced from wzAvailableDisplayScales), or 0 if window is below the minimum required size for the minimum support display scale
unsigned int wzGetMaximumDisplayScaleForWindowSize(unsigned int windowWidth, unsigned int windowHeight)
{
	float maxDisplayScaleFactor = wzGetMaximumDisplayScaleFactorForWindowSize(windowWidth, windowHeight);
	unsigned int maxDisplayScalePercentage = floor(maxDisplayScaleFactor * 100.f);

	auto availableDisplayScales = wzAvailableDisplayScales();
	std::sort(availableDisplayScales.begin(), availableDisplayScales.end());

	auto maxDisplayScale = std::lower_bound(availableDisplayScales.begin(), availableDisplayScales.end(), maxDisplayScalePercentage);
	if (maxDisplayScale == availableDisplayScales.end())
	{
		return 0;
	}
	if (*maxDisplayScale != maxDisplayScalePercentage)
	{
		--maxDisplayScale;
	}
	return *maxDisplayScale;
}

bool wzWindowSizeIsSmallerThanMinimumRequired(unsigned int windowWidth, unsigned int windowHeight, float displayScaleFactor = current_displayScaleFactor)
{
	unsigned int minWindowWidth = 0, minWindowHeight = 0;
	wzGetMinimumWindowSizeForDisplayScaleFactor(&minWindowWidth, &minWindowHeight, displayScaleFactor);
	return ((windowWidth < minWindowWidth) || (windowHeight < minWindowHeight));
}

void processScreenSizeChangeNotificationIfNeeded()
{
	if (currentScreenResizingStatus != nullptr)
	{
		// WZ must process the screen size change
		gameScreenSizeDidChange(currentScreenResizingStatus->oldWidth, currentScreenResizingStatus->oldHeight, currentScreenResizingStatus->newWidth, currentScreenResizingStatus->newHeight);
		delete currentScreenResizingStatus;
		currentScreenResizingStatus = nullptr;
	}
}

bool wzChangeDisplayScale(unsigned int displayScale)
{
	float newDisplayScaleFactor = (float)displayScale / 100.f;

	if (wzWindowSizeIsSmallerThanMinimumRequired(windowWidth, windowHeight, newDisplayScaleFactor))
	{
		// The current window width and/or height are below the required minimum window size
		// for this display scale factor.
		return false;
	}

	// Store the new display scale factor
	setDisplayScale(displayScale);

	// Set the new minimum window size
	unsigned int minWindowWidth = 0, minWindowHeight = 0;
	wzGetMinimumWindowSizeForDisplayScaleFactor(&minWindowWidth, &minWindowHeight, newDisplayScaleFactor);
	SDL_SetWindowMinimumSize(WZwindow, minWindowWidth, minWindowHeight);

	// Update the game's logical screen size
	unsigned int oldScreenWidth = screenWidth, oldScreenHeight = screenHeight;
	unsigned int newScreenWidth = windowWidth, newScreenHeight = windowHeight;
	if (newDisplayScaleFactor > 1.0)
	{
		newScreenWidth = windowWidth / newDisplayScaleFactor;
		newScreenHeight = windowHeight / newDisplayScaleFactor;
	}
	handleGameScreenSizeChange(oldScreenWidth, oldScreenHeight, newScreenWidth, newScreenHeight);
	gameDisplayScaleFactorDidChange(newDisplayScaleFactor);

	// Update the current mouse coordinates
	// (The prior stored mouseXPos / mouseYPos apply to the old coordinate system, and must be translated to the
	// new game coordinate system. Since the mouse hasn't moved - or it would generate events that override this -
	// the current position with respect to the window (which hasn't changed size) can be queried and used to
	// calculate the new game coordinate system mouse position.)
	//
	Vector2i windowMousePos = {0, 0};
	SDL_GetMouseState(&windowMousePos.x, &windowMousePos.y);
	debug(LOG_WZ, "Old mouse position: %d, %d", mousePos.x, mousePos.y);
	mousePos = {windowMousePos.x/current_displayScaleFactor, windowMousePos.y/current_displayScaleFactor};
	debug(LOG_WZ, "New mouse position: %d, %d", mousePos.x, mousePos.y);

	processScreenSizeChangeNotificationIfNeeded();

	return true;
}

bool wzChangeWindowResolution(int screen, unsigned int width, unsigned int height)
{
	assert(WZwindow != nullptr);
	debug(LOG_WZ, "Attempt to change resolution to [%d] %dx%d", screen, width, height);

#if defined(WZ_OS_MAC)
	// Workaround for SDL (2.0.5) quirk on macOS:
	//	When the green titlebar button is used to fullscreen the app in a new space:
	//		- SDL does not return SDL_WINDOW_MAXIMIZED nor SDL_WINDOW_FULLSCREEN.
	//		- Attempting to change the window resolution "succeeds" (in that the new window size is "set" and returned
	//		  by the SDL GetWindowSize functions).
	//		- But other things break (ex. mouse coordinate translation) if the resolution is changed while the window
	//        is maximized in this way.
	//		- And the GL drawable size remains unchanged.
	//		- So if it's been fullscreened by the user like this, but doesn't show as SDL_WINDOW_FULLSCREEN,
	//		  prevent window resolution changes.
	if (cocoaIsSDLWindowFullscreened(WZwindow) && !wzIsFullscreen())
	{
		debug(LOG_WZ, "The main window is fullscreened, but SDL doesn't think it is. Changing window resolution is not possible in this state. (SDL Bug).");
		return false;
	}
#endif

	// Get current window size + position + bounds
	int prev_x = 0, prev_y = 0, prev_width = 0, prev_height = 0;
	SDL_GetWindowPosition(WZwindow, &prev_x, &prev_y);
	SDL_GetWindowSize(WZwindow, &prev_width, &prev_height);

	// Get the usable bounds for the current screen
	SDL_Rect bounds;
	if (wzIsFullscreen())
	{
		// When in fullscreen mode, obtain the screen's overall bounds
		if (SDL_GetDisplayBounds(screen, &bounds) != 0) {
			debug(LOG_ERROR, "Failed to get display bounds for screen: %d", screen);
			return false;
		}
		debug(LOG_WZ, "SDL_GetDisplayBounds for screen [%d]: pos %d x %d : res %d x %d", screen, (int)bounds.x, (int)bounds.y, (int)bounds.w, (int)bounds.h);
	}
	else
	{
		// When in windowed mode, obtain the screen's *usable* display bounds
		if (SDL_GetDisplayUsableBounds(screen, &bounds) != 0) {
			debug(LOG_ERROR, "Failed to get usable display bounds for screen: %d", screen);
			return false;
		}
		debug(LOG_WZ, "SDL_GetDisplayUsableBounds for screen [%d]: pos %d x %d : WxH %d x %d", screen, (int)bounds.x, (int)bounds.y, (int)bounds.w, (int)bounds.h);

		// Verify that the desired window size does not exceed the usable bounds of the specified display.
		if (width > (unsigned)bounds.w || height > (unsigned)bounds.h)
		{
			debug(LOG_WZ, "Unable to change window size to (%d x %d) because it is larger than the screen's usable bounds", width, height);
			return false;
		}
	}

	// Check whether the desired window size is smaller than the minimum required for the current Display Scale
	unsigned int priorDisplayScale = current_displayScale;
	if (wzWindowSizeIsSmallerThanMinimumRequired(width, height))
	{
		// The new window size is smaller than the minimum required size for the current display scale level.

		unsigned int maxDisplayScale = wzGetMaximumDisplayScaleForWindowSize(width, height);
		if (maxDisplayScale < 100)
		{
			// Cannot adjust display scale factor below 1. Desired window size is below the minimum supported.
			debug(LOG_WZ, "Unable to change window size to (%d x %d) because it is smaller than the minimum supported at a 100%% display scale", width, height);
			return false;
		}

		// Adjust the current display scale level to the nearest supported level.
		debug(LOG_WZ, "The current Display Scale (%d%%) is too high for the desired window size. Reducing the current Display Scale to the maximum possible for the desired window size: %d%%.", current_displayScale, maxDisplayScale);
		wzChangeDisplayScale(maxDisplayScale);

		// Store the new display scale
		war_SetDisplayScale(maxDisplayScale);
	}

	// Position the window (centered) on the screen (for its upcoming new size)
	SDL_SetWindowPosition(WZwindow, SDL_WINDOWPOS_CENTERED_DISPLAY(screen), SDL_WINDOWPOS_CENTERED_DISPLAY(screen));

	// Change the window size
	// NOTE: Changing the window size will trigger an SDL window size changed event which will handle recalculating layout.
	SDL_SetWindowSize(WZwindow, width, height);

	// Check that the new size is the desired size
	int resultingWidth, resultingHeight = 0;
	SDL_GetWindowSize(WZwindow, &resultingWidth, &resultingHeight);
	if ((unsigned)resultingWidth != width || (unsigned)resultingHeight != height) {
		// Attempting to set the resolution failed
		debug(LOG_WZ, "Attempting to change the resolution to %dx%d seems to have failed (result: %dx%d).", width, height, resultingWidth, resultingHeight);

		// Revert to the prior position + resolution + display scale, and return false
		SDL_SetWindowSize(WZwindow, prev_width, prev_height);
		SDL_SetWindowPosition(WZwindow, prev_x, prev_y);
		if (current_displayScale != priorDisplayScale)
		{
			// Reverse the correction applied to the Display Scale to support the desired resolution.
			wzChangeDisplayScale(priorDisplayScale);
			war_SetDisplayScale(priorDisplayScale);
		}
		return false;
	}

	// Store the updated screenIndex
	screenIndex = screen;

	return true;
}

// Returns the current window screen, width, and height
void wzGetWindowResolution(int *screen, unsigned int *width, unsigned int *height)
{
	if (screen != nullptr)
	{
		*screen = screenIndex;
	}

	int currentWidth = 0, currentHeight = 0;
	SDL_GetWindowSize(WZwindow, &currentWidth, &currentHeight);
	assert(currentWidth >= 0);
	assert(currentHeight >= 0);
	if (width != nullptr)
	{
		*width = currentWidth;
	}
	if (height != nullptr)
	{
		*height = currentHeight;
	}
}

// This stage, we handle display mode setting
bool wzMainScreenSetup(int antialiasing, bool fullscreen, bool vsync, bool highDPI)
{
	// populate with the saved values (if we had any)
	// NOTE: Prior to wzMainScreenSetup being run, the display system is populated with the window width + height
	// (i.e. not taking into account the game display scale). This function later sets the display system
	// to the *game screen* width and height (taking into account the display scale).
	int width = pie_GetVideoBufferWidth();
	int height = pie_GetVideoBufferHeight();
	int bitDepth = pie_GetVideoBufferDepth();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
	{
		debug(LOG_ERROR, "Error: Could not initialise SDL (%s).", SDL_GetError());
		return false;
	}

	wzSDLAppEvent = SDL_RegisterEvents(1);
	if (wzSDLAppEvent == ((Uint32)-1)) {
		// Failed to register app-defined event with SDL
		debug(LOG_ERROR, "Error: Failed to register app-defined SDL event (%s).", SDL_GetError());
		return false;
	}

#if defined(WZ_OS_MAC)
	// on macOS, support maximizing to a fullscreen space (modern behavior)
	if (SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "1") == SDL_FALSE)
	{
		debug(LOG_WARNING, "Failed to set hint: SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES");
	}
#endif

	// Set the double buffer OpenGL attribute.
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	// Enable stencil buffer, needed for shadows to work.
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	if (antialiasing)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, antialiasing);
	}

#if defined(WZ_USE_OPENGL_3_2_CORE_PROFILE)
	// SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG is *required* to obtain an OpenGL >= 3 Core Context on macOS
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#endif

	// Populated our resolution list (does all displays now)
	SDL_DisplayMode	displaymode;
	struct screeninfo screenlist;
	for (int i = 0; i < SDL_GetNumVideoDisplays(); ++i)		// How many monitors we got
	{
		int numdisplaymodes = SDL_GetNumDisplayModes(i);	// Get the number of display modes on this monitor
		for (int j = 0; j < numdisplaymodes; j++)
		{
			displaymode.format = displaymode.w = displaymode.h = displaymode.refresh_rate = 0;
			displaymode.driverdata = 0;
			if (SDL_GetDisplayMode(i, j, &displaymode) < 0)
			{
				debug(LOG_FATAL, "SDL_LOG_CATEGORY_APPLICATION error:%s", SDL_GetError());
				SDL_Quit();
				exit(EXIT_FAILURE);
			}

			debug(LOG_WZ, "Monitor [%d] %dx%d %d %s", i, displaymode.w, displaymode.h, displaymode.refresh_rate, SDL_GetPixelFormatName(displaymode.format));
			if ((displaymode.w < MIN_WZ_GAMESCREEN_WIDTH) || (displaymode.h < MIN_WZ_GAMESCREEN_HEIGHT))
			{
				debug(LOG_WZ, "Monitor mode resolution < %d x %d -- discarding entry", MIN_WZ_GAMESCREEN_WIDTH, MIN_WZ_GAMESCREEN_HEIGHT);
			}
			else if (displaymode.refresh_rate < 59)
			{
				debug(LOG_WZ, "Monitor mode refresh rate < 59 -- discarding entry");
				// only store 60Hz & higher modes, some display report 59 on Linux
			}
			else
			{
				screenlist.width = displaymode.w;
				screenlist.height = displaymode.h;
				screenlist.refresh_rate = displaymode.refresh_rate;
				screenlist.screen = i;		// which monitor this belongs to
				displaylist.push_back(screenlist);
			}
		}
	}

	SDL_DisplayMode current = { 0, 0, 0, 0, 0 };
	for (int i = 0; i < SDL_GetNumVideoDisplays(); ++i)
	{
		int display = SDL_GetCurrentDisplayMode(i, &current);
		if (display != 0)
		{
			debug(LOG_FATAL, "Can't get the current display mode, because: %s", SDL_GetError());
			SDL_Quit();
			exit(EXIT_FAILURE);
		}
		debug(LOG_WZ, "Monitor [%d] %dx%d %d", i, current.w, current.h, current.refresh_rate);
	}

	if (width == 0 || height == 0)
	{
		width = windowWidth = current.w;
		height = windowHeight = current.h;
	}
	else
	{
		windowWidth = width;
		windowHeight = height;
	}

	setDisplayScale(war_GetDisplayScale());

	// Calculate the minimum window size given the current display scale
	unsigned int minWindowWidth = 0, minWindowHeight = 0;
	wzGetMinimumWindowSizeForDisplayScaleFactor(&minWindowWidth, &minWindowHeight);

	if ((windowWidth < minWindowWidth) || (windowHeight < minWindowHeight))
	{
		// The current window width and/or height is lower than the required minimum for the current display scale.
		//
		// Reset the display scale to 100%, and recalculate the required minimum window size.
		setDisplayScale(100);
		war_SetDisplayScale(100); // save the new display scale configuration
		wzGetMinimumWindowSizeForDisplayScaleFactor(&minWindowWidth, &minWindowHeight);
	}

	windowWidth = MAX(windowWidth, minWindowWidth);
	windowHeight = MAX(windowHeight, minWindowHeight);

	//// The flags to pass to SDL_CreateWindow
	int video_flags  = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;

	if (fullscreen)
	{
		video_flags |= WZ_SDL_FULLSCREEN_MODE;
	}
	else
	{
		// Allow the window to be manually resized, if not fullscreen
		video_flags |= SDL_WINDOW_RESIZABLE;
	}

	if (highDPI)
	{
#if defined(WZ_OS_MAC)
		// Allow SDL to enable its built-in High-DPI display support.
		// As of SDL 2.0.5, this only works on macOS. (But SDL 2.1.x+ may enable Windows support via a different interface.)
		video_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
	}

	SDL_Rect bounds;
	for (int i = 0; i < SDL_GetNumVideoDisplays(); i++)
	{
		SDL_GetDisplayBounds(i, &bounds);
		debug(LOG_WZ, "Monitor %d: pos %d x %d : res %d x %d", i, (int)bounds.x, (int)bounds.y, (int)bounds.w, (int)bounds.h);
	}
	screenIndex = war_GetScreen();
	const int currentNumDisplays = SDL_GetNumVideoDisplays();
	if (currentNumDisplays < 1)
	{
		debug(LOG_FATAL, "SDL_GetNumVideoDisplays returned: %d, with error: %s", currentNumDisplays, SDL_GetError());
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
	if (screenIndex > currentNumDisplays)
	{
		debug(LOG_WARNING, "Invalid screen [%d] defined in configuration; there are only %d displays; falling back to display 0", screenIndex, currentNumDisplays);
		screenIndex = 0;
		war_SetScreen(0);
	}
	WZwindow = SDL_CreateWindow(PACKAGE_NAME, SDL_WINDOWPOS_CENTERED_DISPLAY(screenIndex), SDL_WINDOWPOS_CENTERED_DISPLAY(screenIndex), windowWidth, windowHeight, video_flags);

	if (!WZwindow)
	{
		debug(LOG_FATAL, "Can't create a window, because: %s", SDL_GetError());
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	// Check that the actual window size matches the desired window size
	int resultingWidth, resultingHeight = 0;
	SDL_GetWindowSize(WZwindow, &resultingWidth, &resultingHeight);
	if ((unsigned)resultingWidth != windowWidth || (unsigned)resultingHeight != windowHeight) {
		// Failed to create window at desired size (This can happen for a number of reasons)
		debug(LOG_ERROR, "Failed to create window at desired resolution: [%d] %d x %d; instead, received window of resolution: [%d] %d x %d; Reverting to default resolution of %d x %d", war_GetScreen(), windowWidth, windowHeight, war_GetScreen(), resultingWidth, resultingHeight, minWindowWidth, minWindowHeight);

		// Default to base resolution
		SDL_SetWindowSize(WZwindow, minWindowWidth, minWindowHeight);
		windowWidth = minWindowWidth;
		windowHeight = minWindowHeight;

		// Center window on screen
		SDL_SetWindowPosition(WZwindow, SDL_WINDOWPOS_CENTERED_DISPLAY(screenIndex), SDL_WINDOWPOS_CENTERED_DISPLAY(screenIndex));
	}

	// Calculate the game screen's logical dimensions
	screenWidth = windowWidth;
	screenHeight = windowHeight;
	if (current_displayScaleFactor > 1.0f)
	{
		screenWidth = windowWidth / current_displayScaleFactor;
		screenHeight = windowHeight / current_displayScaleFactor;
	}
	pie_SetVideoBufferWidth(screenWidth);
	pie_SetVideoBufferHeight(screenHeight);

	// Set the minimum window size
	SDL_SetWindowMinimumSize(WZwindow, minWindowWidth, minWindowHeight);

	WZglcontext = SDL_GL_CreateContext(WZwindow);
	if (!WZglcontext)
	{
		debug(LOG_ERROR, "Failed to create a openGL context! [%s]", SDL_GetError());
		return false;
	}

	if (highDPI)
	{
		// When high-DPI mode is enabled, retrieve the DrawableSize in pixels
		// for use in the glViewport function - this will be the actual
		// pixel dimensions, not the window size (which is in points).
		//
		// NOTE: Do not do this if high-DPI support is disabled, or the viewport
		// size may be set inappropriately.

		SDL_GL_GetDrawableSize(WZwindow, &width, &height);
		debug(LOG_WZ, "Logical Size: %d x %d; Drawable Size: %d x %d", windowWidth, windowHeight, width, height);
	}

	int bpp = SDL_BITSPERPIXEL(SDL_GetWindowPixelFormat(WZwindow));
	debug(LOG_WZ, "Bpp = %d format %s" , bpp, SDL_GetPixelFormatName(SDL_GetWindowPixelFormat(WZwindow)));
	if (!bpp)
	{
		debug(LOG_ERROR, "Video mode %dx%d@%dbpp is not supported!", width, height, bitDepth);
		return false;
	}
	switch (bpp)
	{
	case 32:
	case 24:		// all is good...
		break;
	case 16:
		info("Using colour depth of %i instead of a 32/24 bit depth (True color).", bpp);
		info("You will experience graphics glitches!");
		break;
	case 8:
		debug(LOG_FATAL, "You don't want to play Warzone with a bit depth of %i, do you?", bpp);
		SDL_Quit();
		exit(1);
		break;
	default:
		debug(LOG_FATAL, "Unsupported bit depth: %i", bpp);
		exit(1);
		break;
	}

	// Enable/disable vsync if requested by the user
	wzSetSwapInterval(vsync);

	int value = 0;
	if (SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &value) == -1 || value == 0)
	{
		debug(LOG_FATAL, "OpenGL initialization did not give double buffering!");
		debug(LOG_FATAL, "Double buffering is required for this game!");
		SDL_Quit();
		exit(1);
	}

#if !defined(WZ_OS_MAC) // Do not use this method to set the window icon on macOS.

	#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		uint32_t rmask = 0xff000000;
		uint32_t gmask = 0x00ff0000;
		uint32_t bmask = 0x0000ff00;
		uint32_t amask = 0x000000ff;
	#else
		uint32_t rmask = 0x000000ff;
		uint32_t gmask = 0x0000ff00;
		uint32_t bmask = 0x00ff0000;
		uint32_t amask = 0xff000000;
	#endif

#if defined(__GNUC__)
	#pragma GCC diagnostic push
	// ignore warning: cast from type 'const unsigned char*' to type 'void*' casts away qualifiers [-Wcast-qual]
	// FIXME?
	#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
	SDL_Surface *surface_icon = SDL_CreateRGBSurfaceFrom((void *)wz2100icon.pixel_data, wz2100icon.width, wz2100icon.height, wz2100icon.bytes_per_pixel * 8,
	                            wz2100icon.width * wz2100icon.bytes_per_pixel, rmask, gmask, bmask, amask);
#if defined(__GNUC__)
	#pragma GCC diagnostic pop
#endif

	if (surface_icon)
	{
		SDL_SetWindowIcon(WZwindow, surface_icon);
		SDL_FreeSurface(surface_icon);
	}
	else
	{
		debug(LOG_ERROR, "Could not set window icon because %s", SDL_GetError());
	}
#endif

	SDL_SetWindowTitle(WZwindow, PACKAGE_NAME);

	/* initialise all cursors */
	if (war_GetColouredCursor())
	{
		sdlInitColoredCursors();
	}
	else
	{
		sdlInitCursors();
	}

#if defined(WZ_OS_MAC)
	// For the script engine, let Qt know we're alive
	//
	// IMPORTANT: This must come *after* SDL has had a chance to initialize,
	//			  or Qt can step on certain SDL functionality.
	//			  (For example, on macOS, Qt can break the "Quit" menu
	//			  functionality if QApplication is initialized before SDL.)
	appPtr = new QApplication(copied_argc, copied_argv);

	// IMPORTANT: Because QApplication calls setlocale(LC_ALL,""),
	//			  we *must* immediately call setlocale(LC_NUMERIC,"C") after initializing
	//			  or things like loading (parsing) levels / resources can fail
	setlocale(LC_NUMERIC, "C"); // set radix character to the period (".")
#endif

#if defined(WZ_OS_MAC)
	cocoaSetupWZMenus();
#endif

	// FIXME: aspect ratio
	glViewport(0, 0, width, height);
	glCullFace(GL_FRONT);
	glEnable(GL_CULL_FACE);

	return true;
}


// Calculates and returns the scale factor from the SDL window's coordinate system (in points) to the raw
// underlying pixels of the viewport / renderer.
//
// IMPORTANT: This value is *non-inclusive* of any user-configured Game Display Scale.
//
// This exposes what is effectively the SDL window's "High-DPI Scale Factor", if SDL's high-DPI support is enabled and functioning.
//
// In the normal, non-high-DPI-supported case, (in which the context's drawable size in pixels and the window's logical
// size in points are equal) this will return 1.0 for both values.
//
void wzGetWindowToRendererScaleFactor(float *horizScaleFactor, float *vertScaleFactor)
{
	assert(WZwindow != nullptr);

	// Obtain the window context's drawable size in pixels
	int drawableWidth, drawableHeight = 0;
	SDL_GL_GetDrawableSize(WZwindow, &drawableWidth, &drawableHeight);

	// Obtain the logical window size (in points)
	int windowWidth, windowHeight = 0;
	SDL_GetWindowSize(WZwindow, &windowWidth, &windowHeight);

	debug(LOG_WZ, "Window Logical Size (%d, %d) vs Drawable Size in Pixels (%d, %d)", windowWidth, windowHeight, drawableWidth, drawableHeight);

	if (horizScaleFactor != nullptr)
	{
		*horizScaleFactor = ((float)drawableWidth / (float)windowWidth) * current_displayScaleFactor;
	}
	if (vertScaleFactor != nullptr)
	{
		*vertScaleFactor = ((float)drawableHeight / (float)windowHeight) * current_displayScaleFactor;
	}

	int displayIndex = SDL_GetWindowDisplayIndex(WZwindow);
	if (displayIndex >= 0)
	{
		float hdpi, vdpi;
		if (SDL_GetDisplayDPI(displayIndex, nullptr, &hdpi, &vdpi) < 0)
		{
			debug(LOG_WARNING, "Failed to get the display (%d) DPI because : %s", displayIndex, SDL_GetError());
		}
		else
		{
			debug(LOG_WZ, "Display (%d) DPI: %f, %f", displayIndex, hdpi, vdpi);
		}
	}
	else
	{
		debug(LOG_WARNING, "Failed to get the display index for the window because : %s", SDL_GetError());
	}
}

// Calculates and returns the total scale factor from the game's coordinate system (in points)
// to the raw underlying pixels of the viewport / renderer.
//
// IMPORTANT: This value is *inclusive* of both the user-configured "Display Scale" *AND* any underlying
// high-DPI / "Retina" display support provided by SDL.
//
// It is equivalent to: (SDL Window's High-DPI Scale Factor) x (WZ Game Display Scale Factor)
//
// Therefore, if SDL is providing a supported high-DPI window / context, this value will be greater
// than the WZ (user-configured) Display Scale Factor.
//
// It should be used only for internal (non-user-displayed) cases in which the full scaling factor from
// the game system's coordinate system (in points) to the underlying display pixels is required.
// (For example, when rasterizing text for best display.)
//
void wzGetGameToRendererScaleFactor(float *horizScaleFactor, float *vertScaleFactor)
{
	float horizWindowScaleFactor = 0.f, vertWindowScaleFactor = 0.f;
	wzGetWindowToRendererScaleFactor(&horizWindowScaleFactor, &vertWindowScaleFactor);
	assert(horizWindowScaleFactor != 0.f);
	assert(vertWindowScaleFactor != 0.f);

	if (horizScaleFactor != nullptr)
	{
		*horizScaleFactor = horizWindowScaleFactor * current_displayScaleFactor;
	}
	if (vertScaleFactor != nullptr)
	{
		*vertScaleFactor = vertWindowScaleFactor * current_displayScaleFactor;
	}
}

void wzSetWindowIsResizable(bool resizable)
{
	assert(WZwindow != nullptr);
	SDL_bool sdl_resizable = (resizable) ? SDL_TRUE : SDL_FALSE;
	SDL_SetWindowResizable(WZwindow, sdl_resizable);
}

bool wzIsWindowResizable()
{
	Uint32 flags = SDL_GetWindowFlags(WZwindow);
	if (flags & SDL_WINDOW_RESIZABLE)
	{
		return true;
	}
	return false;
}

bool wzSupportsLiveResolutionChanges()
{
	return true;
}

/*!
 * Activation (focus change ... and) eventhandler.  Mainly for debugging.
 */
static void handleActiveEvent(SDL_Event *event)
{
	if (event->type == SDL_WINDOWEVENT)
	{
		switch (event->window.event)
		{
		case SDL_WINDOWEVENT_SHOWN:
			debug(LOG_WZ, "Window %d shown", event->window.windowID);
			break;
		case SDL_WINDOWEVENT_HIDDEN:
			debug(LOG_WZ, "Window %d hidden", event->window.windowID);
			break;
		case SDL_WINDOWEVENT_EXPOSED:
			debug(LOG_WZ, "Window %d exposed", event->window.windowID);
			break;
		case SDL_WINDOWEVENT_MOVED:
			debug(LOG_WZ, "Window %d moved to %d,%d", event->window.windowID, event->window.data1, event->window.data2);
				// FIXME: Handle detecting which screen the window was moved to, and update saved war_SetScreen?
			break;
		case SDL_WINDOWEVENT_RESIZED:
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			debug(LOG_WZ, "Window %d resized to %dx%d", event->window.windowID, event->window.data1, event->window.data2);
			{
				unsigned int oldWindowWidth = windowWidth;
				unsigned int oldWindowHeight = windowHeight;

				Uint32 windowFlags = SDL_GetWindowFlags(WZwindow);
				debug(LOG_WZ, "Window resized to window flags: %u", windowFlags);

				int newWindowWidth = 0, newWindowHeight = 0;
				SDL_GetWindowSize(WZwindow, &newWindowWidth, &newWindowHeight);

				if ((event->window.data1 != newWindowWidth) || (event->window.data2 != newWindowHeight))
				{
					// This can happen - so we use the values retrieved from SDL_GetWindowSize in any case - but
					// log it for tracking down the SDL-related causes later.
					debug(LOG_WARNING, "Received width and height (%d x %d) do not match those from GetWindowSize (%d x %d)", event->window.data1, event->window.data2, newWindowWidth, newWindowHeight);
				}

				handleWindowSizeChange(oldWindowWidth, oldWindowHeight, newWindowWidth, newWindowHeight);

				// Store the new values (in case the user manually resized the window bounds)
				war_SetWidth(newWindowWidth);
				war_SetHeight(newWindowHeight);
			}
			break;
		case SDL_WINDOWEVENT_MINIMIZED:
			debug(LOG_WZ, "Window %d minimized", event->window.windowID);
			break;
		case SDL_WINDOWEVENT_MAXIMIZED:
			debug(LOG_WZ, "Window %d maximized", event->window.windowID);
			break;
		case SDL_WINDOWEVENT_RESTORED:
			debug(LOG_WZ, "Window %d restored", event->window.windowID);
			break;
		case SDL_WINDOWEVENT_ENTER:
			debug(LOG_WZ, "Mouse entered window %d", event->window.windowID);
			break;
		case SDL_WINDOWEVENT_LEAVE:
			debug(LOG_WZ, "Mouse left window %d", event->window.windowID);
			break;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			mouseInWindow = SDL_TRUE;
			debug(LOG_WZ, "Window %d gained keyboard focus", event->window.windowID);
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			mouseInWindow = SDL_FALSE;
			debug(LOG_WZ, "Window %d lost keyboard focus", event->window.windowID);
			break;
		case SDL_WINDOWEVENT_CLOSE:
			debug(LOG_WZ, "Window %d closed", event->window.windowID);
			break;
		default:
			debug(LOG_WZ, "Window %d got unknown event %d", event->window.windowID, event->window.event);
			break;
		}
	}
}

// Actual mainloop
void wzMainEventLoop(void)
{
	SDL_Event event;

	while (true)
	{
		/* Deal with any windows messages */
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_KEYUP:
			case SDL_KEYDOWN:
				inputHandleKeyEvent(&event.key);
				break;
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
				inputHandleMouseButtonEvent(&event.button);
				break;
			case SDL_MOUSEMOTION:
				inputHandleMouseMotionEvent(&event.motion);
				break;
			case SDL_MOUSEWHEEL:
				inputHandleMouseWheelEvent(&event.wheel);
				break;
			case SDL_WINDOWEVENT:
				handleActiveEvent(&event);
				break;
			case SDL_TEXTINPUT:	// SDL now handles text input differently
				inputhandleText(&event.text);
				break;
			case SDL_QUIT:
				return;
			default:
				break;
			}

			if (wzSDLAppEvent == event.type)
			{
				// Custom WZ App Event
				switch (event.user.code)
				{
					case wzSDLAppEventCodes::MAINTHREADEXEC:
						if (event.user.data1 != nullptr)
						{
							WZ_MAINTHREADEXEC * pExec = static_cast<WZ_MAINTHREADEXEC *>(event.user.data1);
							pExec->doExecOnMainThread();
							delete pExec;
						}
						break;
					default:
						break;
				}
			}
		}
#if !defined(WZ_OS_WIN) && !defined(WZ_OS_MAC)
		// Ideally, we don't want Qt processing events in addition to SDL - this causes
		// all kinds of issues (crashes taking screenshots on Windows, freezing on
		// macOS without a nasty workaround) - but without the following line the script
		// debugger window won't display properly on Linux.
		//
		// Therefore, do not include it on Windows and macOS builds, which does not
		// impact the script debugger's functionality, but include it (for now) on other
		// builds until an alternative script debugger UI is available.
		//
		appPtr->processEvents();		// Qt needs to do its stuff
#endif
		processScreenSizeChangeNotificationIfNeeded();
		events.emplace_back(Event::FrameNew, mousePos, inputFlags());
		mainLoop();				// WZ does its thing
		inputNewFrame();			// reset input states
	}
}

void wzShutdown()
{
	// order is important!
	sdlFreeCursors();
	SDL_DestroyWindow(WZwindow);
	SDL_Quit();
	appPtr->quit();
	delete appPtr;
	appPtr = nullptr;

	// delete copies of argc, argv
	if (copied_argv != nullptr)
	{
		for(int i=0; i < copied_argc; i++) {
			delete [] copied_argv[i];
		}
		delete [] copied_argv;
		copied_argv = nullptr;
		copied_argc = 0;
	}
}
