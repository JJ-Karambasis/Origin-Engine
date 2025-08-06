#include "flow_simulation.h"

function flow_body_type Get_Flow_Type(sim_body_type Type) {
	static flow_body_type Types[] = {
		FLOW_BODY_STATIC,
		FLOW_BODY_DYNAMIC
	};
	Assert(Type < Array_Count(Types));
	return Types[Type];
}

function flow_collider_type Get_Flow_Collider_Type(sim_collider_type Type) {
	static flow_collider_type Types[] = {
		FLOW_COLLIDER_TYPE_SPHERE,
		FLOW_COLLIDER_TYPE_BOX
	};
	Assert(Type < Array_Count(Types));
	return Types[Type];
}

function flow_collider Get_Flow_Collider(arena* TempArena, sim_collider Collider) {
	flow_collider Result = {
		.Type = Get_Flow_Collider_Type(Collider.Type)
	};

	switch (Collider.Type) {
		case SIM_COLLIDER_TYPE_SPHERE: {
			Result.Sphere.Radius = Collider.Sphere.Radius;
			Result.Sphere.Density = Collider.Sphere.Density;
		} break;

		case SIM_COLLIDER_TYPE_BOX: {
			Result.Box.HalfExtent = Collider.Box.HalfExtent;
			Result.Box.Density = Collider.Box.Density;
		} break;

		Invalid_Default_Case;
	}

	return Result;
}

function SIMULATION_BACKEND_CREATE_BODY(Flow_Backend_Create_Body) {
	flow_backend* Backend = (flow_backend *)Simulation->Backend;

	arena* Scratch = Scratch_Get();
	flow_body_create_info BodyInfo = {
		.Position = CreateInfo.Position,
		.Orientation = CreateInfo.Orientation,
		.Scale = CreateInfo.Scale,
		.Type = Get_Flow_Type(CreateInfo.BodyType),
		.Collider = Get_Flow_Collider(Scratch, CreateInfo.Collider),
		.Friction = CreateInfo.Friction,
		.UserData = CreateInfo.UserData
	};

	pool_id ID = Pool_Allocate(&Backend->Bodies);
	sim_body* Result = (sim_body*)Pool_Get(&Backend->Bodies, ID);
	Result->BodyID = Flow_Create_Body(Backend->System, &BodyInfo);
	Scratch_Release();

	return Result;
}

function SIMULATION_BACKEND_UPDATE(Flow_Backend_Update) {
	flow_backend* Backend = (flow_backend *)Simulation->Backend;
	
	for (size_t i = 0; i < Simulation->Entities.Capacity; i++) {
		sim_entity* SimEntity = Simulation->Entities.Entities + i;
		if (SimEntity->IsAllocated) {
			flow_body* Body = Flow_Get_Body(Backend->System, SimEntity->Body->BodyID);
			Body->Position = SimEntity->Position;
			Body->Orientation = SimEntity->Orientation;
			Body->LinearVelocity = SimEntity->LinearVelocity;
			Body->AngularVelocity = SimEntity->AngularVelocity;
			Body->Force = SimEntity->Force;
			Body->Torque = SimEntity->Torque;
		}
	}

	Flow_System_Update(Backend->System, dt);

	for (size_t i = 0; i < Simulation->Entities.Capacity; i++) {
		sim_entity* SimEntity = Simulation->Entities.Entities + i;
		if (SimEntity->IsAllocated) {
			flow_body* Body = Flow_Get_Body(Backend->System, SimEntity->Body->BodyID);
			SimEntity->Position = Body->Position;
			SimEntity->Orientation = Body->Orientation;
			SimEntity->LinearVelocity = Body->LinearVelocity;
			SimEntity->AngularVelocity = Body->AngularVelocity;
			SimEntity->Force = Body->Force;
			SimEntity->Torque = Body->Torque;
		}
	}
}

function SIMULATION_BACKEND_SET_GRAVITY(Flow_Backend_Set_Gravity) {
	flow_backend* Backend = (flow_backend *)Simulation->Backend;
	Flow_System_Set_Gravity(Backend->System, Gravity);
}

function void* Sim_Flow_Allocate_Memory(size_t Size, void* UserData) {
	void* Result = Allocator_Allocate_Memory(Default_Allocator_Get(), Size);
	return Result;
}

function void Sim_Flow_Free_Memory(void* Memory, void* UserData) {
	Allocator_Free_Memory(Default_Allocator_Get(), Memory);
}

global simulation_backend_vtable VTable = {
	.CreateBodyFunc = Flow_Backend_Create_Body,
	.UpdateFunc = Flow_Backend_Update,
	.SetGravityFunc = Flow_Backend_Set_Gravity
};

function void Simulation_Init_Backend(simulation* Simulation) {
	flow_backend* Backend = Arena_Push_Struct(Simulation->Arena, flow_backend);
	Backend->VTable = &VTable;
	Simulation->Backend = Backend;
	Backend->Bodies = Pool_Init(sizeof(sim_body));
	
	flow_allocator Allocator = {
		.AllocateMemory = Sim_Flow_Allocate_Memory,
		.FreeMemory = Sim_Flow_Free_Memory
	};

	flow_system_create_info FlowCreateInfo = {
		.Allocator = &Allocator
	};

	Backend->System = Flow_System_Create(&FlowCreateInfo);
}

#define FLOW_IMPLEMENTATION
#include <flow.h>