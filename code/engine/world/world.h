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

	b32 		   IsOn;
	v3  		   Color;
	gfx_shadow_map ShadowMap;

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

struct dir_light_create_info {
	string Name;
	quat   Orientation = Quat_Identity();
	v3 	   Color = V3(1.0f, 1.0f, 1.0f);
	f32    Intensity = 1.0f;
	b32    IsOn = true;
};

struct entity_slot {
	entity* First;
	entity* Last;
};

#define MAX_ENTITY_SLOT_COUNT (4096)
#define ENTITY_SLOT_MASK (MAX_ENTITY_SLOT_COUNT-1)
struct world {
	heap* 		Heap;
	string 		Name;
	pool   		Entities;
	entity_slot EntitySlots[MAX_ENTITY_SLOT_COUNT];
};

#endif