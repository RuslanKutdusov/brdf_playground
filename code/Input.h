#pragma once

struct InputEvent
{
	// WM_KEYDOWN, WM_KEYUP, WM_XBUTTONDOWN, WM_XBUTTONUP, etc.
	uint32_t event;
	union
	{
		uint32_t key;  // virtual key code, VK_*. Valid for WM_KEY* events
		struct
		{
			uint16_t cursorX;
			uint16_t cursorY;
		};
		int16_t mouseWheel;
	};
};


enum EMouseButton
{
	kMouseLButton = 1 << 0,
	kMouseMButton = 1 << 1,
	kMouseRButton = 1 << 2,
};


struct InputState
{
	static const uint32_t kKeysNum = 256;
	bool keyPressed[kKeysNum] = {};
	uint32_t mouseState = 0;
	uint16_t cursorX;
	uint16_t cursorY;
};
