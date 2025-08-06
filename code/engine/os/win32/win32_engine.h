#ifndef WIN32_ENGINE_H
#define WIN32_ENGINE_H

#include <engine.h>
#include <dsound.h>
#include <windowsx.h>

#define WIN32_GAMEPAD_USAGE 5
#define WIN32_JOYSTICK_USAGE 4
#define WIN32_MOUSE_USAGE 2
#define WIN32_USAGE_PAGE 1

#define WIN32_SOUND_HZ (1.0/30.0)
#define WIN32_MAX_SOUND_LATENCY 10

#define MAX_FUNCTION_COUNT 32
struct win32_dll {
	os_library*    Library;
	os_hot_reload* HotReload;
	string 	   	   DLLPath;
	string 	   	   TmpDLLPath;
	size_t 		   FunctionCount;
	string 		   FunctionNames[MAX_FUNCTION_COUNT];
	void*  		   Functions[MAX_FUNCTION_COUNT];
	os_rw_mutex*   RWLock;
};

struct win32_audio {
	IDirectSoundBuffer* SoundBuffer;
	DWORD CurrentPosition;
	DWORD LastWriteCursor;
	u32 LatencyCount;
	u32 SamplesPerFrame;
	u32 BufferSize;
	b32 FirstFrame;
};

struct win32_engine : public engine {
	win32_audio Audio;
	win32_dll   EngineDLL;
	HWND MainWindow;
	job_id RootJob;
};

#endif