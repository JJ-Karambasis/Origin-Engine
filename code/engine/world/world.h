#ifndef WORLD_H
#define WORLD_H

typedef pool_id entity_id;
struct entity {
	entity_id ID;
	string    Name;

	v3   Position;
	quat Orientation;
	v3   Scale;
};

struct sim_entity {
	v3   Position;
	quat Orientation;
	v3   Scale;
};

struct sim_entity_storage {
	memory_reserve Storage;
	sim_entity*    Entities;
};

struct world {
	string Name;
	pool   Entities;
	sim_entity_storage SimEntities;
	atomic_b32 Simulate;
};

#define Is_Simulating() Atomic_Load_B32(&World_Get()->Simulate)

function inline world* World_Get() {
	return Get_Engine()->World;
}

#endif