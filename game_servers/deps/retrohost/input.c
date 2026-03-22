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

extern ArgsOptions args;
extern retro_keyboard_event_t keyboard_event_callback;

struct InputDevice {
    SDL_Joystick *joystick;
    unsigned short emulated_buttons[EMULATED_BUTTON_COUNT];
    char guid[33];
    const char *name;
};

static void setup_joystick(struct InputDevice *device, SDL_Joystick *joystick);
static void init_joysticks();
static void deinit_joysticks();

struct InputState {
    bool input_ids[LAST_BUTTON_ID + 1];
};

extern struct retro_input_descriptor *input_descriptors;

static struct InputState input_states[MAX_DEVICES] = { 0 };
static struct InputDevice input_devices[MAX_DEVICES] = { 0 };
static int last_joy_count = 0;
static double press_us = 0;
static double release_us = 0;
static unsigned int keycode = 0;

void input_init()
{
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	SDL_JoystickEventState(SDL_IGNORE);
}

void input_poll()
{
    double now_us = micros();
    if (press_us > 0 && now_us >= press_us) {
        log_d(LOG_TAG, "Autopress: pressing keycode %d\n", keycode);
        keyboard_event_callback(true, keycode, 0, 0);
        press_us = 0;
    } else if (release_us > 0 && now_us >= release_us) {
        log_d(LOG_TAG, "Autopress: releasing keycode %d\n", keycode);
        keyboard_event_callback(false, keycode, 0, 0);
        release_us = 0;
    }

    SDL_JoystickUpdate();

    int joy_count = SDL_NumJoysticks();
    if (joy_count > MAX_DEVICES) {
        joy_count = MAX_DEVICES;
    }

    if (joy_count != last_joy_count) {
        log_i(LOG_TAG, "Joystick count changed (%d=>%d); reinitializing\n",
            last_joy_count, joy_count);
        deinit_joysticks();
        init_joysticks();
    }

    for (int joy = 0; joy < joy_count; joy++) {
        const struct InputDevice *device = &input_devices[joy];
        SDL_Joystick *joystick = device->joystick;

        int x = SDL_JoystickGetAxis(joystick, 0);
        int y = SDL_JoystickGetAxis(joystick, 1);

        struct InputState *state = &input_states[joy];

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

int16_t callback_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    if (port >= MAX_DEVICES) {
        return 0;
    }

    const struct InputState *state = &input_states[port];
    unsigned short ret = 0;

    switch (device) {
        case RETRO_DEVICE_KEYBOARD:
            // log_d(LOG_TAG, "keycode %d\n", id);
            break;
        case RETRO_DEVICE_JOYPAD:
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
            break;
    }
    return ret;
}

void input_clean_up()
{
    deinit_joysticks();
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
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
    fprintf(stderr, "Scheduled autopress of keycode %d in %lu ms for duration %lu ms\n",
        keycode, keypress->delay_ms, keypress->duration_ms);
}

static void setup_joystick(struct InputDevice *device, SDL_Joystick *joystick)
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

static void init_joysticks()
{
    int joy_count = SDL_NumJoysticks();
    if (joy_count > MAX_DEVICES) {
        joy_count = MAX_DEVICES;
    }

    for (int i = 0; i < joy_count; i++) {
        setup_joystick(&input_devices[i], SDL_JoystickOpen(i));
    }
}

static void deinit_joysticks()
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        struct InputDevice *device = &input_devices[i];
        if (device->joystick) {
            SDL_JoystickClose(device->joystick);
            device->joystick = NULL;
            device->name = NULL;
            device->guid[0] = '\0';
            memset(device->emulated_buttons, 0, sizeof(device->emulated_buttons));
        }
    }
}
