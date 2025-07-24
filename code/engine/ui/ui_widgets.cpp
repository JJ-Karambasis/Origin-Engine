function void UI_Text(string Text) {
	UI_Set_Next_Pref_Width(UI_Txt());
	UI_Set_Next_Pref_Height(UI_Txt());
	UI_Make_Box_From_String(UI_BOX_FLAG_DRAW_TEXT, Text);
}

function void UI_Text_Formatted(const char* Format, ...) {
	arena* Scratch = Scratch_Get();
	
	va_list List;
	va_start(List, Format);
	string Text = String_FormatV((allocator*)Scratch, Format, List);
	va_end(List);

	UI_Text(Text);

	Scratch_Release();
}