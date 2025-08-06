#ifndef SIMULATION_H
#define SIMULATION_H

#define SIMULATION_VELOCITY_ITERATIONS 5
#define SIMULATION_POSITION_ITERATIONS 2

struct sim_entity;
struct sim_entity_handle {
	u64 Generation;
	sim_entity* Entity;
};

struct sim_entity_storage {
	memory_reserve Storage;
	sim_entity*    Entities;
	size_t 		   Capacity;
};

enum sim_message_type {
	SIM_MESSAGE_TYPE_CREATE_ENTITY
};

enum sim_body_type {
	SIM_BODY_STATIC,
	SIM_BODY_DYNAMIC
};

enum sim_collider_type {
	SIM_COLLIDER_TYPE_SPHERE,
	SIM_COLLIDER_TYPE_BOX,
	SIM_COLLIDER_TYPE_COMPOUND
};

struct sim_sphere_collider {
	f32 Radius;
	f32 Density;
};

struct sim_box_collider {
	v3  HalfExtent;
	f32 Density;
};

struct sim_transformed_collider;
struct sim_collider {
	sim_collider_type Type;
	union {
		sim_sphere_collider 		   Sphere;
		sim_box_collider    		   Box;
		span<sim_transformed_collider> Colliders;
	};
};

struct sim_transformed_collider {
	v3   		 Position;
	quat 		 Orientation;
	sim_collider Collider;
};

struct sim_message_create_entity {
	entity_id 	  ID;
	entity_type   Type;
	v3 		  	  Position;
	quat 	  	  Orientation;
	v3 		  	  Scale;
	sim_collider  Collider;
	sim_body_type BodyType;
	f32 		  Friction;
};

struct sim_body;
struct sim_entity {
	b32  		  IsAllocated;
	u64  		  Generation;
	entity_id     ID;
	entity_type   Type;
	v3 		  	  Position;
	quat 	  	  Orientation;
	v3 		  	  Scale;
	v3 			  Force;
	v3 			  Torque;
	v3 			  LinearVelocity;
	v3 			  AngularVelocity;
	sim_collider  Collider;
	sim_body_type BodyType;
	f32 		  Friction;
	sim_body*     Body;
};

struct sim_message {
	sim_message_type Type;
	union {
		sim_message_create_entity CreateEntity;
	};
};

#define SIM_MESSAGE_QUEUE_COUNT 1024
struct sim_message_queue {
	sim_message Queue[SIM_MESSAGE_QUEUE_COUNT];
	u32 EntryToRead;
	u32 EntryToWrite;
	os_mutex* Lock;
};

struct sim_push_cmd {
	entity_id ID;
	v3 		  Position;
	quat 	  Orientation;
};

#define MAX_SIM_PUSH_CMD_COUNT 1024
struct sim_push_frame {
	arena* 		  Arena;
	sim_push_cmd  Cmds[MAX_SIM_PUSH_CMD_COUNT];
	u32 	 	  CmdCount;
	
	high_res_time Time;
	sim_push_frame* Next;
};

struct sim_body_create_info {
	v3 		  	  Position;
	quat 	  	  Orientation;
	v3 		  	  Scale;
	sim_collider  Collider;
	sim_body_type BodyType;
	f32 		  Friction;
	void* 		  UserData;
};

struct simulation;
#define SIMULATION_BACKEND_CREATE_BODY(name) sim_body* name(simulation* Simulation, const sim_body_create_info& CreateInfo)
#define SIMULATION_BACKEND_UPDATE(name) void name(simulation* Simulation, f64 dt)
#define SIMULATION_BACKEND_SET_GRAVITY(name) void name(simulation* Simulation, v3 Gravity)

typedef SIMULATION_BACKEND_CREATE_BODY(simulation_backend_create_body_func);
typedef SIMULATION_BACKEND_UPDATE(simulation_backend_update_func);
typedef SIMULATION_BACKEND_SET_GRAVITY(simulation_backend_set_gravity_func);

struct simulation_backend_vtable {
	simulation_backend_create_body_func* CreateBodyFunc;
	simulation_backend_update_func* UpdateFunc;
	simulation_backend_set_gravity_func* SetGravityFunc;
};

struct simulation_backend {
	simulation_backend_vtable* VTable;
};

struct simulation {
	arena* 	            Arena;
	pool 				Bodies;
	simulation_backend* Backend;

	atomic_b32      IsSimulating;
	os_event*       WaitEvent;
	os_mutex*       FrameLock;
	sim_push_frame* FirstFrame;
	sim_push_frame* CurrentFrame;
	sim_push_frame* LastFrame;
	sim_push_frame* FreeFrames;
	high_res_time   Time;
	high_res_time   UpdateTime; //This is written by the update thread and not the sim thread

	sim_entity_storage Entities;
	sim_message_queue  MessageQueue;
};

function inline sim_body* Simulation_Create_Body(simulation* Simulation, const sim_body_create_info& CreateInfo) { return Simulation->Backend->VTable->CreateBodyFunc(Simulation, CreateInfo); }
function inline void Simulation_Update(simulation* Simulation, f64 dt) { Simulation->Backend->VTable->UpdateFunc(Simulation, dt); }
function inline void Simulation_Set_Gravity(simulation* Simulation, v3 Gravity) { Simulation->Backend->VTable->SetGravityFunc(Simulation, Gravity); }

function void Sim_Add_Message(sim_message_type Type, void* Data);
function void Simulation_Init_Backend(simulation* Simulation);

#endif