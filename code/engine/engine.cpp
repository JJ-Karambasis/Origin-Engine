#include <engine.h>

Array_Implement(v3, V3);
Array_Implement(u32, U32);


#include "camera.cpp"
#include "font.cpp"
#include "mesh.cpp"
#include "renderer/renderer.cpp"
#include "audio/audio.cpp"

#include "os/os_events.cpp"
#include "input.cpp"
#include "ui/ui.cpp"

function void Process_OS_Events(os_event_queue* EventQueue) {
	Input_Update();
	
	input_manager* InputManager = Input_Manager_Get();
	os_event_entry Event;
	while (OS_Get_Event(EventQueue, &Event)) {
		switch (Event.Type) {
			case OS_EVENT_TYPE_KEYBOARD_DOWN:
			case OS_EVENT_TYPE_KEYBOARD_UP: {
				State_Set(&InputManager->KeyboardKeys[Event.Keyboard.Key], Event.Type == OS_EVENT_TYPE_KEYBOARD_DOWN);
			} break;

			case OS_EVENT_TYPE_MOUSE_DOWN:
			case OS_EVENT_TYPE_MOUSE_UP: {
				State_Set(&InputManager->MouseKeys[Event.Mouse.Key], Event.Type == OS_EVENT_TYPE_MOUSE_DOWN);
			} break;
			
			case OS_EVENT_TYPE_MOUSE_MOVE: {
				InputManager->MouseP = Event.MouseMove.P;
			} break;

			case OS_EVENT_TYPE_MOUSE_DELTA: {
				InputManager->MouseDelta = V2_Add_V2(InputManager->MouseDelta, Event.MouseMove.Delta);
			} break;
			
			case OS_EVENT_TYPE_MOUSE_SCROLL: {
				InputManager->MouseZoom += Event.MouseScroll.Delta;
			} break;

			case OS_EVENT_TYPE_CHAR: {

			} break;
		}
	}
}

function ENGINE_FUNCTION_DEFINE(Engine_Simulate_Impl) {
	Set_Input_Thread(&Engine->SimInput);
	Input_Update();
	Process_OS_Events(&Engine->SimOSEvents);

	if (Keyboard_Is_Pressed('D')) {
		Play_Sound(String_Lit("Bloop"), 0, 0.5f);
	}
	return true;
}

function ENGINE_FUNCTION_DEFINE(Engine_Update_Impl) {
	os_event_queue* OSEventQueue = &Engine->UpdateOSEvents;
	Set_Input_Thread(&Engine->UpdateInput);
	Input_Update();
	Process_OS_Events(&Engine->UpdateOSEvents);

	input_manager* InputManager = Input_Manager_Get();

	f32 dt = (f32)Engine->dt;
	camera* Camera = &Engine->Camera;
	if (Mouse_Is_Down(MOUSE_KEY_MIDDLE)) {
		v2 MouseDelta = InputManager->MouseDelta;
		if (Keyboard_Is_Down(KEYBOARD_KEY_CTRL)) {
			f32 Zoom = MouseDelta.y;
			Camera->Distance = Max(Camera->Distance-(Zoom * dt * 10), 1e-4f);
		} else if (Keyboard_Is_Down(KEYBOARD_KEY_SHIFT)) {
			m3 Orientation = Camera_Get_Orientation(Camera);
			Camera->Target = V3_Add_V3(Camera->Target, V3_Mul_S(Orientation.x, MouseDelta.x * dt * 1.5f));
			Camera->Target = V3_Add_V3(Camera->Target, V3_Mul_S(Orientation.y, MouseDelta.y * dt * 1.5f));
		} else {
			Camera->Pitch += MouseDelta.y * dt;
			Camera->Roll += MouseDelta.x * dt;
		}
	}

	if (InputManager->MouseZoom != 0) {
		Camera->Distance = Max(Camera->Distance-(InputManager->MouseZoom * dt * 100), 1e-4f);
	}

	if (Keyboard_Is_Pressed('A')) {
		Play_Sound(String_Lit("Bloop"), 0, 0.5f);
	}

	UI_Begin(V2_From_V2i(GDI_Get_View_Dim()));

	ui_font DefaultFont = { Engine->Font, 30 * UI_Scale() };
	UI_Push_Font(DefaultFont);

	UI_Text_Formatted("FPS: %d###Time", (int)(1.0 / dt));

	ui_box* TxtBox = UI_Get_Last_Box();
	v2 P = V2(GDI_Get_View_Dim().x - TxtBox->FixedDim.x, 0.0f);
	UI_Set_Position_X(TxtBox, P.x);
	UI_Set_Position_Y(TxtBox, P.y);

	UI_Pop_Font();

	UI_End();

	Render_Frame(&Engine->Renderer, Camera);
	return true;
}

function ENGINE_FUNCTION_DEFINE(Engine_Shutdown_Impl) {
	Engine_Stop_Running();
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
	Renderer_Init(&Engine->Renderer);
	Audio_Init(&Engine->Audio);

	Create_GFX_Component(M4_Affine_Transform_Quat_No_Scale(V3(0.5f, -0.5f, 0.0f), Quat_Identity()), V4(0.0f, 1.0f, 0.0f, 1.0f), String_Lit("Box"));
	Create_GFX_Component(M4_Affine_Transform_Quat_No_Scale(V3(0.0f, 0.0f, -2.5f), Quat_Identity()), V4(0.0f, 0.0f, 1.0f, 1.0f), String_Lit("Monkey"));

	Camera_Init(&Engine->Camera, V3_Zero());

	editable_mesh* DebugBoxLineMesh = Create_Box_Line_Editable_Mesh();
	editable_mesh* DebugSphereLineMesh = Create_Sphere_Line_Editable_Mesh(60);
	editable_mesh* DebugSphereTriangleMesh = Create_Sphere_Mesh(3);

	Engine->DebugBoxLineMesh        = Create_GFX_Mesh(DebugBoxLineMesh, String_Lit("Debug_Box_Line"));
	Engine->DebugSphereLineMesh     = Create_GFX_Mesh(DebugSphereLineMesh, String_Lit("Debug_Sphere_Line"));
	Engine->DebugSphereTriangleMesh = Create_GFX_Mesh(DebugSphereTriangleMesh, String_Lit("Debug_Sphere_Triangle"));

	Delete_Editable_Mesh(DebugBoxLineMesh);
	Delete_Editable_Mesh(DebugSphereLineMesh);
	Delete_Editable_Mesh(DebugSphereTriangleMesh);

	Engine->UpdateInput.MouseP = V2(FLT_MAX, FLT_MAX);
	Engine->SimInput.MouseP = V2(FLT_MAX, FLT_MAX);


	arena* Scratch = Scratch_Get();
	buffer FontBuffer = Read_Entire_File((allocator*)Scratch, String_Lit("fonts/LiberationMono.ttf"));
	if (Buffer_Is_Empty(FontBuffer)) return false;

	Engine->Font = Font_Create(FontBuffer);

	Engine->ThreadLocalStorage = OS_TLS_Create();

	Engine->JobSystem = Job_System_Create(1024, OS_Processor_Thread_Count(), 0);

	Play_Sound(String_Lit("TestMusic"), SOUND_FLAG_LOOPING, 0.2f);

	Engine_Start_Running();
	Scratch_Release();

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