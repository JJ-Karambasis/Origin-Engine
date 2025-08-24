function void UI_Text(ui_box_flags Flags, string Text) {
	UI_Set_Next_Pref_Width(UI_Txt());
	UI_Set_Next_Pref_Height(UI_Txt());
	UI_Make_Box_From_String(Flags|UI_BOX_FLAG_DRAW_TEXT, Text);
}

function void UI_Text_Formatted(ui_box_flags Flags, const char* Format, ...) {
	arena* Scratch = Scratch_Get();
	
	va_list List;
	va_start(List, Format);
	string Text = String_FormatV((allocator*)Scratch, Format, List);
	va_end(List);

	UI_Text(Flags, Text);

	Scratch_Release();
}

function void UI_Texture(u32 Width, u32 Height, gfx_texture_id ID) {
	UI_Set_Next_Fixed_Width(Width);
	UI_Set_Next_Fixed_Height(Height);
	UI_Set_Next_Texture(ID);
	UI_Set_Next_Background_Color(V4_All(1.0f));
	UI_Make_Box_From_String_Format(0, "%" PRIu64, ID.ID);
}