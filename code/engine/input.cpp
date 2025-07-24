function void Set_Input_Thread(input_manager* InputManager) {
	engine_thread_context* ThreadContext = Get_Engine_Thread_Context();
	ThreadContext->InputManager = InputManager;
}

function inline input_manager* Input_Manager_Get() {
	engine_thread_context* ThreadContext = Get_Engine_Thread_Context();
	return ThreadContext->InputManager;
}

function inline b32 Keyboard_Is_Pressed(u32 Key) {
	input_manager* InputManager = Input_Manager_Get();
	return State_Turned_On(&InputManager->KeyboardKeys[Key]);
}

function inline b32 Keyboard_Is_Released(u32 Key) {
	input_manager* InputManager = Input_Manager_Get();
	return State_Turned_Off(&InputManager->KeyboardKeys[Key]);
}

function inline b32 Keyboard_Is_Down(u32 Key) {
	input_manager* InputManager = Input_Manager_Get();
	return State_Is_On(&InputManager->KeyboardKeys[Key]);
}

function inline b32 Mouse_Is_Pressed(u32 Key) {
	input_manager* InputManager = Input_Manager_Get();
	return State_Turned_On(&InputManager->MouseKeys[Key]);
}

function inline b32 Mouse_Is_Released(u32 Key) {
	input_manager* InputManager = Input_Manager_Get();
	return State_Turned_Off(&InputManager->MouseKeys[Key]);
}

function inline b32 Mouse_Is_Down(u32 Key) {
	input_manager* InputManager = Input_Manager_Get();
	return State_Is_On(&InputManager->MouseKeys[Key]);
}

function inline void Input_Update() {
	input_manager* InputManager = Input_Manager_Get();
	for (u32 i = 0; i < KEYBOARD_KEY_COUNT; i++) {
		State_Update(&InputManager->KeyboardKeys[i]);
	}

	for (u32 i = 0; i < MOUSE_KEY_COUNT; i++) {
		State_Update(&InputManager->MouseKeys[i]);
	}

	InputManager->MouseZoom = 0.0f;
	InputManager->MouseDelta = V2_Zero();

	if (!Is_Focused()) {
		InputManager->MouseP = V2(FLT_MAX, FLT_MAX);
	}
}