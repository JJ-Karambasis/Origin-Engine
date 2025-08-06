#ifndef WORLD_H
#define WORLD_H

struct entity {
	entity_id 	ID;
	string    	Name;
	u64 	  	Hash;
	entity_type Type;

	v3   Position;
	quat Orientation;
	v3   Scale;

	gfx_component_id GfxComponent;
	sim_collider Collider;

	entity* Next;
	entity* Prev;
};

struct entity_sim_info {
};

struct entity_create_info {
	string Name;
	entity_type Type = ENTITY_TYPE_STATIC;
	v3 Position;
	quat Orientation = Quat_Identity();
	v3 Scale = V3_All(1.0f);
	string MeshName;
	material_info Material;
	sim_body_type BodyType;
	f32 Friction = 0.05f;
	sim_collider Collider = {};
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
	entity_slot EntitySlots[MAX_ENTITY_SLOT_COUNT];
};

#endif