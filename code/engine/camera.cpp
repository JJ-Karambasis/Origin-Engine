function void Camera_Init(camera* Camera, v3 Target) {
	Memory_Clear(Camera, sizeof(camera));
	Camera->Target   = Target;
	Camera->Distance = 3.162f;
	Camera->Pitch    = To_Radians(16.0f);
	Camera->ZNear    = 0.1f;
	Camera->ZFar     = 10000.0f;
	Camera->FieldOfView = To_Radians(60.0f);
}

function m3 Camera_Get_Orientation(camera* Camera) {
	quat RotX = Quat_RotX(Camera->Pitch);
	quat RotY = Quat_RotY(Camera->Yaw);
	quat RotZ = Quat_RotZ(Camera->Roll);

	quat RotXY = Quat_Mul_Quat(RotY, RotX);
	quat Orientation = Quat_Mul_Quat(RotZ, RotXY);

	return M3_From_Quat(Orientation);
}

function v3 Camera_Get_Position(camera* Camera) {
	m3 Orientation = Camera_Get_Orientation(Camera);
	v3 Position = V3_Add_V3(Camera->Target, V3_Mul_S(Orientation.z, Camera->Distance));
	return Position;
}

function m4_affine Camera_Get_View(camera* Camera) {
	m3 Orientation = Camera_Get_Orientation(Camera);
	v3 Position = V3_Add_V3(Camera->Target, V3_Mul_S(Orientation.z, Camera->Distance));
	m4_affine Result = M4_Affine_Inverse_Transform_No_Scale(Position, &Orientation);
	return Result;
}

function void Camera_Move_Arcball(camera* Camera, f32 dt) {
	input_manager* InputManager = Input_Manager_Get();
	if (Mouse_Is_Down(MOUSE_KEY_MIDDLE)) {
		v2 MouseDelta = InputManager->MouseDelta;
		if (Keyboard_Is_Down(KEYBOARD_KEY_CTRL)) {
			f32 Zoom = MouseDelta.y;
			Camera->Distance = Max(Camera->Distance-(Zoom * dt * 10), 1e-4f);
		} else if (Keyboard_Is_Down(KEYBOARD_KEY_SHIFT)) {
			m3 Orientation = Camera_Get_Orientation(Camera);
			Camera->Target = V3_Add_V3(Camera->Target, V3_Mul_S(Orientation.x, MouseDelta.x * dt * 1.5f));
			Camera->Target = V3_Add_V3(Camera->Target, V3_Mul_S(Orientation.y, MouseDelta.y * dt * 1.5f));
		} else {
			Camera->Pitch += MouseDelta.y * dt;
			Camera->Roll += MouseDelta.x * dt;
		}
	}

	if (InputManager->MouseZoom != 0) {
		Camera->Distance = Max(Camera->Distance-(InputManager->MouseZoom * dt * 100), 1e-4f);
	}
}