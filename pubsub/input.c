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
#include "pub_args.h"
#include "log.h"
#include "timing.h"
#include "input.h"

#define LOG_TAG "input"

#define MAX_DEVICES           16
#define LAST_BUTTON_ID        RETRO_DEVICE_ID_JOYPAD_R3
#define EMULATED_BUTTON_COUNT (LAST_BUTTON_ID - 3)
#define JOY_DEADZONE          0x4000
#define DEFAULT_CONFIG        "y,x,l,b,a,r,start,select,l2,r2,l3,r3"

typedef struct {
    int code;
    const char *name;
} InputDescriptor;

#define ID_KEY(x) { RETROK_##x, #x }

// Order is important; index corresponds to Linux input event code
static InputDescriptor keycode_retro_map[] = {
    ID_KEY(DUMMY),        //   0 0x00 KEY_RESERVED
    ID_KEY(ESCAPE),       //   1 0x01 KEY_ESC
    ID_KEY(1),            //   2 0x02 KEY_1
    ID_KEY(2),            //   3 0x03 KEY_2
    ID_KEY(3),            //   4 0x04 KEY_3
    ID_KEY(4),            //   5 0x05 KEY_4
    ID_KEY(5),            //   6 0x06 KEY_5
    ID_KEY(6),            //   7 0x07 KEY_6
    ID_KEY(7),            //   8 0x08 KEY_7
    ID_KEY(8),            //   9 0x09 KEY_8
    ID_KEY(9),            //  10 0x0A KEY_9
    ID_KEY(0),            //  11 0x0B KEY_0
    ID_KEY(MINUS),        //  12 0x0C KEY_MINUS
    ID_KEY(EQUALS),       //  13 0x0D KEY_EQUALS
    ID_KEY(BACKSPACE),    //  14 0x0E KEY_BACKSPACE
    ID_KEY(TAB),          //  15 0x0F KEY_TAB
    ID_KEY(q),            //  16 0x10 KEY_Q
    ID_KEY(w),            //  17 0x11 KEY_W
    ID_KEY(e),            //  18 0x12 KEY_E
    ID_KEY(r),            //  19 0x13 KEY_R
    ID_KEY(t),            //  20 0x14 KEY_T
    ID_KEY(y),            //  21 0x15 KEY_Y
    ID_KEY(u),            //  22 0x16 KEY_U
    ID_KEY(i),            //  23 0x17 KEY_I
    ID_KEY(o),            //  24 0x18 KEY_O
    ID_KEY(p),            //  25 0x19 KEY_P
    ID_KEY(LEFTBRACKET),  //  26 0x1A LEFTBRACKET
    ID_KEY(RIGHTBRACKET), //  27 0x1B RIGHTBRACKET
    ID_KEY(RETURN),       //  28 0x1C KEY_ENTER
    ID_KEY(LCTRL),        //  29 0x1D KEY_LEFTCTRL
    ID_KEY(a),            //  30 0x1E KEY_A
    ID_KEY(s),            //  31 0x1F KEY_S
    ID_KEY(d),            //  32 0x20 KEY_D
    ID_KEY(f),            //  33 0x21 KEY_F
    ID_KEY(g),            //  34 0x22 KEY_G
    ID_KEY(h),            //  35 0x23 KEY_H
    ID_KEY(j),            //  36 0x24 KEY_J
    ID_KEY(k),            //  37 0x25 KEY_K
    ID_KEY(l),            //  38 0x26 KEY_L
    ID_KEY(SEMICOLON),    //  39 0x27 KEY_SEMICOLON
    ID_KEY(QUOTE),        //  40 0x28 KEY_APOSTROPHE
    ID_KEY(BACKQUOTE),    //  41 0x29 KEY_GRAVE
    ID_KEY(LSHIFT),       //  42 0x2A KEY_LEFTSHIFT
    ID_KEY(BACKSLASH),    //  43 0x2B KEY_BACKSLASH
    ID_KEY(z),            //  44 0x2C KEY_Z
    ID_KEY(x),            //  45 0x2D KEY_X
    ID_KEY(c),            //  46 0x2E KEY_C
    ID_KEY(v),            //  47 0x2F KEY_V
    ID_KEY(b),            //  48 0x30 KEY_B
    ID_KEY(n),            //  49 0x31 KEY_N
    ID_KEY(m),            //  50 0x32 KEY_M
    ID_KEY(COMMA),        //  51 0x33 KEY_COMMA
    ID_KEY(PERIOD),       //  52 0x34 KEY_DOT
    ID_KEY(SLASH),        //  53 0x35 KEY_SLASH
    ID_KEY(RSHIFT),       //  54 0x36 KEY_RIGHTSHIFT
    ID_KEY(KP_MULTIPLY),  //  55 0x37 KEY_KPASTERISK
    ID_KEY(LALT),         //  56 0x38 KEY_LEFTALT
    ID_KEY(SPACE),        //  57 0x39 KEY_SPACE
    ID_KEY(CAPSLOCK),     //  58 0x3A KEY_CAPSLOCK
    ID_KEY(F1),           //  59 0x3B KEY_F1
    ID_KEY(F2),           //  60 0x3C KEY_F2
    ID_KEY(F3),           //  61 0x3D KEY_F3
    ID_KEY(F4),           //  62 0x3E KEY_F4
    ID_KEY(F5),           //  63 0x3F KEY_F5
    ID_KEY(F6),           //  64 0x40 KEY_F6
    ID_KEY(F7),           //  65 0x41 KEY_F7
    ID_KEY(F8),           //  66 0x42 KEY_F8
    ID_KEY(F9),           //  67 0x43 KEY_F9
    ID_KEY(F10),          //  68 0x44 KEY_F10
    ID_KEY(NUMLOCK),      //  69 0x45 KEY_NUMLOCK
    ID_KEY(SCROLLOCK),    //  70 0x46 KEY_SCROLLLOCK
    ID_KEY(KP7),          //  71 0x47 KEY_KP7
    ID_KEY(KP8),          //  72 0x48 KEY_KP8
    ID_KEY(KP9),          //  73 0x49 KEY_KP9
    ID_KEY(KP_MINUS),     //  74 0x4A KEY_KPMINUS
    ID_KEY(KP4),          //  75 0x4B KEY_KP4
    ID_KEY(KP5),          //  76 0x4C KEY_KP5
    ID_KEY(KP6),          //  77 0x4D KEY_KP6
    ID_KEY(KP_PLUS),      //  78 0x4E KEY_KPPLUS
    ID_KEY(KP1),          //  79 0x4F KEY_KP1
    ID_KEY(KP2),          //  80 0x50 KEY_KP2
    ID_KEY(KP3),          //  81 0x51 KEY_KP3
    ID_KEY(KP0),          //  82 0x52 KEY_KP0
    ID_KEY(KP_PERIOD),    //  83 0x53 KEY_KPDOT
    ID_KEY(DUMMY),        //  84 0x54
    ID_KEY(DUMMY),        //  85 0x55 KEY_ZENKAKUHANKAKU
    ID_KEY(DUMMY),        //  86 0x56 KEY_102ND
    ID_KEY(F11),          //  87 0x57 KEY_F11
    ID_KEY(F12),          //  88 0x58 KEY_F12
    ID_KEY(DUMMY),        //  89 0x59 KEY_RO
    ID_KEY(DUMMY),        //  90 0x5A KEY_KATAKANA
    ID_KEY(DUMMY),        //  91 0x5B KEY_HIRAGANA
    ID_KEY(DUMMY),        //  92 0x5C KEY_HENKAN
    ID_KEY(DUMMY),        //  93 0x5D KEY_KATAKANAHIRAGANA
    ID_KEY(DUMMY),        //  94 0x5E KEY_MUHENKAN
    ID_KEY(DUMMY),        //  95 0x5F KEY_KPJPCOMMA
    ID_KEY(KP_ENTER),     //  96 0x60 KEY_KPENTER
    ID_KEY(RCTRL),        //  97 0x61 KEY_RIGHTCTRL
    ID_KEY(KP_DIVIDE),    //  98 0x62 KEY_KPSLASH
    ID_KEY(SYSREQ),       //  99 0x63 KEY_SYSRQ
    ID_KEY(RALT),         // 100 0x64 KEY_RIGHTALT
    ID_KEY(DUMMY),        // 101 0x65 KEY_LINEFEED
    ID_KEY(HOME),         // 102 0x66 KEY_HOME
    ID_KEY(UP),           // 103 0x67 KEY_UP
    ID_KEY(PAGEUP),       // 104 0x68 KEY_PAGEUP
    ID_KEY(LEFT),         // 105 0x69 KEY_LEFT
    ID_KEY(RIGHT),        // 106 0x6A KEY_RIGHT
    ID_KEY(END),          // 107 0x6B KEY_END
    ID_KEY(DOWN),         // 108 0x6C KEY_DOWN
    ID_KEY(PAGEDOWN),     // 109 0x6D KEY_PAGEDOWN
    ID_KEY(INSERT),       // 110 0x6E KEY_INSERT
    ID_KEY(DELETE),       // 111 0x6F KEY_DELETE
    ID_KEY(DUMMY),        // 112 0x70 KEY_MACRO
    ID_KEY(DUMMY),        // 113 0x71 KEY_MUTE
    ID_KEY(DUMMY),        // 114 0x72 KEY_VOLUMEDOWN
    ID_KEY(DUMMY),        // 115 0x73 KEY_VOLUMEUP
    ID_KEY(DUMMY),        // 116 0x74 KEY_POWER
    ID_KEY(KP_EQUALS),    // 117 0x75 KEY_KPEQUAL
    ID_KEY(DUMMY),        // 118 0x76 KEY_KPPLUSMINUS
    ID_KEY(PAUSE),        // 119 0x77 KEY_PAUSE
    ID_KEY(DUMMY),        // 120 0x78 KEY_SCALE
    ID_KEY(DUMMY),        // 121 0x79 KEY_KPCOMMA
    ID_KEY(DUMMY),        // 122 0x7A KEY_HANGEUL
    ID_KEY(DUMMY),        // 123 0x7B KEY_HANJA
    ID_KEY(DUMMY),        // 124 0x7C KEY_YEN
    ID_KEY(DUMMY),        // 125 0x7D KEY_LEFTMETA
    ID_KEY(DUMMY),        // 126 0x7E KEY_RIGHTMETA
    ID_KEY(DUMMY),        // 127 0x7F KEY_COMPOSE
    ID_KEY(DUMMY),        // 128 0x80 KEY_STOP
    ID_KEY(DUMMY),        // 129 0x81 KEY_AGAIN
    ID_KEY(DUMMY),        // 130 0x82 KEY_PROPS
    ID_KEY(DUMMY),        // 131 0x83 KEY_UNDO
    ID_KEY(DUMMY),        // 132 0x84 KEY_FRONT
    ID_KEY(DUMMY),        // 133 0x85 KEY_COPY
    ID_KEY(DUMMY),        // 134 0x86 KEY_OPEN
    ID_KEY(DUMMY),        // 135 0x87 KEY_PASTE
    ID_KEY(DUMMY),        // 136 0x88 KEY_FIND
    ID_KEY(DUMMY),        // 137 0x89 KEY_CUT
    ID_KEY(DUMMY),        // 138 0x8A KEY_HELP
    ID_KEY(DUMMY),        // 139 0x8B KEY_MENU
    ID_KEY(DUMMY),        // 140 0x8C KEY_CALC
    ID_KEY(DUMMY),        // 141 0x8D KEY_SETUP
    ID_KEY(DUMMY),        // 142 0x8E KEY_SLEEP
    ID_KEY(DUMMY),        // 143 0x8F KEY_WAKEUP
    ID_KEY(DUMMY),        // 144 0x90 KEY_FILE
    ID_KEY(DUMMY),        // 145 0x91 KEY_SENDFILE
    ID_KEY(DUMMY),        // 146 0x92 KEY_DELETEFILE
    ID_KEY(DUMMY),        // 147 0x93 KEY_XFER
    ID_KEY(DUMMY),        // 148 0x94 KEY_PROG1
    ID_KEY(DUMMY),        // 149 0x95 KEY_PROG2
    ID_KEY(DUMMY),        // 150 0x96 KEY_WWW
    ID_KEY(DUMMY),        // 151 0x97 KEY_MSDOS
    ID_KEY(DUMMY),        // 152 0x98 KEY_COFFEE
    ID_KEY(DUMMY),        // 153 0x99 KEY_ROTATE_DISPLAY
    ID_KEY(DUMMY),        // 154 0x9A KEY_CYCLEWINDOWS
    ID_KEY(DUMMY),        // 155 0x9B KEY_MAIL
    ID_KEY(DUMMY),        // 156 0x9C KEY_BOOKMARKS
    ID_KEY(DUMMY),        // 157 0x9D KEY_COMPUTER
    ID_KEY(DUMMY),        // 158 0x9E KEY_BACK
    ID_KEY(DUMMY),        // 159 0x9F KEY_FORWARD
    ID_KEY(DUMMY),        // 160 0xA0 KEY_CLOSECD
    ID_KEY(DUMMY),        // 161 0xA1 KEY_EJECTCD
    ID_KEY(DUMMY),        // 162 0xA2 KEY_EJECTCLOSECD
    ID_KEY(DUMMY),        // 163 0xA3 KEY_NEXTSONG
    ID_KEY(DUMMY),        // 164 0xA4 KEY_PLAYPAUSE
    ID_KEY(DUMMY),        // 165 0xA5 KEY_PREVIOUSSONG
    ID_KEY(DUMMY),        // 166 0xA6 KEY_STOPCD
    ID_KEY(DUMMY),        // 167 0xA7 KEY_RECORD
    ID_KEY(DUMMY),        // 168 0xA8 KEY_REWIND
    ID_KEY(DUMMY),        // 169 0xA9 KEY_PHONE
    ID_KEY(DUMMY),        // 170 0xAA KEY_ISO
    ID_KEY(DUMMY),        // 171 0xAB KEY_CONFIG
    ID_KEY(DUMMY),        // 172 0xAC KEY_HOMEPAGE
    ID_KEY(DUMMY),        // 173 0xAD KEY_REFRESH
    ID_KEY(DUMMY),        // 174 0xAE KEY_EXIT
    ID_KEY(DUMMY),        // 175 0xAF KEY_MOVE
    ID_KEY(DUMMY),        // 176 0xB0 KEY_EDIT
    ID_KEY(DUMMY),        // 177 0xB1 KEY_SCROLLUP
    ID_KEY(DUMMY),        // 178 0xB2 KEY_SCROLLDOWN
    ID_KEY(DUMMY),        // 179 0xB3 KEY_KPLEFTPAREN
    ID_KEY(DUMMY),        // 180 0xB4 KEY_KPRIGHTPAREN
    ID_KEY(DUMMY),        // 181 0xB5 KEY_NEW
    ID_KEY(DUMMY),        // 182 0xB6 KEY_REDO
    ID_KEY(DUMMY),        // 183 0xB7 KEY_F13
    ID_KEY(DUMMY),        // 184 0xB8 KEY_F14
    ID_KEY(DUMMY),        // 185 0xB9 KEY_F15
    ID_KEY(DUMMY),        // 186 0xBA KEY_F16
    ID_KEY(DUMMY),        // 187 0xBB KEY_F17
    ID_KEY(DUMMY),        // 188 0xBC KEY_F18
    ID_KEY(DUMMY),        // 189 0xBD KEY_F19
    ID_KEY(DUMMY),        // 190 0xBE KEY_F20
    ID_KEY(DUMMY),        // 191 0xBF KEY_F21
    ID_KEY(DUMMY),        // 192 0xC0 KEY_F22
    ID_KEY(DUMMY),        // 193 0xC1 KEY_F23
    ID_KEY(DUMMY),        // 194 0xC2 KEY_F24
    ID_KEY(DUMMY),        // 195 0xC3
    ID_KEY(DUMMY),        // 196 0xC4
    ID_KEY(DUMMY),        // 197 0xC5
    ID_KEY(DUMMY),        // 198 0xC6
    ID_KEY(DUMMY),        // 199 0xC7
    ID_KEY(DUMMY),        // 200 0xC8 KEY_PLAYCD
    ID_KEY(DUMMY),        // 201 0xC9 KEY_PAUSECD
    ID_KEY(DUMMY),        // 202 0xCA KEY_PROG3
    ID_KEY(DUMMY),        // 203 0xCB KEY_PROG4
    ID_KEY(DUMMY),        // 204 0xCC KEY_ALL_APPLICATIONS
    ID_KEY(DUMMY),        // 205 0xCD KEY_SUSPEND
    ID_KEY(DUMMY),        // 206 0xCE KEY_CLOSE
    ID_KEY(DUMMY),        // 207 0xCF KEY_PLAY
    ID_KEY(DUMMY),        // 208 0xD0 KEY_FASTFORWARD
    ID_KEY(DUMMY),        // 209 0xD1 KEY_BASSBOOST
    ID_KEY(DUMMY),        // 210 0xD2 KEY_PRINT
    ID_KEY(DUMMY),        // 211 0xD3 KEY_HP
    ID_KEY(DUMMY),        // 212 0xD4 KEY_CAMERA
    ID_KEY(DUMMY),        // 213 0xD5 KEY_SOUND
    ID_KEY(DUMMY),        // 214 0xD6 KEY_QUESTION
    ID_KEY(DUMMY),        // 215 0xD7 KEY_EMAIL
    ID_KEY(DUMMY),        // 216 0xD8 KEY_CHAT
    ID_KEY(DUMMY),        // 217 0xD9 KEY_SEARCH
    ID_KEY(DUMMY),        // 218 0xDA KEY_CONNECT
    ID_KEY(DUMMY),        // 219 0xDB KEY_FINANCE
    ID_KEY(DUMMY),        // 220 0xDC KEY_SPORT
    ID_KEY(DUMMY),        // 221 0xDD KEY_SHOP
    ID_KEY(DUMMY),        // 222 0xDE KEY_ALTERASE
    ID_KEY(DUMMY),        // 223 0xDF KEY_CANCEL
    ID_KEY(DUMMY),        // 224 0xE0 KEY_BRIGHTNESSDOWN
    ID_KEY(DUMMY),        // 225 0xE1 KEY_BRIGHTNESSUP
    ID_KEY(DUMMY),        // 226 0xE2 KEY_MEDIA
    ID_KEY(DUMMY),        // 227 0xE3 KEY_SWITCHVIDEOMODE
    ID_KEY(DUMMY),        // 228 0xE4 KEY_KBDILLUMTOGGLE
    ID_KEY(DUMMY),        // 229 0xE5 KEY_KBDILLUMDOWN
    ID_KEY(DUMMY),        // 230 0xE6 KEY_KBDILLUMUP
    ID_KEY(DUMMY),        // 231 0xE7 KEY_SEND
    ID_KEY(DUMMY),        // 232 0xE8 KEY_REPLY
    ID_KEY(DUMMY),        // 233 0xE9 KEY_FORWARDMAIL
    ID_KEY(DUMMY),        // 234 0xEA KEY_SAVE
    ID_KEY(DUMMY),        // 235 0xEB KEY_DOCUMENTS
    ID_KEY(DUMMY),        // 236 0xEC KEY_BATTERY
    ID_KEY(DUMMY),        // 237 0xED KEY_BLUETOOTH
    ID_KEY(DUMMY),        // 238 0xEE KEY_WLAN
    ID_KEY(DUMMY),        // 239 0xEF KEY_UWB
    ID_KEY(DUMMY),        // 240 0xF0 KEY_UNKNOWN
    ID_KEY(DUMMY),        // 241 0xF1 KEY_VIDEO_NEXT
    ID_KEY(DUMMY),        // 242 0xF2 KEY_VIDEO_PREV
    ID_KEY(DUMMY),        // 243 0xF3 KEY_BRIGHTNESS_CYCLE
    ID_KEY(DUMMY),        // 244 0xF4 KEY_BRIGHTNESS_AUTO
    ID_KEY(DUMMY),        // 245 0xF5 KEY_DISPLAY_OFF
    ID_KEY(DUMMY),        // 246 0xF6 KEY_WWAN
    ID_KEY(DUMMY),        // 247 0xF7 KEY_RFKILL
    ID_KEY(DUMMY),        // 248 0xF8
    ID_KEY(DUMMY),        // 249 0xF9
    ID_KEY(DUMMY),        // 250 0xFA
    ID_KEY(DUMMY),        // 251 0xFB
    ID_KEY(DUMMY),        // 252 0xFC
    ID_KEY(DUMMY),        // 253 0xFD
    ID_KEY(DUMMY),        // 254 0xFE
    ID_KEY(DUMMY),        // 255 0xFF
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

static int compare_input_descriptor_by_name(const void *a, const void *b);
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

    int keycode_map_by_name_count = 0;
    InputDescriptor **keycode_map_by_name = malloc(keycode_map_count * sizeof(InputDescriptor *));
    if (!keycode_map_by_name) {
        log_e(LOG_TAG, "Failed to allocate memory for keycode map: %s\n",
            strerror(errno));
        return false;
    }

    // Build a list of non-dummy entries
    for (int i = 0; i < keycode_map_count; i++) {
        if (keycode_retro_map[i].code != RETROK_DUMMY) {
            InputDescriptor *entry = &keycode_retro_map[i];
            if (entry->code != RETROK_DUMMY) {
                keycode_map_by_name[keycode_map_by_name_count++] = entry;
            }
        }
    }

    // Sort the entries by name
    qsort(
        keycode_map_by_name,
        keycode_map_by_name_count,
        sizeof(InputDescriptor *),
        compare_input_descriptor_by_name);

    char buffer[512];
    strncpy(buffer, spec, sizeof(buffer) - 1);
    char *token = strtok(buffer, ":");
    if (!token) {
        log_e(LOG_TAG, "Missing delay\n");
        free(keycode_map_by_name);
        return false;
    }
    out->delay_ms = (unsigned long)(atof(token) * 1000);

    token = strtok(NULL, ":");
    if (!token) {
        log_e(LOG_TAG, "Missing keycode\n");
        free(keycode_map_by_name);
        return false;
    }

    const InputDescriptor key = { .name = token };
    const InputDescriptor *kp = &key;
    InputDescriptor **entry = bsearch(
        &kp,
        keycode_map_by_name,
        keycode_map_by_name_count,
        sizeof(InputDescriptor *),
        compare_input_descriptor_by_name);

    if (!entry) {
        log_e(LOG_TAG, "Unrecognized keycode name '%s'\n", token);
        free(keycode_map_by_name);
        return false;
    }
    out->keycode = (*entry)->code;

    token = strtok(NULL, ":");
    if (!token) {
        log_e(LOG_TAG, "Missing duration\n");
        free(keycode_map_by_name);
        return false;
    }
    out->duration_ms = (unsigned long)(atof(token) * 1000);

    if (out->duration_ms == 0) {
        log_e(LOG_TAG, "Duration must be greater than 0\n");
        free(keycode_map_by_name);
        return false;
    }

    free(keycode_map_by_name);
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

static int compare_input_descriptor_by_name(const void *a, const void *b)
{
    const InputDescriptor *left = *(const InputDescriptor **)a;
    const InputDescriptor *right = *(const InputDescriptor **)b;

    return strcasecmp(left->name, right->name);
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
                    int mapped_code = keycode_retro_map[event.code].code;
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
