#ifndef ENGINE_H
#define ENGINE_H

#include <base.h>
#include <meta_program/meta_defines.h>

#include "audio/audio.h"
#include "renderer/renderer.h"

#include "accumulator_loop.h"
#include "camera.h"
#include "font.h"
#include "os/os_events.h"
#include "ui/ui.h"

struct engine;

struct state {
	b16 WasOn;
	b16 IsOn;
};

inline b32 State_Turned_On(state* State) { return State->IsOn && !State->WasOn; }
inline b32 State_Turned_Off(state* State) { return !State->IsOn && State->WasOn; }
inline b32 State_Is_On(state* State) { return State->IsOn; }
inline b32 State_Is_Off(state* State) { return !State->IsOn; }
inline void State_Update(state* State) { State->WasOn = State->IsOn; }
inline void State_Set(state* State, b16 CurrentState) { State->IsOn = CurrentState; }

#include "input.h"

#define ENGINE_FUNCTION_DEFINE(name) b32 name(engine* Engine)
typedef ENGINE_FUNCTION_DEFINE(engine_func);

#define ENGINE_AUDIO_STREAM_DEFINE(name) void name(engine* Engine, sound_samples* OutSamples)
typedef ENGINE_AUDIO_STREAM_DEFINE(engine_audio_stream_func);

struct engine_vtable {
	engine_func* 			  SimulateFunc;
	engine_func* 			  UpdateFunc;
	engine_audio_stream_func* AudioStreamFunc;
	engine_func* 			  ShutdownFunc;
};

struct engine {
	engine_vtable* VTable;
	base* 		   Base;
	job_system*    JobSystem;
	renderer       Renderer;
	audio_manager  Audio;
	input_manager  Input;
	f32 		   UIScale;
	atomic_b32 	   IsFocused;
	font* 		   Font;
	f64 		   dt;
	os_tls* 	   UIThreadLocalStorage;

	os_event_queue AppOSEvents;

	camera Camera;

	atomic_b32 IsRunning;
};

#define Engine_Update(engine) (engine)->VTable->UpdateFunc(engine)
#define Engine_Shutdown(engine) (engine)->VTable->ShutdownFunc(engine)
#define Engine_Audio_Stream(engine, out_samples) (engine)->VTable->AudioStreamFunc(engine, out_samples)

global engine* G_Engine;
function inline engine* Get_Engine() {
	return G_Engine;
}

function inline void Set_Engine(engine* Engine) {
	G_Engine = Engine;
	if (G_Engine) {
		Base_Set(Engine->Base);
		GDI_Set(Engine->Renderer.GDI);
	}
}

function inline void Engine_Start_Running() {
	engine* Engine = Get_Engine();
	Atomic_Store_B32(&Engine->IsRunning, true);
}

function inline void Engine_Stop_Running() {
	engine* Engine = Get_Engine();
	Atomic_Store_B32(&Engine->IsRunning, false);
}

function inline b32 Engine_Is_Running() {
	engine* Engine = Get_Engine();
	return Atomic_Load_B32(&Engine->IsRunning);
}

#define UI_Scale() Get_Engine()->UIScale
#define Is_Focused() Atomic_Load_B32(&Get_Engine()->IsFocused)


#endif