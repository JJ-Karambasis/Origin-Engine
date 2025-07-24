#ifndef SHADER_DATA_H
#define SHADER_DATA_H

struct basic_shader_data {
	m4 ModelToClip;
	v4 C;
};

struct ui_shader_data {
	v2 InvResolution;
};

#endif