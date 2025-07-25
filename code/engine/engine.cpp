#include <engine.h>

Array_Implement(v3, V3);
Array_Implement(u32, U32);


#include "camera.cpp"
#include "font.cpp"
#include "mesh.cpp"
#include "world/world.cpp"
#include "renderer/renderer.cpp"
#include "audio/audio.cpp"

#include "os/os_events.cpp"
#include "input.cpp"
#include "ui/ui.cpp"

function void Pause_Simulation() {
	engine* Engine = Get_Engine();
	OS_Event_Reset(Engine->SimWaitEvent);
	Atomic_Store_B32(&Engine->IsSimulating, false);
	OS_Event_Wait(Engine->SimWaitEvent);
}

function void Resume_Simulation() {
	engine* Engine = Get_Engine();
	Atomic_Store_B32(&Engine->IsSimulating, true);
}

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
	Sim_Begin_Frame();

	Set_Input_Thread(&Engine->SimInput);
	Input_Update();
	Process_OS_Events(&Engine->SimOSEvents);

	Sim_Process_Messages();

	world* World = World_Get();

	f32 dt = (f32)SIM_HZ;

	sim_entity_storage* SimEntities = &World->SimEntities;
	for (u32 i = 0; i < SimEntities->Capacity; i++) {
		sim_entity* Entity = SimEntities->Entities + i;
		if (Entity->Type == ENTITY_TYPE_PLAYER) {
			v2 InputDirection = V2_Zero();

			if (Keyboard_Is_Down('A')) {
				InputDirection.x = -1.0f;
			}
			
			if (Keyboard_Is_Down('D')) {
				InputDirection.x = 1.0f;
			}
			
			if (Keyboard_Is_Down('W')) {
				InputDirection.y = 1.0f;
			}

			if (Keyboard_Is_Down('S')) {
				InputDirection.y = -1.0f;
			}

			v2 Velocity = V2_Mul_S(V2_Norm(InputDirection), 1.5f * dt);
			Entity->Position.xy = V2_Add_V2(Entity->Position.xy, Velocity);

			Sim_Push_Entity(Entity);
		}
	}

	Sim_End_Frame();

	return true;
}

function void Sync_Simulation() {
	engine* Engine = Get_Engine();

	//Get the old frame and the new frame while locking to prevent the simulation
	//from starting or finishing a frame while this occurs
	OS_Mutex_Lock(Engine->SimFrameLock);
	push_frame* OldFrame = NULL;
	push_frame* NewFrame = NULL;
	for (;;) {
		b32 FreeFrame = false;

		OldFrame = Engine->FirstFrame;
		if (!OldFrame) break;

		if (Time_Is_Newer(Engine->UpdateSimTime, OldFrame->Time)) {
			push_frame* NextFrame = OldFrame->Next;
			if (!NextFrame) {
				break;
			}

			if (Time_Is_Newer(NextFrame->Time, Engine->UpdateSimTime)) {
				NewFrame = NextFrame;
				break;
			}

			SLL_Pop_Front(Engine->FirstFrame);
			if (!Engine->FirstFrame) Engine->LastFrame = NULL;
			OldFrame->Next = NULL;
			SLL_Push_Front(Engine->FreeFrames, OldFrame);
		} Invalid_Else;
	}
	OS_Mutex_Unlock(Engine->SimFrameLock);

	//Set the new frame to let the update frame know if its still simulating or not

	if (OldFrame) {

		//If there is at least an old frame, update the entities with the old
		//simulation frame to at least keep the rendering in sync and to assist
		//in interpolation if there is a new frame

		if (Is_Simulating() || NewFrame) {
			for (size_t i = 0; i < OldFrame->CmdCount; i++) {
				push_cmd* Cmd = OldFrame->Cmds + i;
				entity* Entity = Get_Entity(Cmd->ID);
				if (Entity) {
					//Store old transforms in current transforms
					Entity->Position = Cmd->Position;
					Entity->Orientation = Cmd->Orientation;
				}
			}
		} else {
			for (pool_iter Iter = Pool_Begin_Iter(&Engine->World->Entities); Iter.IsValid; Pool_Iter_Next(&Iter)) {
				entity* Entity = (entity*)Iter.Data;
				if (Entity->ID.Index < Engine->World->SimEntities.Capacity) {
					sim_entity* SimEntity = Engine->World->SimEntities.Entities + Entity->ID.Index;
					if(SimEntity->IsAllocated) {
						Entity->Position = SimEntity->Position;
						Entity->Orientation = SimEntity->Orientation;
					}
				}
			}
		}

		if (NewFrame) {
			//If there is a new frame, interpolate between the two frames to get
			//the entities actual position in the frame

			Assert(Engine->UpdateSimTime.Interval == OldFrame->Time.Interval &&
				(OldFrame->Time.Interval == NewFrame->Time.Interval ||
				OldFrame->Time.Interval == NewFrame->Time.Interval - 1));
		
			f64 tAt = Engine->UpdateSimTime.dt - OldFrame->Time.dt;
			f64 tInterp = tAt / SIM_HZ;

			Assert(tInterp >= 0 && tInterp <= 1);

			for (size_t i = 0; i < NewFrame->CmdCount; i++) {
				push_cmd* Cmd = NewFrame->Cmds + i;
				entity* Entity = Get_Entity(Cmd->ID);
				if (Entity) {
					Entity->Position = V3_Lerp(Entity->Position, (f32)tInterp, Cmd->Position);
					Entity->Orientation = Quat_Lerp(Entity->Orientation, (f32)tInterp, Cmd->Orientation);
				}
			}
		}

		if (NewFrame) {
			Time_Increment(&Engine->UpdateSimTime, Engine->dt);
		}
	}
}

function ENGINE_FUNCTION_DEFINE(Engine_Update_Impl) {
	Set_Input_Thread(&Engine->UpdateInput);
	Input_Update();
	Process_OS_Events(&Engine->UpdateOSEvents);


	Sync_Simulation();

	f32 dt = (f32)Engine->dt;
	camera* Camera = &Engine->Camera;
	Camera_Move_Arcball(Camera, dt);

	UI_Begin(V2_From_V2i(GDI_Get_View_Dim()));

	ui_font DefaultFont = { Engine->Font, 30 * UI_Scale() };
	UI_Push_Font(DefaultFont);

	UI_Set_Next_Fixed_Size(V2(500, 300));
	UI_Set_Next_Layout_Axis(UI_AXIS_Y);
	ui_box* Box = UI_Make_Box_From_String(0, String_Lit("Info Box"));
	v2 P = V2(GDI_Get_View_Dim().x - Box->FixedDim.x, 0.0f);
	UI_Set_Position(Box, P);

	UI_Push_Parent(Box);

	UI_Text_Formatted("FPS: %d", (int)(1.0 / dt));
	UI_Text_Formatted("Is Simulating: %s", Is_Simulating() ? "true" : "false");

	UI_Pop_Parent();

	UI_Pop_Font();

	UI_End();

	world* World = World_Get();
	for (pool_iter Iter = Pool_Begin_Iter(&World->Entities); Iter.IsValid; Pool_Iter_Next(&Iter)) {
		entity* Entity = (entity*)Iter.Data;
		gfx_component* Component = Get_GFX_Component(Entity->GfxComponent);
		Component->Transform = M4_Affine_Transform_Quat(Entity->Position, Entity->Orientation, Entity->Scale);
	}

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

	Engine->SimArena = Arena_Create();
	Engine->SimWaitEvent = OS_Event_Create();
	Engine->SimFrameLock = OS_Mutex_Create();

	Engine->World = World_Create(String_Lit("Default World"));

	Create_Entity({
		.Name = String_Lit("Entity A"),
		.Type = ENTITY_TYPE_PLAYER,
		.Position = V3(0.5f, -0.5f, 0.0f),
		.Color = V4(0.0f, 1.0f, 0.0f, 1.0f),
		.MeshName = String_Lit("Box")
	});

	Create_Entity({
		.Name = String_Lit("Entity B"),
		.Position = V3(0.0f, 0.0f, -2.5f),
		.Color = V4(0.0f, 0.0f, 1.0f, 1.0f),
		.MeshName = String_Lit("Box")
	});

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