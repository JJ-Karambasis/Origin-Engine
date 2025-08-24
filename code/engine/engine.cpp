#include <engine.h>

Array_Implement(v3, V3);
Array_Implement(u32, U32);


#include "camera.cpp"
#include "font.cpp"
#include "mesh.cpp"
#include "world/world.cpp"
#include "simulation/simulation.cpp"
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
	Sim_Begin_Frame();

	Set_Input_Thread(&Engine->SimInput);
	Input_Update();
	Process_OS_Events(&Engine->SimOSEvents);

	Sim_Process_Messages();

	simulation* Simulation = Simulation_Get();

	f32 dt = (f32)SIM_HZ;

	sim_entity_storage* SimEntities = &Simulation->Entities;

	v3 PlayerP = V3_Zero();
	for (u32 i = 0; i < SimEntities->Capacity; i++) {
		sim_entity* Entity = SimEntities->Entities + i;
		if (Entity->Type == ENTITY_TYPE_PLAYER) {
			PlayerP = Entity->Position;

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

			Entity->Force = V3_Add_V3(Entity->Force, V3_From_V2(V2_Mul_S(V2_Norm(InputDirection), 600.0f)));
		}
	}

	Simulation_Update(Simulation, SIM_HZ);

	for (u32 i = 0; i < SimEntities->Capacity; i++) {
		sim_entity* Entity = SimEntities->Entities + i;
		if (Entity && Entity->BodyType == SIM_BODY_DYNAMIC) {
			Sim_Push_Entity(Entity);
		}
	}

#ifdef FLOW_DEBUG_RENDERING
	flow_debug_renderer* DebugRenderer = Flow_Get_Renderer(Simulation->FlowSystem);
	
	sim_push_frame* PushFrame = Simulation->CurrentFrame;
	PushFrame->FlowDebugCmdCount = DebugRenderer->CmdCount;
	PushFrame->FlowDebugCmds = Arena_Push_Array(PushFrame->Arena, DebugRenderer->CmdCount, flow_debug_cmd);
	Memory_Copy(PushFrame->FlowDebugCmds, DebugRenderer->Cmds, DebugRenderer->CmdCount * sizeof(flow_debug_cmd));
#endif

	Sim_End_Frame();

	return true;
}

function ENGINE_FUNCTION_DEFINE(Engine_Update_Impl) {
	Set_Input_Thread(&Engine->UpdateInput);
	Input_Update();
	Process_OS_Events(&Engine->UpdateOSEvents);

	Sync_Simulation();

	local u64 i = 0;
	if (Mouse_Is_Pressed(MOUSE_KEY_LEFT)) {
		Debug_Log("Down %d", i);
		i++;
	}

	f32 dt = (f32)Engine->dt;
	camera* Camera = &Engine->Camera;
	Camera_Move_Arcball(Camera, dt);

	gdi_swapchain_info ViewInfo = Get_View_Info();

	UI_Begin(V2_From_V2i(ViewInfo.Dim));

	ui_font DefaultFont = { Engine->Font, 30 * UI_Scale() };
	UI_Push_Font(DefaultFont);

	UI_Set_Next_Fixed_Size(V2(500, 300));
	UI_Set_Next_Layout_Axis(UI_AXIS_Y);
	ui_box* Box = UI_Make_Box_From_String(0, String_Lit("Info Box"));
	v2 P = V2(ViewInfo.Dim.x - Box->FixedDim.x, 0.0f);
	UI_Set_Position(Box, P);

	UI_Push_Parent(Box);
	UI_Set_Next_Text_Color(V4(1.0f, 1.0f, 0.0f, 1.0f));

	UI_Text_Formatted(UI_BOX_FLAG_RIGHT_ALIGN, "FPS: %d", (int)(1.0 / dt));
	UI_Text_Formatted(UI_BOX_FLAG_RIGHT_ALIGN, "Is Simulating: %s", Is_Update_Simulating() ? "true" : "false");

	UI_Text_Formatted(UI_BOX_FLAG_RIGHT_ALIGN, "Draw Colliders: %s###Button0", Engine->DrawColliders ? "true" : "false");
	
	if (UI_LMB_Pressed(UI_Get_Last_Box())) {
		Engine->DrawColliders = !Engine->DrawColliders;
	}

	UI_Pop_Parent();
	UI_Pop_Font();

	UI_Texture(1024, 1024, Engine->Renderer.ShadowMap);

	UI_End();

	world* World = World_Get();
	for (pool_iter Iter = Pool_Begin_Iter(&World->Entities); Iter.IsValid; Pool_Iter_Next(&Iter)) {
		entity* Entity = (entity*)Iter.Data;
		if (Entity->Type == ENTITY_TYPE_PLAYER) {
			Camera->Target = Entity->Position;
		}
		gfx_component* Component = Get_GFX_Component(Entity->GfxComponent);
		Component->Transform = M4_Affine_Transform_Quat(Entity->Position, Entity->Orientation, Entity->Scale);
	}

	if (Engine->DrawColliders) {
		v3 Color = V3(0.0f, 0.0f, 1.0f);
		for (pool_iter Iter = Pool_Begin_Iter(&World->Entities); Iter.IsValid; Pool_Iter_Next(&Iter)) {
			entity* Entity = (entity*)Iter.Data;
			switch (Entity->Collider.Type) {
				case SIM_COLLIDER_TYPE_SPHERE: {
					f32 Radius = V3_Largest(Entity->Scale)*Entity->Collider.Sphere.Radius;
					Draw_Sphere(DRAW_FLAG_LINE, Entity->Position, Radius, Color);
				} break;

				case SIM_COLLIDER_TYPE_BOX: {
					v3 HalfExtent = V3_Mul_V3(Entity->Scale, Entity->Collider.Box.HalfExtent);
					Draw_Box(DRAW_FLAG_LINE, Entity->Position, HalfExtent, Entity->Orientation, Color);
				} break;
			}
		}
	}

#ifdef FLOW_DEBUG_RENDERING
	if (Engine->DrawFlowCmds) {
		if (Engine->OldFrame) {
			size_t CmdCount = Engine->OldFrame->FlowDebugCmdCount;
			flow_debug_cmd* FlowDebugCmds = Engine->OldFrame->FlowDebugCmds;
		
			for (size_t i = 0; i < CmdCount; i++) {
				flow_debug_cmd* Cmd = FlowDebugCmds + i;
				Draw_Sphere(0, Cmd->Penetration.P1, 0.02f, V3(1.0f, 0.0f, 0.0f));
				Draw_Sphere(0, Cmd->Penetration.P2, 0.02f, V3(0.0f, 0.0f, 1.0f));

				Draw_Arrow(Cmd->Penetration.P2, Cmd->Penetration.V, 0.02f, V3(0.0f, 1.0f, 1.0f));
			}
		}
	}
#endif

	dir_light* DirLight = &Engine->Renderer.DirLight;
	DirLight->Orientation = Quat_RotX(To_Radians(-45.0f));
	DirLight->Intensity = 1.0f;
	DirLight->Color = V3(1.0f, 1.0f, 1.0f);
	DirLight->IsOn = true;

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
	Engine->JobSystem = Job_System_Create(1024, OS_Processor_Thread_Count(), 0);


	Renderer_Init(&Engine->Renderer);
	Audio_Init(&Engine->Audio);
	Sim_Init(&Engine->Simulation);
	Engine->World = World_Create(String_Lit("Default World"));

	Create_Entity({
		.Name = String_Lit("Entity A"),
		.Type = ENTITY_TYPE_PLAYER,
		.Position = V3(0.0f, 0.0f, 0.0f),
		.MeshName = String_Lit("Box"),
		.Material = {
			.Diffuse = {
				.IsTexture = true,
				.TextureName = String_Lit("Brick_Color")
			}
		},
		.BodyType = SIM_BODY_DYNAMIC,
		.Friction = 0.7f,
		.Collider = {
			.Type = SIM_COLLIDER_TYPE_BOX,
			.Box = { V3_All(1.0f), 5.0f }
		},
	});

	Create_Entity({
		.Name = String_Lit("Entity C"),
		.Position = V3(0.0f, 3.0f, 0.0f),
		.MeshName = String_Lit("Box"),
		.Material = {
			.Diffuse = {
				.IsTexture = true,
				.TextureName = String_Lit("Brick_Color")
			}
		},
		.BodyType = SIM_BODY_DYNAMIC,
		.Friction = 0.5f,
		.Collider = {
			.Type = SIM_COLLIDER_TYPE_BOX,
			.Box = { V3_All(1.0f), 1.0f }
		},
	});

	Create_Entity({
		.Name = String_Lit("Entity D"),
		.Position = V3(0.0f, 3.0f, 2.0f),
		.MeshName = String_Lit("Box"),
		.Material = {
			.Diffuse = {
				.IsTexture = true,
				.TextureName = String_Lit("Brick_Color")
			}
		},
		.BodyType = SIM_BODY_DYNAMIC,
		.Friction = 0.5f,
		.Collider = {
			.Type = SIM_COLLIDER_TYPE_BOX,
			.Box = { V3_All(1.0f), 1.0f }
		},
	});

	Create_Entity({
		.Name = String_Lit("Entity E"),
		.Position = V3(0.0f, 3.0f, 4.0f),
		.MeshName = String_Lit("Box"),
		.Material = {
			.Diffuse = {
				.IsTexture = true,
				.TextureName = String_Lit("Brick_Color")
			}
		},
		.BodyType = SIM_BODY_DYNAMIC,
		.Friction = 0.5f,
		.Collider = {
			.Type = SIM_COLLIDER_TYPE_BOX,
			.Box = { V3_All(1.0f), 1.0f }
		},
	});

	Create_Entity({
		.Name = String_Lit("Entity F"),
		.Position = V3(0.0f, 3.0f, 6.0f),
		.MeshName = String_Lit("Box"),
		.Material = {
			.Diffuse = {
				.IsTexture = true,
				.TextureName = String_Lit("Brick_Color")
			}
		},
		.BodyType = SIM_BODY_DYNAMIC,
		.Friction = 0.5f,
		.Collider = {
			.Type = SIM_COLLIDER_TYPE_BOX,
			.Box = { V3_All(1.0f), 1.0f }
		},
	});

	Create_Entity({
		.Name = String_Lit("Entity B"),
		.Position = V3(0.0f, 0.0f, -1.1f),
		.Scale = V3(30.0f, 30.0f, 0.1f),
		.MeshName = String_Lit("Box"),
		.Material = { 
			.Diffuse = {
				.IsTexture = true,
				.TextureName = String_Lit("Brick_Color")
			}
		},
		.Friction = 0.7f,
		.Collider = {
			.Type = SIM_COLLIDER_TYPE_BOX,
			.Box = { V3_All(1.0f) }
		},
	});

	Camera_Init(&Engine->Camera, V3_Zero());

	Engine->UpdateInput.MouseP = V2(FLT_MAX, FLT_MAX);
	Engine->SimInput.MouseP = V2(FLT_MAX, FLT_MAX);


	arena* Scratch = Scratch_Get();
	buffer FontBuffer = Read_Entire_File((allocator*)Scratch, String_Lit("fonts/LiberationMono.ttf"));
	if (Buffer_Is_Empty(FontBuffer)) return false;

	Engine->Font = Font_Create(FontBuffer);

	Engine->ThreadLocalStorage = OS_TLS_Create();

	Play_Sound(String_Lit("TestMusic"), SOUND_FLAG_LOOPING, 0.2f);
	Engine->DrawColliders = true;

	Engine_Start_Running();
	Sim_Resume();
	Scratch_Release();

#ifdef FLOW_DEBUG_RENDERING
	Engine->DrawFlowCmds = false;
#endif

	return true;
}

export_function ENGINE_FUNCTION_DEFINE(Engine_Reload) {
	Set_Engine(Engine);
	Engine->VTable = &Engine_VTable;

	Simulation_Reload(&Engine->Simulation);

	return true;
}

#ifdef OS_WIN32
#pragma comment(lib, "base.lib")
#pragma comment(lib, "user32.lib")
#endif