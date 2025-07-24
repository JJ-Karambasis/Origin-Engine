function ui* UI_Get() {
	engine* Engine = Get_Engine();
	engine_thread_context* ThreadContext = Get_Engine_Thread_Context();
	if (!ThreadContext->UI) {
		arena* Arena = Arena_Create();
		ThreadContext->UI = Arena_Push_Struct(Arena, ui);
		ThreadContext->UI->Arena = Arena;
		ThreadContext->UI->BuildArenas[0] = Arena_Create(); 
		ThreadContext->UI->BuildArenas[1] = Arena_Create();
	}
	return ThreadContext->UI;
}

function ui* UI_Has_Begun() {
	ui* UI = UI_Get();
	Assert(UI->HasBegun);
	return UI;
}

function arena* UI_Build_Arena() {
	ui* UI = UI_Get();
	u64 Index = UI->CurrentBuildIndex % Array_Count(UI->BuildArenas);
	return UI->BuildArenas[Index];
}

function ui_box_id UI_Box_ID_From_Identifier(string Identifier, ui_box_id Seed) {
	ui_box_id Result = U64_Hash_String_With_Seed(Identifier, Seed);
	return Result;
}

function ui_box* UI_Get_Last_Box() {
	ui* UI = UI_Get();
	return UI->LastBox;
}

function void UI_Set_Position_X(ui_box* Box, f32 Value) {
	Box->Flags |= UI_BOX_FLAG_FLOATING_X;
	Box->FixedP.x = Value;
}

function void UI_Set_Position_Y(ui_box* Box, f32 Value) {
	Box->Flags |= UI_BOX_FLAG_FLOATING_Y;
	Box->FixedP.y = Value;
}

function ui_box* UI_Make_Box(ui_box_flags Flags, ui_box_id ID) {
	ui* UI = UI_Has_Begun();
	ui_box* Box = NULL;

	//The ID acts as the hash
	u64 SlotIndex = UI_BOX_SLOT_MASK & ID;
	ui_box_hash_slot* Slot = UI->BoxSlots + SlotIndex;

	//Check if its in the hash
	for (ui_box* HashBox = Slot->FirstBox; HashBox; HashBox = HashBox->NextInHash) {
		if (HashBox->ID == ID) {
			Box = HashBox;
			break;
		}
	}

	//Else create a new box and add it to the hash
	if (!Box) {
		Box = UI->FreeBoxes;
		if (Box) SLL_Pop_Front_N(UI->FreeBoxes, NextInHash);
		else {
			Box = Arena_Push_Struct_No_Clear(UI->Arena, ui_box);
		}
		Memory_Clear(Box, sizeof(ui_box));
		DLL_Push_Back_NP(Slot->FirstBox, Slot->LastBox, Box, NextInHash, PrevInHash);
	}

	Box->ID = ID;
	Box->Flags = Flags;
	Box->BuildIndex = UI->CurrentBuildIndex;

	//Add to the hierarchy
	if (UI_Has_Parent()) {
		ui_box* Parent = UI_Get_Parent();
		Box->Parent = Parent;
		DLL_Push_Back_NP(Parent->FirstChild, Parent->LastChild, Box, NextSibling, PrevSibling);
		Parent->ChildCount++;
	}

	//Set the per frame properties
	if (UI_Has_Background_Color()) {
		Box->BackgroundColor = UI_Get_Background_Color();
	}

	if (UI_Has_Font()) {
		Box->Font = UI_Get_Font();
	}

	Box->TextColor = V4_All(1.0f);
	if (UI_Has_Text_Color()) {
		Box->TextColor = UI_Get_Text_Color();
	}

	if (UI_Has_Layout_Axis()) {
		Box->LayoutAxis = UI_Get_Layout_Axis();
	}

	if (UI_Has_Pref_Width()) {
		Box->PrefSizes[UI_AXIS_X] = UI_Get_Pref_Width();
	}

	if (UI_Has_Pref_Height()) {
		Box->PrefSizes[UI_AXIS_Y] = UI_Get_Pref_Height();
	}

	if (UI_Has_Fixed_Width()) {
		Box->FixedDim.x = UI_Get_Fixed_Width();
	}

	if (UI_Has_Fixed_Height()) {
		Box->FixedDim.y = UI_Get_Fixed_Height();
	}

	if (UI_Has_Fixed_X()) {
		UI_Set_Position_X(Box, UI_Get_Fixed_X());
	}

	if (UI_Has_Fixed_Y()) {
		UI_Set_Position_Y(Box, UI_Get_Fixed_Y());
	}

	UI_Autopop_Stack();

	UI->LastBox = Box;

	return Box;
}

function void UI_Set_Display_String(ui_box* Box, string DisplayString) {
	Box->DisplayString = String_Copy((allocator*)UI_Build_Arena(), DisplayString);
}

function ui_box* UI_Make_Box_From_String(ui_box_flags Flags, string Identifier) {
	ui_box_id ParentID = 0;
	if (UI_Has_Parent()) {
		ParentID = UI_Get_Parent()->ID;
	}

	string DisplayPart = Identifier;
	string IdentifierPart = Identifier;
	
	size_t Index = String_Find_First(Identifier, String_Lit("###"));
	if (Index != STRING_INVALID_INDEX) {
		DisplayPart = String_Substr(Identifier, 0, Index);
		IdentifierPart = String_Substr(Identifier, Index+3, Identifier.Size);
	}

	ui_box_id ID = UI_Box_ID_From_Identifier(IdentifierPart, ParentID);
	ui_box* Box = UI_Make_Box(Flags, ID);
	
	if (Flags & UI_BOX_FLAG_DRAW_TEXT) {
		UI_Set_Display_String(Box, DisplayPart);
	}

	return Box;
}

function inline ui_box* UI_Make_Box_From_String_FormatV(ui_box_flags Flags, const char* Format, va_list List) {
	arena* Scratch = Scratch_Get();
	string Identifier = String_FormatV((allocator*)Scratch, Format, List);
	ui_box* Result = UI_Make_Box_From_String(Flags, Identifier);
	Scratch_Release();
	return Result;
}

function ui_box* UI_Make_Box_From_String_Format(ui_box_flags Flags, const char* Format, ...) {
	va_list List;
	va_start(List, Format);
	ui_box* Result = UI_Make_Box_From_String_FormatV(Flags, Format, List);
	va_end(List);
	return Result;
}

#ifdef DEBUG_BUILD
#define UI_Validate_Stack() UI_Validate_Stack_()
#else
#define UI_Validate_Stack()
#endif

function void UI_Begin(v2 ViewDim) {
	ui* UI = UI_Get();
	Assert(!UI->HasBegun);
	UI->HasBegun = true;
	UI->LastBox = NULL;

	//Reset the hierarchy
	for (u32 i = 0; i < UI_MAX_BOX_SLOTS; i++) {
		ui_box_hash_slot* Slot = &UI->BoxSlots[i];
		
		ui_box* Box = Slot->FirstBox;
		while (Box) {
			ui_box* BoxToCheck = Box;
			Box = Box->NextInHash;

			BoxToCheck->Parent = NULL;
			BoxToCheck->FirstChild = NULL;
			BoxToCheck->LastChild = NULL;
			BoxToCheck->NextSibling = NULL;
			BoxToCheck->PrevSibling = NULL;
			BoxToCheck->ChildCount = 0;
		}
	}

	UI_Set_Next_Fixed_Width(ViewDim.x);
	UI_Set_Next_Fixed_Height(ViewDim.y);
	UI_Set_Next_Layout_Axis(UI_AXIS_X);
	UI->Root = UI_Make_Box_From_String_Format(0, "Root %p", (void*)UI);
	UI_Push_Parent(UI->Root);
}

function void UI_Layout_Fixed_Size(ui_box* Box) {
	for (u32 AxisIndex = 0; AxisIndex < 2; AxisIndex++) {
		ui_axis Axis = (ui_axis)AxisIndex;

		if (Box->PrefSizes[Axis].Type == UI_SIZE_TYPE_TEXT) {
			Box->FixedDim.Data[Axis] = Box->DisplayStringDim.Data[Axis];
		}
	}

	for (ui_box* Child = Box->FirstChild; Child; Child = Child->NextSibling) {
		UI_Layout_Fixed_Size(Child);
	}
}

function void UI_Layout_Position(ui_box* Box) {
	if (!Box->Parent) {
		Box->Rect.p0 = Box->FixedP;
		Box->Rect.p1 = V2_Add_V2(Box->FixedP, Box->FixedDim);
	}

	v2 P = Box->Rect.p0;
	ui_axis LayoutAxis = Box->LayoutAxis;

	for (ui_box* Child = Box->FirstChild; Child; Child = Child->NextSibling) {
		if (!(Child->Flags & (UI_BOX_FLAG_FLOATING_X << LayoutAxis))) {
			Child->FixedP.Data[LayoutAxis] = P.Data[LayoutAxis];
		}

		Child->Rect.p0 = Child->FixedP;
		Child->Rect.p1 = V2_Add_V2(Child->FixedP, Child->FixedDim);

		if (!(Child->Flags & (UI_BOX_FLAG_FLOATING_X << LayoutAxis))) {
			P.Data[LayoutAxis] += Child->FixedDim.Data[LayoutAxis];
		}
	}

	for (ui_box* Child = Box->FirstChild; Child; Child = Child->NextSibling) {
		UI_Layout_Position(Child);
	}
}

function void UI_Layout(ui_box* Root) {
	UI_Layout_Fixed_Size(Root);
	UI_Layout_Position(Root);
}

function void UI_Calc_Display_String(ui_box* Box) {
	if (Box->Flags & UI_BOX_FLAG_DRAW_TEXT) {
		string DisplayString = Box->DisplayString;
		ui_font* Font = &Box->Font;
		const char* UTF8At = DisplayString.Ptr;
		const char* UTF8End = DisplayString.Ptr + DisplayString.Size;
		f32 Ascent = Font_Get_Ascent(Font->Font, Font->Size);

		u32 LastCodepoint = (u32)-1;

		Box->DisplayStringDim = V2(0.0f, Font_Get_Line_Height(Font->Font, Font->Size));
		for (;;) {
			if (UTF8At >= UTF8End) {
				Assert(UTF8At == UTF8End);
				return;
			}

			u32 Length;
			u32 Codepoint = UTF8_Read(UTF8At, &Length);
			UTF8At += Length;

			if (LastCodepoint != (u32)-1) {
				Box->DisplayStringDim.x += Font_Get_Kerning(Font->Font, LastCodepoint, Codepoint, Font->Size);
			}

			glyph* Glyph = Font_Get_Glyph(Font->Font, Codepoint, Font->Size);
			Box->DisplayStringDim.x += Glyph->Advance;
			LastCodepoint = Codepoint;
		}
	}

	for (ui_box* Child = Box->FirstChild; Child; Child = Child->NextSibling) {
		UI_Calc_Display_String(Child);
	}
}

function void UI_Reset_State(ui_box* Box) {
	Box->LastState = Box->CurrentState;
	Box->CurrentState = 0;

	for (ui_box* Child = Box->FirstChild; Child; Child = Child->NextSibling) {
		UI_Reset_State(Child);
	}
}

function void UI_Calc_State(ui_box* Box) {
	input_manager* InputManager = Input_Manager_Get();
	v2 MouseP = InputManager->MouseP;

	if (Rect2_Contains_V2(Box->Rect, MouseP)) {
		Box->CurrentState |= UI_BOX_STATE_HOVERING;

		if (Mouse_Is_Down(MOUSE_KEY_LEFT)) {
			Box->CurrentState |= UI_BOX_STATE_MOUSE_LEFT_DOWN;
		}

		if (Mouse_Is_Down(MOUSE_KEY_MIDDLE)) {
			Box->CurrentState |= UI_BOX_STATE_MOUSE_MIDDLE_DOWN;
		}

		if (Mouse_Is_Down(MOUSE_KEY_RIGHT)) {
			Box->CurrentState |= UI_BOX_STATE_MOUSE_RIGHT_DOWN;
		}
	}

	for (ui_box* Child = Box->FirstChild; Child; Child = Child->NextSibling) {
		UI_Calc_State(Child);
	}
}

function void UI_End() {
	ui* UI = UI_Has_Begun();
	UI_Pop_Parent();

	UI_Validate_Stack();

	UI_Calc_Display_String(UI->Root);

	UI_Reset_State(UI->Root);
	UI_Layout(UI->Root);
	UI_Calc_State(UI->Root);

	//Free all old boxes
	for (u32 i = 0; i < UI_MAX_BOX_SLOTS; i++) {
		ui_box_hash_slot* Slot = &UI->BoxSlots[i];
		
		ui_box* Box = Slot->FirstBox;
		while (Box) {
			ui_box* BoxToCheck = Box;
			Box = Box->NextInHash;

			if (BoxToCheck->BuildIndex < UI->CurrentBuildIndex) {
				DLL_Remove_NP(Slot->FirstBox, Slot->LastBox, BoxToCheck, NextInHash, PrevInHash);
				SLL_Push_Front_N(UI->FreeBoxes, BoxToCheck, NextInHash);
			}
		}
	}

	UI->CurrentBuildIndex++;
	Arena_Clear(UI_Build_Arena());

	UI->HasBegun = false;
}

#include "ui_widgets.cpp"
#include "meta/ui_meta.c"