#ifndef JOLT_SIMULATION_H
#define JOLT_SIMULATION_H

#define JOLT_MAX_NUM_BODIES 10000
#define JOLT_MAX_BODY_PAIRS 20000
#define JOLT_MAX_CONTACT_CONSTRAINTS 100000


class jolt_object_layer_pair_filter : public ObjectLayerPairFilter {
public:
	virtual inline bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
		return true;
	}
};

class jolt_broad_phase_layer_filter : public ObjectVsBroadPhaseLayerFilter {
public:
	virtual inline bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
		return true;
	}
};

class jolt_layer_interface : public BroadPhaseLayerInterface {
public:
	BroadPhaseLayer Layer;

	inline jolt_layer_interface() : Layer(0) { }

	virtual inline uint GetNumBroadPhaseLayers() const override {
		return 1;
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
		return "DEFAULT";
	}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

	virtual inline BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
		return Layer;
	}
};

class jolt_temp_allocator : public TempAllocator {
public:
	arena* Scratch;

	inline jolt_temp_allocator() {
		Scratch = Scratch_Get();
	}

	virtual inline ~jolt_temp_allocator() {
		Scratch_Release();
	}

	virtual inline void* Allocate(uint inSize) override {
		return Arena_Push(Scratch, inSize);
	}

	virtual inline void Free(void *inAddress, uint inSize) override {
		//Noop
	}
};

struct sim_body {
	Body* Body;
};

struct jolt_backend : public simulation_backend {
	pool Bodies;
	PhysicsSystem System;
	BodyInterface BodyInterface;
	jolt_object_layer_pair_filter LayerPairFilter;
	jolt_broad_phase_layer_filter BroadPhaseLayerFilters;
	jolt_layer_interface BroadPhaseLayers;
	jolt_job_system JobSystem;
};

#endif