#include <stdint.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "wimeapi.h"
#include "win.h"
#include "winkey.h"

uint8_t* Xk2Vk[256];
extern int KeyMap[][2];

 __attribute__((constructor))
void initkeymap(void)
{
    uint8_t **p;

    for(int n=0; KeyMap[n][0] != XK_VoidSymbol; ++n){
	p = Xk2Vk + (KeyMap[n][0]>>8);
	if(*p == NULL)
	    *p = calloc(256,1);
	*(*p + (KeyMap[n][0] & 0xff)) = KeyMap[n][1];
    }
}

/*
  Xのkeysymをwinのキーコードにする。
  上8bitには修飾キー状態がセットされる
  先にinitkeymap()を呼んでおくこと。→コンストラクタにしたので明示的に呼ぶ必要なし
*/
unsigned ConvToVk(KeySym ks,unsigned state)
{
    unsigned grp = ks>>8;
    unsigned vk=0;

    if(grp<=0xff && Xk2Vk[grp]!=NULL)
	vk = Xk2Vk[grp][ks & 0xff];
    if(state & ShiftMask)
	vk |= VKMODKEY(WINMODKEY_SHIFT);
    if(state & ControlMask)
	vk |= VKMODKEY(WINMODKEY_CTRL);
    if(state & Mod1Mask)
	vk |= VKMODKEY(WINMODKEY_ALT);
    if(state & LockMask)
	vk |= VKMODKEY(WINMODKEY_LOCK);
    return vk;
}

int KeyMap[][2]={
    {XK_BackSpace,VK_BACK}, /* Back space, back char */
    {XK_Tab,VK_TAB},
    {XK_Clear,VK_CLEAR},
    {XK_Return,VK_RETURN},  /* Return, enter */
    {XK_Pause,VK_PAUSE},  /* Pause, hold */
    {XK_Escape,VK_ESCAPE},
    {XK_Delete,VK_DELETE},  /* Delete, rubout */

    {XK_Home,VK_HOME},
    {XK_Left,VK_LEFT},  /* Move left, left arrow */
    {XK_Up,VK_UP},  /* Move up, up arrow */
    {XK_Right,VK_RIGHT},  /* Move right, right arrow */
    {XK_Down,VK_DOWN},  /* Move down, down arrow */
    {XK_Page_Up,VK_PRIOR},
    {XK_Page_Down,VK_NEXT},
    {XK_End,VK_END},  /* EOL */
    {XK_Begin,VK_HOME},  /* BOL */

    {XK_Select,VK_SELECT},  /* Select, mark */
    {XK_Print,VK_PRINT},
    {XK_Execute,VK_EXECUTE},  /* Execute, run, do */
    {XK_Insert,VK_INSERT},  /* Insert, insert here */
    {XK_Menu,VK_MENU},
    {XK_Cancel,VK_CANCEL},  /* Cancel, stop, abort, exit */
    {XK_Help,VK_HELP},  /* Help */
    {XK_Num_Lock,VK_NUMLOCK},

    {XK_KP_Multiply,VK_MULTIPLY},
    {XK_KP_Add,VK_ADD},
    {XK_KP_Separator,VK_SEPARATOR},  /* Separator, often comma */
    {XK_KP_Subtract,VK_SUBTRACT},
    {XK_KP_Decimal,VK_DECIMAL},
    {XK_KP_Divide,VK_DIVIDE},

    {XK_KP_0,VK_NUMPAD0},
    {XK_KP_1,VK_NUMPAD1},
    {XK_KP_2,VK_NUMPAD2},
    {XK_KP_3,VK_NUMPAD3},
    {XK_KP_4,VK_NUMPAD4},
    {XK_KP_5,VK_NUMPAD5},
    {XK_KP_6,VK_NUMPAD6},
    {XK_KP_7,VK_NUMPAD7},
    {XK_KP_8,VK_NUMPAD8},
    {XK_KP_9,VK_NUMPAD9},

    {XK_F1,VK_F1},
    {XK_F2,VK_F2},
    {XK_F3,VK_F3},
    {XK_F4,VK_F4},
    {XK_F5,VK_F5},
    {XK_F6,VK_F6},
    {XK_F7,VK_F7},
    {XK_F8,VK_F8},
    {XK_F9,VK_F9},
    {XK_F10,VK_F10},
    {XK_F11,VK_F11},
    {XK_F12,VK_F12},
    {XK_F13,VK_F13},
    {XK_F14,VK_F14},
    {XK_F15,VK_F15},
    {XK_F16,VK_F16},
    {XK_F17,VK_F17},
    {XK_F18,VK_F18},
    {XK_F19,VK_F19},
    {XK_F20,VK_F20},
    {XK_F21,VK_F21},
    {XK_F22,VK_F22},
    {XK_F23,VK_F23},
    {XK_F24,VK_F24},

    {XK_Shift_L,VK_SHIFT},  /* Left shift */
    {XK_Shift_R,VK_SHIFT},  /* Right shift */
    {XK_Control_L,VK_CONTROL},  /* Left control */
    {XK_Control_R,VK_CONTROL},  /* Right control */
    {XK_Caps_Lock,VK_CAPITAL},  /* Caps lock */
    {XK_Alt_L,VK_MENU},  /* Left alt */
    {XK_Alt_R,VK_MENU},  /* Right alt */
    {XK_Super_L,VK_LWIN},  /* Left super */
    {XK_Super_R,VK_RWIN},  /* Right super */

    {XK_space,VK_SPACE},  /* U+0020 SPACE */
    {XK_apostrophe,VK_OEM_7},  /* U+0027 APOSTROPHE */
    {XK_comma,VK_OEM_COMMA},  /* U+002C COMMA */
    {XK_minus,VK_OEM_MINUS},  /* U+002D HYPHEN-MINUS */
    {XK_period,VK_OEM_PERIOD},  /* U+002E FULL STOP */
    {XK_slash,VK_OEM_2}, /* U+002F SOLIDUS */
    {XK_0,'0'},  /* U+0030 DIGIT ZERO */
    {XK_1,'1'},  /* U+0031 DIGIT ONE */
    {XK_2,'2'},  /* U+0032 DIGIT TWO */
    {XK_3,'3'},  /* U+0033 DIGIT THREE */
    {XK_4,'4'},  /* U+0034 DIGIT FOUR */
    {XK_5,'5'},  /* U+0035 DIGIT FIVE */
    {XK_6,'6'},  /* U+0036 DIGIT SIX */
    {XK_7,'7'},  /* U+0037 DIGIT SEVEN */
    {XK_8,'8'},  /* U+0038 DIGIT EIGHT */
    {XK_9,'9'},  /* U+0039 DIGIT NINE */
    {XK_semicolon,VK_OEM_1},  /* U+003B SEMICOLON */
    {XK_equal,VK_OEM_PLUS},  /* U+003D EQUALS SIGN */
    {XK_bracketleft,VK_OEM_4},  /* U+005B LEFT SQUARE BRACKET */
    {XK_backslash,VK_OEM_5},  /* U+005C REVERSE SOLIDUS */
    {XK_bracketright,VK_OEM_6},  /* U+005D RIGHT SQUARE BRACKET */
    {XK_grave,VK_OEM_3},  /* U+0060 GRAVE ACCENT */
    {XK_a,'A'},  /* U+0061 LATIN SMALL LETTER A */
    {XK_b,'B'},  /* U+0062 LATIN SMALL LETTER B */
    {XK_c,'C'},  /* U+0063 LATIN SMALL LETTER C */
    {XK_d,'D'},  /* U+0064 LATIN SMALL LETTER D */
    {XK_e,'E'},  /* U+0065 LATIN SMALL LETTER E */
    {XK_f,'F'},  /* U+0066 LATIN SMALL LETTER F */
    {XK_g,'G'},  /* U+0067 LATIN SMALL LETTER G */
    {XK_h,'H'},  /* U+0068 LATIN SMALL LETTER H */
    {XK_i,'I'},  /* U+0069 LATIN SMALL LETTER I */
    {XK_j,'J'},  /* U+006A LATIN SMALL LETTER J */
    {XK_k,'K'},  /* U+006B LATIN SMALL LETTER K */
    {XK_l,'L'},  /* U+006C LATIN SMALL LETTER L */
    {XK_m,'M'},  /* U+006D LATIN SMALL LETTER M */
    {XK_n,'N'},  /* U+006E LATIN SMALL LETTER N */
    {XK_o,'O'},  /* U+006F LATIN SMALL LETTER O */
    {XK_p,'P'},  /* U+0070 LATIN SMALL LETTER P */
    {XK_q,'Q'},  /* U+0071 LATIN SMALL LETTER Q */
    {XK_r,'R'},  /* U+0072 LATIN SMALL LETTER R */
    {XK_s,'S'},  /* U+0073 LATIN SMALL LETTER S */
    {XK_t,'T'},  /* U+0074 LATIN SMALL LETTER T */
    {XK_u,'U'},  /* U+0075 LATIN SMALL LETTER U */
    {XK_v,'V'},  /* U+0076 LATIN SMALL LETTER V */
    {XK_w,'W'},  /* U+0077 LATIN SMALL LETTER W */
    {XK_x,'X'},  /* U+0078 LATIN SMALL LETTER X */
    {XK_y,'Y'},  /* U+0079 LATIN SMALL LETTER Y */
    {XK_z,'Z'},  /* U+007A LATIN SMALL LETTER Z */

    {XK_VoidSymbol,0}
};
