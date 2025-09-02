#ifndef CAMERA_H
#define CAMERA_H

typedef struct {
	f32 Pitch;
	f32 Yaw;
	f32 Roll;
	f32 Distance;
	v3  Target;
	f32 FieldOfView;
	f32 ZNear;
	f32 ZFar;
} camera;

struct camera_frustum {
	v3 P[8];
};

#endif