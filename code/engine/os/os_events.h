#ifndef OS_EVENTS_H
#define OS_EVENTS_H

enum keyboard_key {
	KEYBOARD_KEY_CTRL=256,
	KEYBOARD_KEY_SHIFT=257,
	KEYBOARD_KEY_ALT=258,
	KEYBOARD_KEY_BACKTICK=259,
	KEYBOARD_KEY_LEFT=260,
	KEYBOARD_KEY_RIGHT=261,
	KEYBOARD_KEY_UP=262,
	KEYBOARD_KEY_DOWN=263,
	KEYBOARD_KEY_DELETE=264,
	KEYBOARD_KEY_COUNT=265
};

enum mouse_key {
	MOUSE_KEY_LEFT,
	MOUSE_KEY_RIGHT,
	MOUSE_KEY_MIDDLE,
	MOUSE_KEY_COUNT
};

enum os_event_type {
	OS_EVENT_TYPE_KEYBOARD_DOWN,
	OS_EVENT_TYPE_KEYBOARD_UP,
	OS_EVENT_TYPE_MOUSE_DOWN,
	OS_EVENT_TYPE_MOUSE_UP,
	OS_EVENT_TYPE_MOUSE_MOVE,
	OS_EVENT_TYPE_MOUSE_DELTA,
	OS_EVENT_TYPE_MOUSE_SCROLL,
	OS_EVENT_TYPE_CHAR
};

struct os_keyboard_event {
	keyboard_key Key;
};

struct os_mouse_event {
	mouse_key Key;
};

struct os_mouse_move_event {
	union {
		v2 Delta;
		v2 P;
	};
};

struct os_mouse_scroll_event {
	f32 Delta;
};

struct os_char_event {
	u32 Codepoint;
};

struct os_event_entry {
	os_event_type Type;
	union {
		os_keyboard_event 	  Keyboard;
		os_mouse_event 		  Mouse;
		os_mouse_move_event   MouseMove;
		os_mouse_scroll_event MouseScroll;
		os_char_event 		  Char;
	};
};

#define OS_EVENT_QUEUE_COUNT (4096)
struct os_event_queue {
	os_mutex* 	   Lock;
	u32 	  	   EntryToRead;
	u32 	  	   EntryToWrite;
	os_event_entry Queue[OS_EVENT_QUEUE_COUNT];
} ;


#endif