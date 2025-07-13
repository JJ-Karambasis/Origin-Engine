#include <engine.h>

#include "renderer/renderer.cpp"
#include "audio/audio.cpp"

function ENGINE_FUNCTION_DEFINE(Engine_Simulate_Impl) {
	Set_Engine(Engine);
	return true;
}

function ENGINE_FUNCTION_DEFINE(Engine_Update_Impl) {
	Set_Engine(Engine);
	Render_Frame(&Engine->Renderer);
	return true;
}

function ENGINE_FUNCTION_DEFINE(Engine_Shutdown_Impl) {
	Set_Engine(Engine);
	return true;
}

global engine_vtable Engine_VTable = {
	.SimulateFunc = Engine_Simulate_Impl,
	.UpdateFunc = Engine_Update_Impl,
	.AudioStreamFunc = Engine_Audio_Stream_Impl,
	.ShutdownFunc = Engine_Shutdown_Impl
};

export_function ENGINE_FUNCTION_DEFINE(Engine_Initialize) {
	Set_Engine(Engine);
	Engine->VTable = &Engine_VTable;
	Engine->JobSystem = Job_System_Create(1024, OS_Processor_Thread_Count(), 0);
	Renderer_Init(&Engine->Renderer);

	Engine_Start_Running();
	return true;
}

export_function ENGINE_FUNCTION_DEFINE(Engine_Reload) {
	Set_Engine(Engine);
	Engine->VTable = &Engine_VTable;
	return true;
}

#ifdef OS_WIN32
#pragma comment(lib, "base.lib")
#endif