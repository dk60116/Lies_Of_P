#pragma once

#ifndef Engine_Enum_h__
#define Engine_Enum_h__

namespace Engine
{
	enum class WINMODE { FULL, MODE_WINDOW };

	enum MOUSEKEYSTATE { DIM_LB, DIM_RB, DIM_MB, DIM_END };
	enum MOUSEMOVESTATE { DIMS_X, DIMS_Y, DIMS_Z, DIMS_END };

	enum Layer { DEFAULT, UI, LAYER_END };

	enum KEY_CODE
	{
		KEY_START = 0,

		Alpha0 = 0x30, Alpha1 = 0x31, Alpha2 = 0x32, Alpha3 = 0x33, Alpha4 = 0x34, Alpha5 = 0x35, Alpha6 = 0x36, Alpha7 = 0x37, Alpha8 = 0x38, Alpha9 = 0x39,

		Q = 'Q', W = 'W', E = 'E', R = 'R', T = 'T', Y = 'Y', U = 'U', I = 'I', O = 'O', P = 'P',
		A = 'A', S = 'S', D = 'D', F = 'F', G = 'G', H = 'H', J = 'J', K = 'K', L = 'L',
		Z = 'Z', X = 'X', C = 'C', V = 'V', B = 'B', N = 'N', M = 'M',

		SHIFT = VK_SHIFT, L_SHIFT = VK_LSHIFT, R_SHIFT = VK_RSHIFT, CONTROL = VK_CONTROL, L_CONTROL = VK_LCONTROL, R_CONTROL = VK_RCONTROL,
		ALT = VK_MENU, L_ALT = VK_LMENU, R_ALT = VK_RMENU,

		RETURN = VK_RETURN, ESCAPE = VK_ESCAPE, SPACE = VK_SPACE, TAB = VK_TAB, KEY_DELETE = VK_DELETE,

		UP = VK_UP, DOWN = VK_DOWN, LEFT = VK_LEFT, RIGHT = VK_RIGHT,

		MOUSE_L = VK_LBUTTON, MOUSE_R = VK_RBUTTON, MOUSE_WHILL = VK_MBUTTON,

		KEY_END
	};
}
#endif // Engine_Enum_h__
