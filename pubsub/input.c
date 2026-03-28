// Copyright (c) 2024 Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>
#include <SDL.h>
#include "libretro.h"
#include "args.h"
#include "log.h"
#include "timing.h"
#include "input.h"

#define LOG_TAG "input"

#define MAX_DEVICES           16
#define LAST_BUTTON_ID        RETRO_DEVICE_ID_JOYPAD_R3
#define EMULATED_BUTTON_COUNT (LAST_BUTTON_ID - 3)
#define JOY_DEADZONE          0x4000
#define DEFAULT_CONFIG        "y,x,l,b,a,r,start,select,l2,r2,l3,r3"

static int keycode_retro_map[] = {
    RETROK_DUMMY,        //   0 0x00 KEY_RESERVED
    RETROK_ESCAPE,       //   1 0x01 KEY_ESC
    RETROK_1,            //   2 0x02 KEY_1
    RETROK_2,            //   3 0x03 KEY_2
    RETROK_3,            //   4 0x04 KEY_3
    RETROK_4,            //   5 0x05 KEY_4
    RETROK_5,            //   6 0x06 KEY_5
    RETROK_6,            //   7 0x07 KEY_6
    RETROK_7,            //   8 0x08 KEY_7
    RETROK_8,            //   9 0x09 KEY_8
    RETROK_9,            //  10 0x0A KEY_9
    RETROK_0,            //  11 0x0B KEY_0
    RETROK_MINUS,        //  12 0x0C KEY_MINUS
    RETROK_EQUALS,       //  13 0x0D KEY_EQUAL
    RETROK_BACKSPACE,    //  14 0x0E KEY_BACKSPACE
    RETROK_TAB,          //  15 0x0F KEY_TAB
    RETROK_q,            //  16 0x10 KEY_Q
    RETROK_w,            //  17 0x11 KEY_W
    RETROK_e,            //  18 0x12 KEY_E
    RETROK_r,            //  19 0x13 KEY_R
    RETROK_t,            //  20 0x14 KEY_T
    RETROK_y,            //  21 0x15 KEY_Y
    RETROK_u,            //  22 0x16 KEY_U
    RETROK_i,            //  23 0x17 KEY_I
    RETROK_o,            //  24 0x18 KEY_O
    RETROK_p,            //  25 0x19 KEY_P
    RETROK_LEFTBRACKET,  //  26 0x1A KEY_LEFTBRACE
    RETROK_RIGHTBRACKET, //  27 0x1B KEY_RIGHTBRACE
    RETROK_RETURN,       //  28 0x1C KEY_ENTER
    RETROK_LCTRL,        //  29 0x1D KEY_LEFTCTRL
    RETROK_a,            //  30 0x1E KEY_A
    RETROK_s,            //  31 0x1F KEY_S
    RETROK_d,            //  32 0x20 KEY_D
    RETROK_f,            //  33 0x21 KEY_F
    RETROK_g,            //  34 0x22 KEY_G
    RETROK_h,            //  35 0x23 KEY_H
    RETROK_j,            //  36 0x24 KEY_J
    RETROK_k,            //  37 0x25 KEY_K
    RETROK_l,            //  38 0x26 KEY_L
    RETROK_SEMICOLON,    //  39 0x27 KEY_SEMICOLON
    RETROK_QUOTE,        //  40 0x28 KEY_APOSTROPHE
    RETROK_BACKQUOTE,    //  41 0x29 KEY_GRAVE
    RETROK_LSHIFT,       //  42 0x2A KEY_LEFTSHIFT
    RETROK_BACKSLASH,    //  43 0x2B KEY_BACKSLASH
    RETROK_z,            //  44 0x2C KEY_Z
    RETROK_x,            //  45 0x2D KEY_X
    RETROK_c,            //  46 0x2E KEY_C
    RETROK_v,            //  47 0x2F KEY_V
    RETROK_b,            //  48 0x30 KEY_B
    RETROK_n,            //  49 0x31 KEY_N
    RETROK_m,            //  50 0x32 KEY_M
    RETROK_COMMA,        //  51 0x33 KEY_COMMA
    RETROK_PERIOD,       //  52 0x34 KEY_DOT
    RETROK_SLASH,        //  53 0x35 KEY_SLASH
    RETROK_RSHIFT,       //  54 0x36 KEY_RIGHTSHIFT
    RETROK_KP_MULTIPLY,  //  55 0x37 KEY_KPASTERISK
    RETROK_LALT,         //  56 0x38 KEY_LEFTALT
    RETROK_SPACE,        //  57 0x39 KEY_SPACE
    RETROK_CAPSLOCK,     //  58 0x3A KEY_CAPSLOCK
    RETROK_F1,           //  59 0x3B KEY_F1
    RETROK_F2,           //  60 0x3C KEY_F2
    RETROK_F3,           //  61 0x3D KEY_F3
    RETROK_F4,           //  62 0x3E KEY_F4
    RETROK_F5,           //  63 0x3F KEY_F5
    RETROK_F6,           //  64 0x40 KEY_F6
    RETROK_F7,           //  65 0x41 KEY_F7
    RETROK_F8,           //  66 0x42 KEY_F8
    RETROK_F9,           //  67 0x43 KEY_F9
    RETROK_F10,          //  68 0x44 KEY_F10
    RETROK_NUMLOCK,      //  69 0x45 KEY_NUMLOCK
    RETROK_SCROLLOCK,    //  70 0x46 KEY_SCROLLLOCK
    RETROK_KP7,          //  71 0x47 KEY_KP7
    RETROK_KP8,          //  72 0x48 KEY_KP8
    RETROK_KP9,          //  73 0x49 KEY_KP9
    RETROK_KP_MINUS,     //  74 0x4A KEY_KPMINUS
    RETROK_KP4,          //  75 0x4B KEY_KP4
    RETROK_KP5,          //  76 0x4C KEY_KP5
    RETROK_KP6,          //  77 0x4D KEY_KP6
    RETROK_KP_PLUS,      //  78 0x4E KEY_KPPLUS
    RETROK_KP1,          //  79 0x4F KEY_KP1
    RETROK_KP2,          //  80 0x50 KEY_KP2
    RETROK_KP3,          //  81 0x51 KEY_KP3
    RETROK_KP0,          //  82 0x52 KEY_KP0
    RETROK_KP_PERIOD,    //  83 0x53 KEY_KPDOT
    RETROK_DUMMY,        //  84 0x54
    RETROK_DUMMY,        //  85 0x55 KEY_ZENKAKUHANKAKU
    RETROK_DUMMY,        //  86 0x56 KEY_102ND
    RETROK_F11,          //  87 0x57 KEY_F11
    RETROK_F12,          //  88 0x58 KEY_F12
    RETROK_DUMMY,        //  89 0x59 KEY_RO
    RETROK_DUMMY,        //  90 0x5A KEY_KATAKANA
    RETROK_DUMMY,        //  91 0x5B KEY_HIRAGANA
    RETROK_DUMMY,        //  92 0x5C KEY_HENKAN
    RETROK_DUMMY,        //  93 0x5D KEY_KATAKANAHIRAGANA
    RETROK_DUMMY,        //  94 0x5E KEY_MUHENKAN
    RETROK_DUMMY,        //  95 0x5F KEY_KPJPCOMMA
    RETROK_KP_ENTER,     //  96 0x60 KEY_KPENTER
    RETROK_RCTRL,        //  97 0x61 KEY_RIGHTCTRL
    RETROK_KP_DIVIDE,    //  98 0x62 KEY_KPSLASH
    RETROK_SYSREQ,       //  99 0x63 KEY_SYSRQ
    RETROK_RALT,         // 100 0x64 KEY_RIGHTALT
    RETROK_DUMMY,        // 101 0x65 KEY_LINEFEED
    RETROK_HOME,         // 102 0x66 KEY_HOME
    RETROK_UP,           // 103 0x67 KEY_UP
    RETROK_PAGEUP,       // 104 0x68 KEY_PAGEUP
    RETROK_LEFT,         // 105 0x69 KEY_LEFT
    RETROK_RIGHT,        // 106 0x6A KEY_RIGHT
    RETROK_END,          // 107 0x6B KEY_END
    RETROK_DOWN,         // 108 0x6C KEY_DOWN
    RETROK_PAGEDOWN,     // 109 0x6D KEY_PAGEDOWN
    RETROK_INSERT,       // 110 0x6E KEY_INSERT
    RETROK_DELETE,       // 111 0x6F KEY_DELETE
    RETROK_DUMMY,        // 112 0x70 KEY_MACRO
    RETROK_DUMMY,        // 113 0x71 KEY_MUTE
    RETROK_DUMMY,        // 114 0x72 KEY_VOLUMEDOWN
    RETROK_DUMMY,        // 115 0x73 KEY_VOLUMEUP
    RETROK_DUMMY,        // 116 0x74 KEY_POWER
    RETROK_KP_EQUALS,    // 117 0x75 KEY_KPEQUAL
    RETROK_DUMMY,        // 118 0x76 KEY_KPPLUSMINUS
    RETROK_PAUSE,        // 119 0x77 KEY_PAUSE
    RETROK_DUMMY,        // 120 0x78 KEY_SCALE
    RETROK_DUMMY,        // 121 0x79 KEY_KPCOMMA
    RETROK_DUMMY,        // 122 0x7A KEY_HANGEUL
    RETROK_DUMMY,        // 123 0x7B KEY_HANJA
    RETROK_DUMMY,        // 124 0x7C KEY_YEN
    RETROK_DUMMY,        // 125 0x7D KEY_LEFTMETA
    RETROK_DUMMY,        // 126 0x7E KEY_RIGHTMETA
    RETROK_DUMMY,        // 127 0x7F KEY_COMPOSE
    RETROK_DUMMY,        // 128 0x80 KEY_STOP
    RETROK_DUMMY,        // 129 0x81 KEY_AGAIN
    RETROK_DUMMY,        // 130 0x82 KEY_PROPS
    RETROK_DUMMY,        // 131 0x83 KEY_UNDO
    RETROK_DUMMY,        // 132 0x84 KEY_FRONT
    RETROK_DUMMY,        // 133 0x85 KEY_COPY
    RETROK_DUMMY,        // 134 0x86 KEY_OPEN
    RETROK_DUMMY,        // 135 0x87 KEY_PASTE
    RETROK_DUMMY,        // 136 0x88 KEY_FIND
    RETROK_DUMMY,        // 137 0x89 KEY_CUT
    RETROK_DUMMY,        // 138 0x8A KEY_HELP
    RETROK_DUMMY,        // 139 0x8B KEY_MENU
    RETROK_DUMMY,        // 140 0x8C KEY_CALC
    RETROK_DUMMY,        // 141 0x8D KEY_SETUP
    RETROK_DUMMY,        // 142 0x8E KEY_SLEEP
    RETROK_DUMMY,        // 143 0x8F KEY_WAKEUP
    RETROK_DUMMY,        // 144 0x90 KEY_FILE
    RETROK_DUMMY,        // 145 0x91 KEY_SENDFILE
    RETROK_DUMMY,        // 146 0x92 KEY_DELETEFILE
    RETROK_DUMMY,        // 147 0x93 KEY_XFER
    RETROK_DUMMY,        // 148 0x94 KEY_PROG1
    RETROK_DUMMY,        // 149 0x95 KEY_PROG2
    RETROK_DUMMY,        // 150 0x96 KEY_WWW
    RETROK_DUMMY,        // 151 0x97 KEY_MSDOS
    RETROK_DUMMY,        // 152 0x98 KEY_COFFEE
    RETROK_DUMMY,        // 153 0x99 KEY_ROTATE_DISPLAY
    RETROK_DUMMY,        // 154 0x9A KEY_CYCLEWINDOWS
    RETROK_DUMMY,        // 155 0x9B KEY_MAIL
    RETROK_DUMMY,        // 156 0x9C KEY_BOOKMARKS
    RETROK_DUMMY,        // 157 0x9D KEY_COMPUTER
    RETROK_DUMMY,        // 158 0x9E KEY_BACK
    RETROK_DUMMY,        // 159 0x9F KEY_FORWARD
    RETROK_DUMMY,        // 160 0xA0 KEY_CLOSECD
    RETROK_DUMMY,        // 161 0xA1 KEY_EJECTCD
    RETROK_DUMMY,        // 162 0xA2 KEY_EJECTCLOSECD
    RETROK_DUMMY,        // 163 0xA3 KEY_NEXTSONG
    RETROK_DUMMY,        // 164 0xA4 KEY_PLAYPAUSE
    RETROK_DUMMY,        // 165 0xA5 KEY_PREVIOUSSONG
    RETROK_DUMMY,        // 166 0xA6 KEY_STOPCD
    RETROK_DUMMY,        // 167 0xA7 KEY_RECORD
    RETROK_DUMMY,        // 168 0xA8 KEY_REWIND
    RETROK_DUMMY,        // 169 0xA9 KEY_PHONE
    RETROK_DUMMY,        // 170 0xAA KEY_ISO
    RETROK_DUMMY,        // 171 0xAB KEY_CONFIG
    RETROK_DUMMY,        // 172 0xAC KEY_HOMEPAGE
    RETROK_DUMMY,        // 173 0xAD KEY_REFRESH
    RETROK_DUMMY,        // 174 0xAE KEY_EXIT
    RETROK_DUMMY,        // 175 0xAF KEY_MOVE
    RETROK_DUMMY,        // 176 0xB0 KEY_EDIT
    RETROK_DUMMY,        // 177 0xB1 KEY_SCROLLUP
    RETROK_DUMMY,        // 178 0xB2 KEY_SCROLLDOWN
    RETROK_DUMMY,        // 179 0xB3 KEY_KPLEFTPAREN
    RETROK_DUMMY,        // 180 0xB4 KEY_KPRIGHTPAREN
    RETROK_DUMMY,        // 181 0xB5 KEY_NEW
    RETROK_DUMMY,        // 182 0xB6 KEY_REDO
    RETROK_DUMMY,        // 183 0xB7 KEY_F13
    RETROK_DUMMY,        // 184 0xB8 KEY_F14
    RETROK_DUMMY,        // 185 0xB9 KEY_F15
    RETROK_DUMMY,        // 186 0xBA KEY_F16
    RETROK_DUMMY,        // 187 0xBB KEY_F17
    RETROK_DUMMY,        // 188 0xBC KEY_F18
    RETROK_DUMMY,        // 189 0xBD KEY_F19
    RETROK_DUMMY,        // 190 0xBE KEY_F20
    RETROK_DUMMY,        // 191 0xBF KEY_F21
    RETROK_DUMMY,        // 192 0xC0 KEY_F22
    RETROK_DUMMY,        // 193 0xC1 KEY_F23
    RETROK_DUMMY,        // 194 0xC2 KEY_F24
    RETROK_DUMMY,        // 195 0xC3
    RETROK_DUMMY,        // 196 0xC4
    RETROK_DUMMY,        // 197 0xC5
    RETROK_DUMMY,        // 198 0xC6
    RETROK_DUMMY,        // 199 0xC7
    RETROK_DUMMY,        // 200 0xC8 KEY_PLAYCD
    RETROK_DUMMY,        // 201 0xC9 KEY_PAUSECD
    RETROK_DUMMY,        // 202 0xCA KEY_PROG3
    RETROK_DUMMY,        // 203 0xCB KEY_PROG4
    RETROK_DUMMY,        // 204 0xCC KEY_ALL_APPLICATIONS
    RETROK_DUMMY,        // 205 0xCD KEY_SUSPEND
    RETROK_DUMMY,        // 206 0xCE KEY_CLOSE
    RETROK_DUMMY,        // 207 0xCF KEY_PLAY
    RETROK_DUMMY,        // 208 0xD0 KEY_FASTFORWARD
    RETROK_DUMMY,        // 209 0xD1 KEY_BASSBOOST
    RETROK_DUMMY,        // 210 0xD2 KEY_PRINT
    RETROK_DUMMY,        // 211 0xD3 KEY_HP
    RETROK_DUMMY,        // 212 0xD4 KEY_CAMERA
    RETROK_DUMMY,        // 213 0xD5 KEY_SOUND
    RETROK_DUMMY,        // 214 0xD6 KEY_QUESTION
    RETROK_DUMMY,        // 215 0xD7 KEY_EMAIL
    RETROK_DUMMY,        // 216 0xD8 KEY_CHAT
    RETROK_DUMMY,        // 217 0xD9 KEY_SEARCH
    RETROK_DUMMY,        // 218 0xDA KEY_CONNECT
    RETROK_DUMMY,        // 219 0xDB KEY_FINANCE
    RETROK_DUMMY,        // 220 0xDC KEY_SPORT
    RETROK_DUMMY,        // 221 0xDD KEY_SHOP
    RETROK_DUMMY,        // 222 0xDE KEY_ALTERASE
    RETROK_DUMMY,        // 223 0xDF KEY_CANCEL
    RETROK_DUMMY,        // 224 0xE0 KEY_BRIGHTNESSDOWN
    RETROK_DUMMY,        // 225 0xE1 KEY_BRIGHTNESSUP
    RETROK_DUMMY,        // 226 0xE2 KEY_MEDIA
    RETROK_DUMMY,        // 227 0xE3 KEY_SWITCHVIDEOMODE
    RETROK_DUMMY,        // 228 0xE4 KEY_KBDILLUMTOGGLE
    RETROK_DUMMY,        // 229 0xE5 KEY_KBDILLUMDOWN
    RETROK_DUMMY,        // 230 0xE6 KEY_KBDILLUMUP
    RETROK_DUMMY,        // 231 0xE7 KEY_SEND
    RETROK_DUMMY,        // 232 0xE8 KEY_REPLY
    RETROK_DUMMY,        // 233 0xE9 KEY_FORWARDMAIL
    RETROK_DUMMY,        // 234 0xEA KEY_SAVE
    RETROK_DUMMY,        // 235 0xEB KEY_DOCUMENTS
    RETROK_DUMMY,        // 236 0xEC KEY_BATTERY
    RETROK_DUMMY,        // 237 0xED KEY_BLUETOOTH
    RETROK_DUMMY,        // 238 0xEE KEY_WLAN
    RETROK_DUMMY,        // 239 0xEF KEY_UWB
    RETROK_DUMMY,        // 240 0xF0 KEY_UNKNOWN
    RETROK_DUMMY,        // 241 0xF1 KEY_VIDEO_NEXT
    RETROK_DUMMY,        // 242 0xF2 KEY_VIDEO_PREV
    RETROK_DUMMY,        // 243 0xF3 KEY_BRIGHTNESS_CYCLE
    RETROK_DUMMY,        // 244 0xF4 KEY_BRIGHTNESS_AUTO
    RETROK_DUMMY,        // 245 0xF5 KEY_DISPLAY_OFF
    RETROK_DUMMY,        // 246 0xF6 KEY_WWAN
    RETROK_DUMMY,        // 247 0xF7 KEY_RFKILL
    RETROK_DUMMY,        // 248 0xF8
    RETROK_DUMMY,        // 249 0xF9
    RETROK_DUMMY,        // 250 0xFA
    RETROK_DUMMY,        // 251 0xFB
    RETROK_DUMMY,        // 252 0xFC
    RETROK_DUMMY,        // 253 0xFD
    RETROK_DUMMY,        // 254 0xFE
    RETROK_DUMMY,        // 255 0xFF
};
const int keycode_map_count = sizeof(keycode_retro_map) / sizeof(keycode_retro_map[0]);

extern ArgsOptions args;
extern retro_keyboard_event_t keyboard_event_callback;

struct JoypadState {
    bool input_ids[LAST_BUTTON_ID + 1];
};

struct JoypadDevice {
    SDL_Joystick *joystick;
    unsigned short emulated_buttons[EMULATED_BUTTON_COUNT];
    char guid[33];
    const char *name;
};

struct MouseState {
    short x_delta;
    short y_delta;
    bool left_button;
    bool middle_button;
    bool right_button;
};

struct KeyboardState {
    bool retro_keycode_states[RETROK_LAST];
};

static void setup_joypad(struct JoypadDevice *device, SDL_Joystick *joystick);
static void init_joypads();
static void deinit_joypads();
static void poll_joypads();
static short get_joypad_state(unsigned int port, unsigned int id);

static void init_mouse();
static void deinit_mouse();
static void poll_mouse();
static short get_mouse_state(unsigned int port, unsigned int id);

static void init_keyboard();
static void deinit_keyboard();
static void poll_keyboard();
static unsigned short get_keyboard_state(unsigned int port, unsigned int id);

static const char* find_device(const char *type);

extern struct retro_input_descriptor *input_descriptors;

static struct JoypadState joypad_states[MAX_DEVICES] = { 0 };
static struct JoypadDevice joypad_devices[MAX_DEVICES] = { 0 };
static struct MouseState mouse_state = { 0 };
static struct KeyboardState keyboard_state = { 0 };
static double press_us = 0;
static double release_us = 0;
static unsigned int keycode = 0;
static bool mouse_initialized = false;
static bool keyboard_initialized = false;
static int mouse_device_fd = -1;
static int keyboard_device_fd = -1;

void input_init()
{
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	SDL_JoystickEventState(SDL_IGNORE);
}

void input_clean_up()
{
    deinit_joypads();
    deinit_keyboard();
    deinit_mouse();
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

void input_poll()
{
    poll_joypads();
    poll_keyboard();
    poll_mouse();
}

int16_t callback_input_state(unsigned int port, unsigned int device, unsigned int index, unsigned int id)
{
    unsigned short ret = 0;
    switch (device) {
        case RETRO_DEVICE_JOYPAD:
            ret = get_joypad_state(port, id);
            break;
        case RETRO_DEVICE_KEYBOARD:
            ret = get_keyboard_state(port, id);
            break;
        case RETRO_DEVICE_MOUSE:
            ret = get_mouse_state(port, id);
            break;
    }
    return ret;
}

bool input_parse_deferred_keypress(const char *spec, DeferredKeypress *out)
{
    if (strlen(spec) >= 512) {
        return false;
    }

    char buffer[512];
    strncpy(buffer, spec, sizeof(buffer) - 1);
    char *token = strtok(buffer, ":");
    if (!token) {
        log_e(LOG_TAG, "Missing delay\n");
        return false;
    }
    out->delay_ms = (unsigned long)(atof(token) * 1000);

    token = strtok(NULL, ":");
    if (!token) {
        log_e(LOG_TAG, "Missing keycode\n");
        return false;
    }
    out->keycode = atoi(token);
    if (out->keycode <= 0) {
        log_e(LOG_TAG, "Invalid keycode: %d\n", out->keycode);
        return false;
    }

    token = strtok(NULL, ":");
    if (!token) {
        log_e(LOG_TAG, "Missing duration\n");
        return false;
    }
    out->duration_ms = (unsigned long)(atof(token) * 1000);

    if (out->duration_ms == 0) {
        log_e(LOG_TAG, "Duration must be greater than 0\n");
        return false;
    }

    return true;
}

void input_schedule_keypress(const DeferredKeypress *keypress)
{
    double now_us = micros();
    press_us = now_us + keypress->delay_ms * 1000;
    release_us = press_us + keypress->duration_ms * 1000;
    keycode = keypress->keycode;
    log_v(LOG_TAG, "Scheduled autopress of keycode %d in %lu ms for duration %lu ms\n",
        keycode, keypress->delay_ms, keypress->duration_ms);
}

static void setup_joypad(struct JoypadDevice *device, SDL_Joystick *joystick)
{
    device->joystick = joystick;
    SDL_JoystickGetGUIDString(
        SDL_JoystickGetGUID(device->joystick),
        device->guid,
        sizeof(device->guid));
    device->name = SDL_JoystickName(device->joystick);
    log_i(LOG_TAG, "Setting up %s (GUID %s)\n", device->name, device->guid);

    char buffer[512];
    const char *spec = kvstore_get(&args.input_configs, device->guid);
    if (spec) {
        strncpy(buffer, spec, sizeof(buffer) - 1);
    } else {
        log_i(LOG_TAG, "No config found; will use defaults\n");
        strcpy(buffer, DEFAULT_CONFIG);
    }

    const char *token = strtok(buffer, ",");
    int button_index;
    for (button_index = 0; button_index < EMULATED_BUTTON_COUNT && token; button_index++) {
        if (strcmp(token, "y") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_Y;
        } else if (strcmp(token, "x") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_X;
        } else if (strcmp(token, "l") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_L;
        } else if (strcmp(token, "b") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_B;
        } else if (strcmp(token, "a") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_A;
        } else if (strcmp(token, "r") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_R;
        } else if (strcmp(token, "start") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_START;
        } else if (strcmp(token, "select") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_SELECT;
        } else if (strcmp(token, "l2") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_L2;
        } else if (strcmp(token, "r2") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_R2;
        } else if (strcmp(token, "l3") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_L3;
        } else if (strcmp(token, "r3") == 0) {
            device->emulated_buttons[button_index] = RETRO_DEVICE_ID_JOYPAD_R3;
        } else if (strcmp(token, "_") == 0 || strcmp(token, "none") == 0) {
            device->emulated_buttons[button_index] = 65535;
        } else {
            log_w(LOG_TAG, "Unrecognized button '%s' in input config for '%s'\n",
                token, device->name);
            device->emulated_buttons[button_index] = 65535;
        }
        token = strtok(NULL, ",");
    }
    for (; button_index < EMULATED_BUTTON_COUNT; button_index++) {
        device->emulated_buttons[button_index] = 65535;
    }
}

static void init_joypads()
{
    int joy_count = SDL_NumJoysticks();
    if (joy_count > MAX_DEVICES) {
        joy_count = MAX_DEVICES;
    }

    for (int i = 0; i < joy_count; i++) {
        setup_joypad(&joypad_devices[i], SDL_JoystickOpen(i));
    }
}

static void deinit_joypads()
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        struct JoypadDevice *device = &joypad_devices[i];
        if (device->joystick) {
            SDL_JoystickClose(device->joystick);
            device->joystick = NULL;
            device->name = NULL;
            device->guid[0] = '\0';
            memset(device->emulated_buttons, 0, sizeof(device->emulated_buttons));
        }
    }
}

static void poll_joypads()
{
    SDL_JoystickUpdate();

    int joy_count = SDL_NumJoysticks();
    if (joy_count > MAX_DEVICES) {
        joy_count = MAX_DEVICES;
    }

    static int last_joy_count = 0;
    if (joy_count != last_joy_count) {
        log_i(LOG_TAG, "Joystick count changed (%d=>%d); reinitializing\n",
            last_joy_count, joy_count);
        deinit_joypads();
        init_joypads();
    }

    for (int joy = 0; joy < joy_count; joy++) {
        const struct JoypadDevice *device = &joypad_devices[joy];
        SDL_Joystick *joystick = device->joystick;

        int x = SDL_JoystickGetAxis(joystick, 0);
        int y = SDL_JoystickGetAxis(joystick, 1);

        struct JoypadState *state = &joypad_states[joy];

        state->input_ids[RETRO_DEVICE_ID_JOYPAD_UP] = (y < -JOY_DEADZONE);
        state->input_ids[RETRO_DEVICE_ID_JOYPAD_RIGHT] = (x > JOY_DEADZONE);
        state->input_ids[RETRO_DEVICE_ID_JOYPAD_DOWN] = (y > JOY_DEADZONE);
        state->input_ids[RETRO_DEVICE_ID_JOYPAD_LEFT] = (x < -JOY_DEADZONE);

        for (int i = 0; i < EMULATED_BUTTON_COUNT; i++) {
            unsigned short button_id = device->emulated_buttons[i];
            if (button_id >= 0 && button_id <= LAST_BUTTON_ID) {
                state->input_ids[button_id] = SDL_JoystickGetButton(joystick, i);
            }
        }
    }
    last_joy_count = joy_count;
}

static short get_joypad_state(unsigned int port, unsigned int id)
{
    if (port >= MAX_DEVICES) {
        return 0;
    }

    const struct JoypadState *state = &joypad_states[port];
    unsigned short ret = 0;
    switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_MASK:
            for (int i = 0; i <= LAST_BUTTON_ID; i++) {
                if (state->input_ids[i]) {
                    ret |= (1 << i);
                }
            }
            break;
        default:
            if (id <= LAST_BUTTON_ID) {
                ret = state->input_ids[id];
            }
            break;
    }
    return ret;
}

static void init_mouse()
{
    if (mouse_device_fd >= 0) {
        log_d(LOG_TAG, "Mouse device already initialized\n");
        return;
    }

    const char *device_path = args.mouse_device_path;
    if (!device_path && !(device_path = find_device("mouse"))) {
        log_e(LOG_TAG, "Mouse device path not specified (and detection failed)\n");
        return;
    }

    // Open the mouse input device in non-blocking mode
    if ((mouse_device_fd = open(device_path, O_RDONLY | O_NONBLOCK)) < 0) {
        log_e(LOG_TAG, "Error opening mouse device '%s': %s\n",
            device_path, strerror(errno));
    } else {
        log_i(LOG_TAG, "Mouse device '%s' opened successfully\n", device_path);
    }
}

static void deinit_mouse()
{
    if (mouse_device_fd >= 0) {
        close(mouse_device_fd);
        mouse_device_fd = -1;
    }
    mouse_initialized = false;
}

static void poll_mouse()
{
    if (!mouse_initialized) {
        init_mouse();
        mouse_initialized = true;
    }

    if (mouse_device_fd < 0) {
        return;
    }

    mouse_state.x_delta = 0;
    mouse_state.y_delta = 0;

    struct input_event event;
    ssize_t bytes_read;
    while ((bytes_read = read(mouse_device_fd, &event, sizeof(event))) == sizeof(event)) {
        switch (event.type) {
            case EV_REL:
                if (event.code == REL_X) {
                    mouse_state.x_delta = (short) event.value;
                } else if (event.code == REL_Y) {
                    mouse_state.y_delta = (short) event.value;
                }
                break;
            case EV_KEY:
                if (event.code == BTN_LEFT) {
                    mouse_state.left_button = event.value;
                } else if (event.code == BTN_RIGHT) {
                    mouse_state.right_button = event.value;
                } else if (event.code == BTN_MIDDLE) {
                    mouse_state.middle_button = event.value;
                }
                break;
            default:
                log_v(LOG_TAG, "Mouse event: type=0x%x code=0x%x value=0x%x\n",
                    event.type, event.code, event.value);
                break;
        }
    }
}

static short get_mouse_state(unsigned int port, unsigned int id)
{
    unsigned short ret = 0;
    switch (id) {
    case RETRO_DEVICE_ID_MOUSE_X:
        ret = mouse_state.x_delta;
        break;
    case RETRO_DEVICE_ID_MOUSE_Y:
        ret = mouse_state.y_delta;
        break;
    case RETRO_DEVICE_ID_MOUSE_LEFT:
        ret = mouse_state.left_button;
        break;
    case RETRO_DEVICE_ID_MOUSE_MIDDLE:
        ret = mouse_state.middle_button;
        break;
    case RETRO_DEVICE_ID_MOUSE_RIGHT:
        ret = mouse_state.right_button;
        break;
    default:
        log_v(LOG_TAG, "callback_input_state(RETRO_DEVICE_MOUSE, ? %d)\n", id);
        break;
    }
    return ret;
}

static void init_keyboard()
{
    if (keyboard_device_fd >= 0) {
        log_d(LOG_TAG, "Keyboard device already initialized\n");
        return;
    }

    const char *device_path = args.keyboard_device_path;
    if (!device_path && !(device_path = find_device("kbd"))) {
        log_e(LOG_TAG, "Keyboard device path not specified (and detection failed)\n");
        return;
    }

    // Open the keyboard input device in non-blocking mode
    if ((keyboard_device_fd = open(device_path, O_RDONLY | O_NONBLOCK)) < 0) {
        log_e(LOG_TAG, "Error opening keyboard device '%s': %s\n",
            device_path, strerror(errno));
    } else {
        log_i(LOG_TAG, "Keyboard device '%s' opened successfully\n", device_path);
    }
}

static void deinit_keyboard()
{
    if (keyboard_device_fd >= 0) {
        close(keyboard_device_fd);
        keyboard_device_fd = -1;
    }
    keyboard_initialized = false;
}

static void poll_keyboard()
{
    const retro_keyboard_event_t callback = keyboard_event_callback;
    if (callback) {
        double now_us = micros();
        if (press_us > 0 && now_us >= press_us) {
            log_d(LOG_TAG, "Autopress: pressing keycode %d\n", keycode);
            callback(true, keycode, 0, 0);
            press_us = 0;
        } else if (release_us > 0 && now_us >= release_us) {
            log_d(LOG_TAG, "Autopress: releasing keycode %d\n", keycode);
            callback(false, keycode, 0, 0);
            release_us = 0;
        }
    }

    if (!keyboard_initialized) {
        init_keyboard();
        keyboard_initialized = true;
    }

    if (keyboard_device_fd < 0) {
        return;
    }

    struct input_event event;
    ssize_t bytes_read;
    while ((bytes_read = read(keyboard_device_fd, &event, sizeof(event))) == sizeof(event)) {
        switch (event.type) {
            case EV_KEY:
                if (event.code < keycode_map_count) {
                    int mapped_code = keycode_retro_map[event.code];
                    if (mapped_code != RETROK_DUMMY) {
                        bool is_down = (event.value != 0);
                        keyboard_state.retro_keycode_states[mapped_code] = is_down;
                        if (callback) {
                            callback(is_down, mapped_code, 0, 0);
                        }
                    } else {
                        log_v(LOG_TAG, "Unmapped keycode %1$d (0x%1$x)\n",
                            event.code);
                    }
                }
                break;
            default:
                log_v(LOG_TAG, "Keyboard event: type=0x%x code=0x%x value=0x%x\n",
                    event.type, event.code, event.value);
                break;
        }
    }
}

static unsigned short get_keyboard_state(unsigned int port, unsigned int id)
{
    unsigned short ret = 0;
    if (id < RETROK_LAST) {
        ret = keyboard_state.retro_keycode_states[id];
    }
    return ret;
}

static const char* find_device(const char *type)
{
    FILE *f = fopen("/proc/bus/input/devices", "r");
    if (!f) {
        log_e(LOG_TAG, "Unable to open /proc/bus/input/devices: %s\n", strerror(errno));
        return NULL;
    }

    const char *match = NULL;
    static char line[1024];
    while (fgets(line, sizeof(line), f) != NULL) {
        if (strstr(line, "H: ") == line && strstr(line, type)) {
            const char *event_name = strstr(line, "event");
            if (event_name) {
                char *end = strpbrk(event_name, " \t\n");
                if (end) {
                    // Truncate at the end of the event name
                    *end = '\0';
                }
                snprintf(line, sizeof(line), "/dev/input/%s", event_name);
                match = line;
                break;
            }
        }
    }
    fclose(f);

    return match;
}
