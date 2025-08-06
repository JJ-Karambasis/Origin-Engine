#ifndef FLOW_SIMULATION_H
#define FLOW_SIMULATION_H

#define FLOW_VEC3_CLASS_EXTRA \
constexpr flow_v3(const v3& V) : x(V.x), y(V.y), z(V.z) {} \
	operator v3() const { return V3(x, y, z); }

#define FLOW_QUAT_CLASS_EXTRA \
constexpr flow_quat(const quat& V) : x(V.x), y(V.y), z(V.z), w(V.w) {} \
	operator quat() const { return Quat(x, y, z, w); }

#define FLOW_MAX_VELOCITY_ITERATIONS SIMULATION_VELOCITY_ITERATIONS
#define FLOW_MAX_POSITION_ITERATIONS SIMULATION_POSITION_ITERATIONS
#include <flow.h>

struct sim_body {
	flow_body_id BodyID;
};

struct flow_backend : simulation_backend {
	flow_system* System;
	pool 		 Bodies;
};

#endif