#ifndef WORLD_H
#define WORLD_H

enum entity_type {
	ENTITY_TYPE_STATIC,
	ENTITY_TYPE_PLAYER
};

typedef pool_id entity_id;
struct entity {
	entity_id 	ID;
	string    	Name;
	u64 	  	Hash;
	entity_type Type;

	v3   Position;
	quat Orientation;
	v3   Scale;

	gfx_component_id GfxComponent;

	entity* Next;
	entity* Prev;
};

struct entity_create_info {
	string Name;
	entity_type Type = ENTITY_TYPE_STATIC;
	v3 Position;
	quat Orientation = Quat_Identity();
	v3 Scale = V3_All(1.0f);
	v4 Color;
	string MeshName;
};

struct sim_entity {
	b32  		IsAllocated;
	u64  		Generation;
	entity_id   ID;
	entity_type Type;
	v3   		Position;
	quat 		Orientation;
	v3   		Scale;
};

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

struct sim_message_create_entity {
	entity_id 	ID;
	entity_type Type;
	v3 		  	Position;
	quat 	  	Orientation;
	v3 		  	Scale;
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

struct entity_slot {
	entity* First;
	entity* Last;
};

#define MAX_ENTITY_SLOT_COUNT (4096)
#define ENTITY_SLOT_MASK (MAX_ENTITY_SLOT_COUNT-1)
struct world {
	heap* Heap;
	string Name;
	pool   Entities;
	sim_entity_storage SimEntities;
	entity_slot EntitySlots[MAX_ENTITY_SLOT_COUNT];
	sim_message_queue SimMessageQueue;
};

#endif