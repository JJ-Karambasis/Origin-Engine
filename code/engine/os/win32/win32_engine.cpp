#include "win32_engine.h"

#include <accumulator_loop.cpp>

#include <os/os_events.cpp>

function inline win32_engine* Win32_Get() {
	win32_engine* Result = (win32_engine*)Get_Engine();
	Assert(Result);
	return Result;
}

function inline v2 Win32_Get_Client_Dim(HWND Window) {
	RECT Rect;
	GetClientRect(Window, &Rect);
	v2 Result = V2((f32)(Rect.right - Rect.left), (f32)(Rect.bottom - Rect.top));
	return Result;
}

function keyboard_key Win32_Get_Keyboard_Key(int VKCode) {
	switch (VKCode) {
		case VK_CONTROL: return KEYBOARD_KEY_CTRL;
		case VK_SHIFT: return KEYBOARD_KEY_SHIFT;
		case VK_MENU: return KEYBOARD_KEY_ALT;
		case VK_OEM_3: return KEYBOARD_KEY_BACKTICK;
		case VK_LEFT: return KEYBOARD_KEY_LEFT;
		case VK_RIGHT: return KEYBOARD_KEY_RIGHT;
		case VK_UP: return KEYBOARD_KEY_UP;
		case VK_DOWN: return KEYBOARD_KEY_DOWN;
		case VK_DELETE: return KEYBOARD_KEY_DELETE;
	}

	if (VKCode < 256) return (keyboard_key)VKCode;
	return (keyboard_key)-1;
}


function b32 Win32_DLL_Load(win32_dll* DLL) {
	if (!OS_Copy_File(DLL->DLLPath, DLL->TmpDLLPath)) {
		return false;
	}

	DLL->Library = OS_Library_Create(DLL->TmpDLLPath);
	if (!DLL->Library) {
		return false;
	}

	for (size_t i = 0; i < DLL->FunctionCount; i++) {
		DLL->Functions[i] = OS_Library_Get_Function(DLL->Library, DLL->FunctionNames[i].Ptr);
		Assert(DLL->Functions[i]);
	}
	return true;
}

function b32 Win32_DLL_Init(win32_dll* DLL, string DLLPath, string* FunctionNames, size_t FunctionCount) {
	win32_engine* Win32 = Win32_Get();
	
	DLL->HotReload = OS_Hot_Reload_Create(DLLPath);

	string DLLFilePath = String_Get_Directory_Path(DLLPath);
	string FilenameWithoutExt = String_Get_Filename_Without_Ext(DLLPath);

	string ConcatData[] = { DLLFilePath, FilenameWithoutExt, String_Lit("_tmp.dll") };

	DLL->DLLPath = String_Copy((allocator*)Win32->Arena, DLLPath);
	DLL->TmpDLLPath = String_Combine((allocator*)Win32->Arena, ConcatData, Array_Count(ConcatData));
	
	Assert(FunctionCount < MAX_FUNCTION_COUNT);

	DLL->FunctionCount = FunctionCount;
	for (size_t i = 0; i < FunctionCount; i++) {
		DLL->FunctionNames[i] = String_Copy((allocator*)Win32->Arena, FunctionNames[i]);
	}

	DLL->RWLock = OS_RW_Mutex_Create();

	return Win32_DLL_Load(DLL);
}

function b32 Win32_DLL_Check_Reload(win32_dll* DLL) {
	if (!OS_Hot_Reload_Has_Reloaded(DLL->HotReload)) {
		return false;
	}

	OS_RW_Mutex_Write_Lock(DLL->RWLock);
	OS_Library_Delete(DLL->Library);
	for (size_t i = 0; i < 128; i++) {
		if (Win32_DLL_Load(DLL)) {
			return true;
		} else {
			OS_Sleep(1);
		}
	}

	OS_RW_Mutex_Write_Unlock(DLL->RWLock);
	Assert(false);
	return false;
}

function inline void Win32_DLL_Finish_Reload(win32_dll* DLL) {
	OS_RW_Mutex_Write_Unlock(DLL->RWLock);
}

function inline void Win32_DLL_Wait_For_Reload(win32_dll* DLL) {
	OS_RW_Mutex_Read_Lock(DLL->RWLock);
}

function inline void Win32_DLL_Finish_Waiting(win32_dll* DLL) {
	OS_RW_Mutex_Read_Unlock(DLL->RWLock);
}

function LRESULT Win32_Engine_Proc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
	win32_engine* Engine = Win32_Get();
	
	LRESULT Result = 0;
	switch (Message) {
		case WM_CHAR: {
			u32 Codepoint = UTF16_Read((const wchar_t*)&WParam, NULL);
			os_char_event Event = { .Codepoint = Codepoint };
			OS_Add_Event(OS_EVENT_TYPE_CHAR, &Event);
		} break;

		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_KEYDOWN: {
			b32 WasDown = ((LParam & (1 << 30)) != 0);
			b32 IsDown = ((LParam & (1UL << 31)) == 0);

			if (IsDown != WasDown) {
				keyboard_key Key = Win32_Get_Keyboard_Key((int)WParam);
				if (Key != (keyboard_key)-1) {
					os_event_type Type = IsDown ? OS_EVENT_TYPE_KEYBOARD_DOWN : OS_EVENT_TYPE_KEYBOARD_UP;
					os_keyboard_event Event = { .Key = Key };
					OS_Add_Event(Type, &Event);
				}
			}
		} break;

		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP: {
			os_event_type Type = (Message == WM_LBUTTONDOWN || Message == WM_RBUTTONDOWN || Message == WM_MBUTTONDOWN) ? OS_EVENT_TYPE_MOUSE_DOWN : OS_EVENT_TYPE_MOUSE_UP;
			mouse_key Key = (Message == WM_LBUTTONUP || Message == WM_LBUTTONDOWN) ? MOUSE_KEY_LEFT : ((Message == WM_RBUTTONUP || Message == WM_RBUTTONDOWN) ? MOUSE_KEY_RIGHT : MOUSE_KEY_MIDDLE);
			os_mouse_event Event = { .Key = Key };
			OS_Add_Event(Type, &Event);
		} break;

		case WM_MOUSEWHEEL: {
			f32 Delta = (f32)GET_WHEEL_DELTA_WPARAM(WParam) / (f32)WHEEL_DELTA;
			os_mouse_scroll_event Event = { .Delta = Delta };
			OS_Add_Event(OS_EVENT_TYPE_MOUSE_SCROLL, &Event);
		} break;

		case WM_INPUT: {
			arena* Scratch = Scratch_Get();

			UINT DataSize;
			GetRawInputData((HRAWINPUT)LParam, RID_INPUT, NULL, &DataSize, sizeof(RAWINPUTHEADER));

			RAWINPUT* RawInput = (RAWINPUT*)Arena_Push(Scratch, DataSize);
			GetRawInputData((HRAWINPUT)LParam, RID_INPUT, RawInput, &DataSize, sizeof(RAWINPUTHEADER));

			switch (RawInput->header.dwType) {
				case RIM_TYPEMOUSE: {
					RAWMOUSE* Mouse = &RawInput->data.mouse;
					if (Mouse->lLastX != 0 || Mouse->lLastY != 0) {
						v2 Delta = V2((f32)Mouse->lLastX, (f32)-Mouse->lLastY);
						os_mouse_move_event Event = { .Delta = Delta };
						OS_Add_Event(OS_EVENT_TYPE_MOUSE_DELTA, &Event);
					}
				} break;
			}
			Scratch_Release();
		} break;

		case WM_MOUSEMOVE: {
			v2 Resolution = Win32_Get_Client_Dim(Window);

			v2 MousePosition = V2((f32)GET_X_LPARAM(LParam), (f32)GET_Y_LPARAM(LParam));
			os_mouse_move_event Event = { .P = MousePosition };
			OS_Add_Event(OS_EVENT_TYPE_MOUSE_MOVE, &Event);
		} break;

		case WM_DPICHANGED: {
			UINT NewDPI = HIWORD(WParam);
			Engine->UIScale = (f32)NewDPI / 96.0f;
		} break;

		case WM_ACTIVATE: {
			b32 IsFocused = (LOWORD(WParam) != WA_INACTIVE);
			Atomic_Store_B32(&Engine->IsFocused, IsFocused);
		} break;

		case WM_SETFOCUS: {
			Atomic_Store_B32(&Engine->IsFocused, true);
		} break;

		case WM_KILLFOCUS: {
			Atomic_Store_B32(&Engine->IsFocused, false);
		} break;

		case WM_SHOWWINDOW: {
			if (!WParam) { // Window being hidden
				Atomic_Store_B32(&Engine->IsFocused, false);
			}
		} break;

		case WM_CLOSE: {
			Engine_Shutdown(Engine);
			Job_System_Wait_For_Job(Engine->JobSystem, Engine->RootJob);
			DestroyWindow(Window);
		} break;

		case WM_DESTROY: {
			PostQuitMessage(0);
		} break;

		default: {
			Result = DefWindowProcW(Window, Message, WParam, LParam);
		} break;
	}
	return Result;
}

function void Win32_Sound_Buffer_Clear(win32_audio* Audio) {
	VOID* Region1;
	VOID* Region2;
    DWORD Region1Size, Region2Size;
    if(SUCCEEDED(IDirectSoundBuffer_Lock(Audio->SoundBuffer, 0, Audio->BufferSize, 
										 &Region1, &Region1Size, &Region2, &Region2Size, 0)))
    {   
		Memory_Clear(Region1, Region1Size);
		Memory_Clear(Region2, Region2Size);
        IDirectSoundBuffer_Unlock(Audio->SoundBuffer, Region1, Region1Size, Region2, Region2Size);
    }
}

function JOB_CALLBACK_DEFINE(Win32_Sample_Audio_Job) {
	win32_engine* Engine = Win32_Get();
	win32_audio* Audio = &Engine->Audio;

	if (Audio->FirstFrame) {
		Win32_Sound_Buffer_Clear(Audio);
		IDirectSoundBuffer_Play(Audio->SoundBuffer, 0, 0, DSBPLAY_LOOPING);

		IDirectSoundBuffer_GetCurrentPosition(Audio->SoundBuffer, NULL, &Audio->LastWriteCursor);
		Audio->LatencyCount = 1;
		Audio->CurrentPosition = (Audio->LastWriteCursor+(Audio->SamplesPerFrame*sizeof(u16)*SOUND_NUM_CHANNELS)) % Audio->BufferSize;
		Audio->FirstFrame = false;
	}

	WAVEFORMATEX WaveFormat = {};
	IDirectSoundBuffer_GetFormat(Audio->SoundBuffer, &WaveFormat, sizeof(WAVEFORMATEX), NULL);

	DWORD WriteCursor;
	IDirectSoundBuffer_GetCurrentPosition(Audio->SoundBuffer, NULL, &WriteCursor);

	//First determine if the new write cursor is passed the last playing position
	//There are two cases. One where the write cursor is behind the current position,
	//but the last write cursor is in front of the write cursor. This means the write cursor
	//has wrapped passed the buffer.

	bool AdjustLatency = false;
	if(WriteCursor > Audio->CurrentPosition && (Audio->CurrentPosition >= Audio->LastWriteCursor)) {
		AdjustLatency = true;
	}
	else {
		if((Audio->LastWriteCursor > WriteCursor) && (Audio->LastWriteCursor < Audio->CurrentPosition)) {
			AdjustLatency = true;
		}
	}

	//If we need to adjust the latency from the previous logic, adjust it here
	u32 BytesToWrite = 0;
	if(AdjustLatency) {
		//When we adjust the latency make sure the current position is the write cursor
		//and we need to write the latency amount of audio data into the sound buffer 

		if(Audio->LatencyCount < WIN32_MAX_SOUND_LATENCY) {
			Audio->LatencyCount++;
		}
		BytesToWrite = (Audio->SamplesPerFrame*SOUND_NUM_CHANNELS*2)*Audio->LatencyCount;
		Audio->CurrentPosition = (WriteCursor+BytesToWrite) % Audio->BufferSize;

	} else {
		//Otherwise the samples to write is just the difference between the write cursor and last write cursor
		if(Audio->LastWriteCursor > WriteCursor) {
			BytesToWrite = (Audio->BufferSize-Audio->LastWriteCursor)+WriteCursor;
		} else {
			BytesToWrite = WriteCursor-Audio->LastWriteCursor;
		}
	}

	void* RegionBytes[2];
	DWORD RegionByteCount[2];

	bool Locked = SUCCEEDED(IDirectSoundBuffer_Lock(Audio->SoundBuffer, Audio->CurrentPosition, BytesToWrite,
													&RegionBytes[0], &RegionByteCount[0], 
													&RegionBytes[1], &RegionByteCount[1], 0));
		
	DWORD SamplesToWrite = BytesToWrite / (SOUND_NUM_CHANNELS*2);

	arena* Scratch = Scratch_Get();
		
	sound_samples SoundSamples = {};
	SoundSamples.SampleCount = SamplesToWrite;
	SoundSamples.LeftChannel = Arena_Push_Array(Scratch, SamplesToWrite, s16);
	SoundSamples.RightChannel = Arena_Push_Array(Scratch, SamplesToWrite, s16);

	Win32_DLL_Wait_For_Reload(&Engine->EngineDLL);
	Engine_Audio_Stream(Engine, &SoundSamples);
	Win32_DLL_Finish_Waiting(&Engine->EngineDLL);

	if (Locked) {
		s16* RegionSamples[2] = {(s16*)RegionBytes[0], (s16*)RegionBytes[1]};
		u32  RegionSampleCount[2] = {RegionByteCount[0]/(SOUND_NUM_CHANNELS*2), RegionByteCount[1]/(SOUND_NUM_CHANNELS*2)};

		Assert(RegionSampleCount[0]+RegionSampleCount[1] == SamplesToWrite);
			
		s16* LeftChannelAt  = SoundSamples.LeftChannel;
		s16* RightChannelAt = SoundSamples.RightChannel;
		for(u32 RegionIndex = 0; RegionIndex < Array_Count(RegionSamples); RegionIndex++) {
			s16* RegionSample = RegionSamples[RegionIndex];
			for(u32 RegionSampleIndex = 0; RegionSampleIndex < RegionSampleCount[RegionIndex]; RegionSampleIndex++) {    
				*RegionSample++ = *LeftChannelAt++;
				*RegionSample++ = *RightChannelAt++;
			}  
		}

		IDirectSoundBuffer_Unlock(Audio->SoundBuffer, RegionBytes[0], RegionByteCount[0], RegionBytes[1], RegionByteCount[1]);
	}

	Scratch_Release();

	Audio->CurrentPosition = (Audio->CurrentPosition+BytesToWrite) % Audio->BufferSize;
	Audio->LastWriteCursor = WriteCursor;
}

function JOB_CALLBACK_DEFINE(Win32_Audio_Job) {
	win32_engine* Engine = Win32_Get();
	win32_audio* Audio = &Engine->Audio;

	IDirectSound* DirectSound;
	DirectSoundCreate(0, &DirectSound, 0);

	IDirectSound_SetCooperativeLevel(DirectSound, Engine->MainWindow, DSSCL_PRIORITY);

	DSBUFFERDESC PrimaryBufferDesc = {
		.dwSize = sizeof(DSBUFFERDESC),
		.dwFlags = DSBCAPS_PRIMARYBUFFER
	};

	IDirectSoundBuffer* PrimaryBuffer;
	IDirectSound_CreateSoundBuffer(DirectSound, &PrimaryBufferDesc, &PrimaryBuffer, 0);

	WORD BitsPerSample = sizeof(s16) * 8;
	WORD BlockAlign = (SOUND_NUM_CHANNELS*BitsPerSample) / 8;
	WAVEFORMATEX WaveFormat = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels  = SOUND_NUM_CHANNELS,
		.nSamplesPerSec = SOUND_SAMPLES_PER_SEC,
		.nAvgBytesPerSec = (DWORD)(SOUND_SAMPLES_PER_SEC*BlockAlign),
		.nBlockAlign = BlockAlign,
		.wBitsPerSample = BitsPerSample,
	};

	IDirectSoundBuffer_SetFormat(PrimaryBuffer, &WaveFormat);

	DWORD Flags = 0;

#ifdef DEBUG_BUILD
	Flags |= DSBCAPS_GLOBALFOCUS;
#endif

	DSBUFFERDESC SecondaryBufferDesc = {
		.dwSize = sizeof(DSBUFFERDESC),
		.dwFlags = Flags,
		.dwBufferBytes = WaveFormat.nAvgBytesPerSec, //One sec sound buffer
		.lpwfxFormat = &WaveFormat
	};

	IDirectSound_CreateSoundBuffer(DirectSound, &SecondaryBufferDesc, &Audio->SoundBuffer, 0);

	DSBCAPS Caps = { .dwSize = sizeof(DSBCAPS) };
	IDirectSoundBuffer_GetCaps(Audio->SoundBuffer, &Caps);


	Audio->BufferSize = Caps.dwBufferBytes;
	Audio->LatencyCount = 3;
    Audio->CurrentPosition = 0;

    Audio->SamplesPerFrame = Ceil_U32((f64)SOUND_SAMPLES_PER_SEC*WIN32_SOUND_HZ);

	Audio->FirstFrame = true;

	accumulator_loop AudioAccumLoop = {};
	Accumulator_Loop_Start(&AudioAccumLoop, WIN32_SOUND_HZ);

	while (Engine_Is_Running()) {
		Accumulator_Loop_Increment(&AudioAccumLoop);
		if (Accumulator_Loop_Should_Update(&AudioAccumLoop)) {
			while (Accumulator_Loop_Update(&AudioAccumLoop)) {
			
				job_id ParentJob = Job_System_Alloc_Empty_Job(JobSystem, JOB_FLAG_FREE_WHEN_DONE_BIT);
				
				job_data SampleAudioJobData = { Win32_Sample_Audio_Job };
				job_id SampleAudioJob = Job_System_Alloc_Job(JobSystem, SampleAudioJobData, ParentJob, JOB_FLAG_FREE_WHEN_DONE_BIT | JOB_FLAG_QUEUE_IMMEDIATELY_BIT);

				Job_System_Add_Job(JobSystem, ParentJob);
				Job_System_Wait_For_Job(JobSystem, ParentJob);
			}
		} else {
			//Execute other jobs in the meantime
			Job_System_Process_One_Job_And_Yield(JobSystem);
		}

		Thread_Context_Validate();
	}
}

function JOB_CALLBACK_DEFINE(Win32_Engine_Sim_Job) {
	win32_engine* Engine = Win32_Get();
	Engine_Simulate(Engine);
}

function JOB_CALLBACK_DEFINE(Win32_Sim_Job) {
	engine* Engine = Get_Engine();

	accumulator_loop SimAccumLoop = {};
	b32 IsPaused = !Is_Simulating();
	Accumulator_Loop_Start(&SimAccumLoop, SIM_HZ);
	while (Engine_Is_Running()) {
		Accumulator_Loop_Increment(&SimAccumLoop);
		if (Accumulator_Loop_Should_Update(&SimAccumLoop)) {
			while (Accumulator_Loop_Update(&SimAccumLoop)) {
				if (IsPaused) {
					Job_System_Process_One_Job(JobSystem);

					//We also need to consume all the simulation os events
					//since we essentially turned them all off
					OS_Consume_Events(&Engine->SimOSEvents);
					Memory_Clear(&Engine->SimInput, sizeof(input_manager));
					Engine->SimInput.MouseP = V2(FLT_MAX, FLT_MAX);

					if (Is_Simulating()) {
						IsPaused = false;
					}
				} else {
					job_id ParentJob = Job_System_Alloc_Empty_Job(JobSystem, JOB_FLAG_FREE_WHEN_DONE_BIT);
				
					job_data SimJobData = { Win32_Engine_Sim_Job };
					job_id SimAudioJob = Job_System_Alloc_Job(JobSystem, SimJobData, ParentJob, JOB_FLAG_FREE_WHEN_DONE_BIT | JOB_FLAG_QUEUE_IMMEDIATELY_BIT);

					Job_System_Add_Job(JobSystem, ParentJob);
					Job_System_Wait_For_Job(JobSystem, ParentJob);

					if (!Is_Simulating()) {
						//We signal to all threads waiting for simulations to pause
						//using the event 
						IsPaused = true;
						OS_Event_Signal(Engine->SimWaitEvent);
					}
				}
			}
		} else {
			//Execute other jobs in the meantime
			Job_System_Process_One_Job_And_Yield(JobSystem);
		}

		Thread_Context_Validate();
	}
}

function JOB_CALLBACK_DEFINE(Win32_Engine_Update_Job) {
	win32_engine* Engine = Win32_Get();
	Engine_Update(Engine);
}

function JOB_CALLBACK_DEFINE(Win32_Update_Job) {
	win32_engine* Engine = Win32_Get();
	
	u64 StartCounter = OS_Query_Performance_Counter();
	u64 Frequency = OS_Query_Performance_Frequency();
	while (Engine_Is_Running()) {
		u64 LastCounter = OS_Query_Performance_Counter();
		u64 DeltaHz = LastCounter - StartCounter;
		Engine->dt = (f64)DeltaHz / (f64)Frequency;
		StartCounter = LastCounter;

		if (Win32_DLL_Check_Reload(&Engine->EngineDLL)) {
			engine_func* Engine_Reload = (engine_func*)Engine->EngineDLL.Functions[1];
			Engine_Reload(Engine);
			Win32_DLL_Finish_Reload(&Engine->EngineDLL);
		}

		job_id ParentJob = Job_System_Alloc_Empty_Job(JobSystem, JOB_FLAG_FREE_WHEN_DONE_BIT);
		
		job_data EngineUpdateJobData = { Win32_Engine_Update_Job };
		job_id EngineUpdateJob = Job_System_Alloc_Job(JobSystem, EngineUpdateJobData, ParentJob, JOB_FLAG_FREE_WHEN_DONE_BIT | JOB_FLAG_QUEUE_IMMEDIATELY_BIT);

		Job_System_Add_Job(JobSystem, ParentJob);
		Job_System_Wait_For_Job(JobSystem, ParentJob);
		
		Thread_Context_Validate();
	}
}

#ifdef USE_CONSOLE
int WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int ShowCmd) {
#else
int main() {
	HINSTANCE Instance = GetModuleHandleW(NULL);
#endif
	Base_Init();

	win32_engine Win32Engine = {};
	Win32Engine.Base = Base_Get();
	Set_Engine(&Win32Engine);

	Win32Engine.Arena = Arena_Create();
	Win32Engine.UpdateOSEvents.Lock = OS_Mutex_Create();
	Win32Engine.SimOSEvents.Lock = OS_Mutex_Create();

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	RAWINPUTDEVICE RawInputDevices[] = {
		{
			.usUsagePage = WIN32_USAGE_PAGE,
			.usUsage = WIN32_MOUSE_USAGE,
		}
	};

	if (!RegisterRawInputDevices(RawInputDevices, Array_Count(RawInputDevices), sizeof(RAWINPUTDEVICE))) {
		MessageBoxW(NULL, L"Could not register raw input devices", L"Error", MB_OK);
		return 1;
	}


	WNDCLASSEXW WindowClassEx = {
		.cbSize = sizeof(WNDCLASSEXW),
		.style = CS_VREDRAW|CS_HREDRAW|CS_OWNDC,
		.lpfnWndProc = Win32_Engine_Proc,
		.hInstance = Instance,
		.lpszClassName = L"Origin-Engine-Window-Class"
	};

	RegisterClassExW(&WindowClassEx);

	Win32Engine.MainWindow = CreateWindowExW(0, WindowClassEx.lpszClassName, L"Origin Engine", WS_OVERLAPPEDWINDOW, 
									  		 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
									  		 NULL, NULL, Instance, NULL);
	ShowWindow(Win32Engine.MainWindow, SW_MAXIMIZE);

	gdi* GDI = GDI_Init(Base_Get(), Win32Engine.MainWindow, Instance);
	if (!GDI) {
		MessageBoxW(NULL, L"Failed to initialize gdi", L"Error", MB_OK);
		return 1;
	}

	Win32Engine.Renderer.GDI = GDI;
	UINT DPI = GetDpiForWindow(Win32Engine.MainWindow);

	Win32Engine.UIScale = (f32)DPI / 96.0f;

	{
		arena* Scratch = Scratch_Get();
		string DLLPath = String_Concat((allocator*)Scratch, OS_Program_Path(), String_Lit("engine.dll"));
		string FunctionNames[] = { String_Lit("Engine_Initialize"), String_Lit("Engine_Reload") };
		b32 Result = Win32_DLL_Init(&Win32Engine.EngineDLL, DLLPath, FunctionNames, Array_Count(FunctionNames));
		Scratch_Release();
		
		if (!Result) {
			MessageBoxW(NULL, L"Could not find engine.dll", L"Error", MB_OK);
			return 1;
		}	
	}

	engine_func* Engine_Initialize = (engine_func*)Win32Engine.EngineDLL.Functions[0];
	if (!Engine_Initialize(&Win32Engine)) {
		MessageBoxW(NULL, L"Failed to initialize the engine", L"Error", MB_OK);
		return 1;
	}

	Atomic_Store_B32(&Win32Engine.IsFocused, GetForegroundWindow() == Win32Engine.MainWindow);

	Win32Engine.RootJob = Job_System_Alloc_Empty_Job(Win32Engine.JobSystem, JOB_FLAG_FREE_WHEN_DONE_BIT);
	
	job_data AudioJobData = { Win32_Audio_Job };
	Job_System_Alloc_Job(Win32Engine.JobSystem, AudioJobData, Win32Engine.RootJob, JOB_FLAG_FREE_WHEN_DONE_BIT | JOB_FLAG_QUEUE_IMMEDIATELY_BIT);

	job_data UpdateJobData = { Win32_Update_Job };
	Job_System_Alloc_Job(Win32Engine.JobSystem, UpdateJobData, Win32Engine.RootJob, JOB_FLAG_FREE_WHEN_DONE_BIT | JOB_FLAG_QUEUE_IMMEDIATELY_BIT);

	job_data SimJobData = { Win32_Sim_Job };
	Job_System_Alloc_Job(Win32Engine.JobSystem, SimJobData, Win32Engine.RootJob, JOB_FLAG_FREE_WHEN_DONE_BIT | JOB_FLAG_QUEUE_IMMEDIATELY_BIT);

	Job_System_Add_Job(Win32Engine.JobSystem, Win32Engine.RootJob);

	MSG Message;
	while (GetMessageW(&Message, NULL, 0, 0)) {
		TranslateMessage(&Message);
		DispatchMessageW(&Message);

		Thread_Context_Validate();
	}

	return 0;
}

#pragma comment(lib, "base.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dsound.lib")