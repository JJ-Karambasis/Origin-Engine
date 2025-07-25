#ifndef INPUT_H
#define INPUT_H

struct input_manager {
	state KeyboardKeys[KEYBOARD_KEY_COUNT];
	state MouseKeys[MOUSE_KEY_COUNT];
	f32   MouseZoom;
	v2    MouseDelta;
	v2    MouseP;
};

function input_manager* Input_Manager_Get();
function b32 Keyboard_Is_Pressed(u32 Key);
function b32 Keyboard_Is_Released(u32 Key);
function b32 Keyboard_Is_Down(u32 Key);
function b32 Mouse_Is_Pressed(u32 Key);
function b32 Mouse_Is_Released(u32 Key);
function b32 Mouse_Is_Down(u32 Key);

#endif