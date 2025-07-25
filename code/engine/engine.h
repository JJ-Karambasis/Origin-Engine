#ifndef ENGINE_H
#define ENGINE_H

#include <base.h>
#include <meta_program/meta_defines.h>

#include "mesh.h"
#include "audio/audio.h"
#include "renderer/renderer.h"
#include "world/world.h"

#include "accumulator_loop.h"
#include "camera.h"
#include "font.h"
#include "os/os_events.h"
#include "ui/ui.h"

#define SIM_HZ (1.0/60.0)

struct engine;

struct state {
	b16 WasOn;
	b16 IsOn;
};

#define DT_PER_INTERVAL 20.0
struct high_res_time {
	f64 dt;
	u64 Interval;
};

function inline b32 Time_Is_Newer(high_res_time A, high_res_time B) {
	if (A.Interval == B.Interval) {
		return A.dt >= B.dt;
	}

	return A.Interval > B.Interval;
}

function inline void Time_Increment(high_res_time* Time, f64 dt) {
	Time->dt += dt;
	while (Time->dt >= DT_PER_INTERVAL) {
		Time->dt -= DT_PER_INTERVAL;
		Time->Interval++;
	}
}

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

struct engine_thread_context {
	ui* 		   UI;
	input_manager* InputManager;
};

struct push_cmd {
	entity_id ID;
	v3 		  Position;
	quat 	  Orientation;
};

#define MAX_PUSH_CMD_COUNT 1024
struct push_frame {
	push_cmd 	  Cmds[MAX_PUSH_CMD_COUNT];
	u32 	 	  CmdCount;
	high_res_time Time;

	push_frame* Next;
};

struct engine {
	engine_vtable* VTable;
	base* 		   Base;
	job_system*    JobSystem;
	renderer       Renderer;
	audio_manager  Audio;
	input_manager  UpdateInput;
	input_manager  SimInput;
	f32 		   UIScale;
	atomic_b32 	   IsFocused;
	font* 		   Font;
	f64 		   dt;
	os_tls* 	   ThreadLocalStorage;
	world* 		   World;

	os_event_queue UpdateOSEvents;
	os_event_queue SimOSEvents;

	arena* 	   SimArena;
	atomic_b32 IsSimulating;
	os_event*  SimWaitEvent;
	os_mutex*  SimFrameLock;
	push_frame* FirstFrame;
	push_frame* CurrentFrame;
	push_frame* LastFrame;
	push_frame* FreeFrames;
	high_res_time SimTime;
	high_res_time UpdateSimTime;

	camera Camera;

	gfx_mesh* DebugBoxLineMesh;
	gfx_mesh* DebugSphereLineMesh;
	gfx_mesh* DebugCapsuleLineMesh;
	gfx_mesh* DebugSphereTriangleMesh;

	atomic_b32 IsRunning;
};

#define Engine_Simulate(engine) (engine)->VTable->SimulateFunc(engine)
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

function inline engine_thread_context* Get_Engine_Thread_Context() {
	engine* Engine = Get_Engine();
	engine_thread_context* ThreadContext = (engine_thread_context*)OS_TLS_Get(Engine->ThreadLocalStorage);
	if (!ThreadContext) {
		ThreadContext = Allocator_Allocate_Struct(Default_Allocator_Get(), engine_thread_context);
		OS_TLS_Set(Engine->ThreadLocalStorage, ThreadContext);
	}
	return ThreadContext;
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
#define Is_Simulating() Atomic_Load_B32(&Get_Engine()->IsSimulating)

#endif