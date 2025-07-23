#include <engine.h>

#include "camera.cpp"
#include "font.cpp"
#include "renderer/renderer.cpp"
#include "audio/audio.cpp"

#include "os/os_events.cpp"
#include "input.cpp"
#include "ui/ui.cpp"

function ENGINE_FUNCTION_DEFINE(Engine_Simulate_Impl) {
	return true;
}

function ENGINE_FUNCTION_DEFINE(Engine_Update_Impl) {
	os_event_queue* OSEventQueue = &Engine->AppOSEvents;
	
	Input_Update();

	input_manager* InputManager = Input_Manager_Get();

	os_event_entry Event;
	while (OS_Get_Event(OSEventQueue, &Event)) {
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

	UI_Set_Next_Fixed_Width(1000);
	UI_Set_Next_Fixed_Height(1000);
	UI_Set_Next_Background_Color(V4(1.0f, 1.0f, 1.0f, 1.0f));
	UI_Set_Next_Layout_Axis(UI_AXIS_Y);
	UI_Push_Parent(UI_Make_Box_From_String(0, String_Lit("Box")));

	UI_Set_Next_Fixed_Width(500);
	UI_Set_Next_Fixed_Height(500);
	UI_Set_Next_Background_Color(V4(1.0f, 0.0f, 1.0f, 1.0f));
	UI_Make_Box_From_String(0, String_Lit("Box"));

	UI_Set_Next_Fixed_Width(500);
	UI_Set_Next_Fixed_Height(100);
	UI_Set_Next_Background_Color(V4(0.0f, 1.0f, 1.0f, 1.0f));
	UI_Make_Box_From_String(0, String_Lit("Box1"));

	UI_Set_Next_Pref_Width(UI_Txt());
	UI_Set_Next_Pref_Height(UI_Txt());
	UI_Set_Next_Background_Color(V4(0.0f, 0.0f, 0.0f, 1.0f));
	UI_Set_Next_Text_Color(V4(1.0f, 1.0f, 1.0f, 1.0f));
	UI_Make_Box_From_String(UI_BOX_FLAG_DRAW_TEXT, String_Lit("Hello World###Box2"));

	UI_Pop_Parent();

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

	Camera_Init(&Engine->Camera, V3_Zero());

	Engine->Input.MouseP = V2(FLT_MAX, FLT_MAX);

	arena* Scratch = Scratch_Get();
	buffer FontBuffer = Read_Entire_File((allocator*)Scratch, String_Lit("fonts/LiberationMono.ttf"));
	if (Buffer_Is_Empty(FontBuffer)) return false;

	Engine->Font = Font_Create(FontBuffer);

	Engine->UIThreadLocalStorage = OS_TLS_Create();

	Engine->JobSystem = Job_System_Create(1024, OS_Processor_Thread_Count(), 0);

	Play_Sound(String_Lit("TestMusic"), SOUND_FLAG_LOOPING, 0.2f);

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