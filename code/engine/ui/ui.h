#ifndef UI_H
#define UI_H

typedef u64 ui_box_id;

enum {
	UI_BOX_FLAG_NONE = 0,
	UI_BOX_FLAG_FLOATING_X = (1 << 0),
	UI_BOX_FLAG_FLOATING_Y = (1 << 1),
	UI_BOX_FLAG_DRAW_TEXT = (1 << 2),
	UI_BOX_FLAG_RIGHT_ALIGN = (1 << 3),
	UI_BOX_FLAG_BOTTOM_ALIGN = (1 << 4)
};
typedef u64 ui_box_flags;

enum ui_axis {
	UI_AXIS_X,
	UI_AXIS_Y
};

enum ui_size_type {
	UI_SIZE_TYPE_NONE,
	UI_SIZE_TYPE_TEXT
};

struct ui_size {
	ui_size_type Type;
	f32 		 Value;
};

function inline ui_size UI_Size(ui_size_type Type, f32 Value) { 
	ui_size Result = { Type, Value }; 
	return Result; 
}

#define UI_Txt() UI_Size(UI_SIZE_TYPE_TEXT, 0.0f)

struct ui_font {
	font* Font;
	f32   Size;
};

function inline ui_font UI_Font(font* Font, f32 Size) {
	ui_font Result = { Font, Size };
	return Result;
}

enum {
	UI_BOX_STATE_NONE = 0,
	UI_BOX_STATE_HOVERING = (1 << 0),
	UI_BOX_STATE_MOUSE_LEFT_DOWN = (1 << 1),
	UI_BOX_STATE_MOUSE_LEFT_CLICKED = (1 << 2)
};
typedef u64 ui_box_state;

struct ui;
struct ui_box;

#define UI_CUSTOM_DRAW_CALLBACK(name) void name(gdi_render_pass* RenderPass, ui* UI, ui_box* Box, void* UserData)
typedef UI_CUSTOM_DRAW_CALLBACK(ui_custom_draw_callback_func);

struct ui_box {
	ui_box_id ID;

	ui_box* Parent;
	ui_box* FirstChild;
	ui_box* LastChild;
	ui_box* NextSibling;
	ui_box* PrevSibling;
	u32     ChildCount;

	ui_box* NextInHash;
	ui_box* PrevInHash;

	ui_box_flags Flags;
	ui_box_state CurrentState;
	ui_box_state LastState;
	ui_axis LayoutAxis;
	ui_size PrefSizes[2];
	v4 BackgroundColor;
	v4 TextColor;
	ui_font Font;
	string DisplayString;
	v2 DisplayStringDim;
	gfx_texture_id Texture;
	v2 FixedDim;
	v2 FixedP;

	rect2 Rect;

	ui_custom_draw_callback_func* CustomDrawCallback;
	void* CustomDrawCallbackUserData;

	u64 BuildIndex;
};

#define UI_LMB_Clicked(box) ((box)->CurrentState & UI_BOX_STATE_MOUSE_LEFT_CLICKED)

struct ui_box_hash_slot {
	ui_box* FirstBox;
	ui_box* LastBox;
};

#include "meta/ui_meta.h"

#define UI_MAX_BOX_SLOTS 4096
#define UI_BOX_SLOT_MASK (UI_MAX_BOX_SLOTS-1)
struct ui {
	arena* Arena;
	u64    CurrentBuildIndex;
	arena* BuildArenas[2];
	
	ui_box* 		 LastBox;
	ui_box* 		 Root;
	ui_box* 		 FreeBoxes;
	ui_box_hash_slot BoxSlots[UI_MAX_BOX_SLOTS];

	b32 HasBegun;

	ui_stack Stack;
};


global ui* G_UI;
function inline void UI_Set(ui* UI) {
	G_UI = UI;
}

function inline ui* UI_Get() {
	return G_UI;
}

#endif