#ifndef INPUT_H
#define INPUT_H

struct input_manager {
	state KeyboardKeys[KEYBOARD_KEY_COUNT];
	state MouseKeys[MOUSE_KEY_COUNT];
	f32   MouseZoom;
	v2    MouseDelta;
	v2    MouseP;
};

#endif