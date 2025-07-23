#ifndef CAMERA_H
#define CAMERA_H

typedef struct {
	f32 Pitch;
	f32 Yaw;
	f32 Roll;
	f32 Distance;
	v3  Target;
} camera;

#endif