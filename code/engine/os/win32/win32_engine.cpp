#include "win32_engine.h"

#include <accumulator_loop.cpp>

function inline win32_engine* Win32_Get() {
	win32_engine* Result = (win32_engine*)Get_Engine();
	Assert(Result);
	return Result;
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
	LRESULT Result = 0;
	switch (Message) {
		case WM_CLOSE: {
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

function JOB_CALLBACK_DEFINE(Win32_Audio_Sample_Audio_Job) {
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
	//Flags |= DSBCAPS_GLOBALFOCUS;
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
				
				job_data SampleAudioJobData = { Win32_Audio_Sample_Audio_Job };
				job_id SampleAudioJob = Job_System_Alloc_Job(JobSystem, SampleAudioJobData, ParentJob, JOB_FLAG_FREE_WHEN_DONE_BIT | JOB_FLAG_QUEUE_IMMEDIATELY_BIT);

				Job_System_Add_Job(JobSystem, ParentJob);
				Job_System_Wait_For_Job(JobSystem, ParentJob);
			}
		} else {
			//Execute other jobs in the meantime
			//Job_System_Process_One_Job_And_Yield(JobSystem);
		}
	}
}

int WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int ShowCmd) {
	Base_Init();

	win32_engine Win32Engine = {};
	Win32Engine.Base = Base_Get();
	Set_Engine(&Win32Engine);

	Win32Engine.Arena = Arena_Create();

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

	job_id ParentJob = Job_System_Alloc_Empty_Job(Win32Engine.JobSystem, JOB_FLAG_FREE_WHEN_DONE_BIT);
	
	job_data AudioJobData = { Win32_Audio_Job };
	job_id AudioJob = Job_System_Alloc_Job(Win32Engine.JobSystem, AudioJobData, ParentJob, JOB_FLAG_FREE_WHEN_DONE_BIT | JOB_FLAG_QUEUE_IMMEDIATELY_BIT);

	Job_System_Add_Job(Win32Engine.JobSystem, ParentJob);

	for (;;) {
		if (Win32_DLL_Check_Reload(&Win32Engine.EngineDLL)) {
			engine_func* Engine_Reload = (engine_func*)Win32Engine.EngineDLL.Functions[1];
			Engine_Reload(&Win32Engine);
			Win32_DLL_Finish_Reload(&Win32Engine.EngineDLL);
		}

		MSG Message;
		while (PeekMessageW(&Message, NULL, 0, 0, PM_REMOVE)) {
			switch (Message.message) {
				case WM_QUIT: {
					Engine_Stop_Running();
					Job_System_Wait_For_Job(Win32Engine.JobSystem, ParentJob);
					Engine_Shutdown(&Win32Engine);
					return 0;
				} break;

				default: {
					TranslateMessage(&Message);
					DispatchMessageW(&Message);
				} break;
			}
		}

		Engine_Update(&Win32Engine);
	}
}

#pragma comment(lib, "base.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dsound.lib")