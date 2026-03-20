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
#include "args.h"
#include "libretro.h"
#include "input.h"

#define MAX_DEVICES           16
#define LAST_BUTTON_ID        RETRO_DEVICE_ID_JOYPAD_R3
#define EMULATED_BUTTON_COUNT (LAST_BUTTON_ID - 3)
#define JOY_DEADZONE          0x4000
#define DEFAULT_CONFIG        "y,x,l,b,a,r,start,select,l2,r2,l3,r3"

extern ArgsOptions args;

static void setup_joystick(struct InputDevice *device, SDL_Joystick *joystick);
static void init_joysticks();
static void deinit_joysticks();

struct InputDevice {
    SDL_Joystick *joystick;
    unsigned short emulated_buttons[EMULATED_BUTTON_COUNT];
    char guid[33];
    const char *name;
};

struct InputState {
    bool input_ids[LAST_BUTTON_ID + 1];
};

extern struct retro_input_descriptor *input_descriptors;

static struct InputState input_states[MAX_DEVICES] = { 0 };
static struct InputDevice input_devices[MAX_DEVICES] = { 0 };
static int last_joy_count = 0;

void input_init()
{
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	SDL_JoystickEventState(SDL_IGNORE);
}

void input_poll()
{
    SDL_JoystickUpdate();

    int joy_count = SDL_NumJoysticks();
    if (joy_count > MAX_DEVICES) {
        joy_count = MAX_DEVICES;
    }

    if (joy_count != last_joy_count) {
        fprintf(stderr, "input: Joystick count changed (%d=>%d); reinitializing\n",
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

static void setup_joystick(struct InputDevice *device, SDL_Joystick *joystick)
{
    device->joystick = joystick;
    SDL_JoystickGetGUIDString(
        SDL_JoystickGetGUID(device->joystick),
        device->guid,
        sizeof(device->guid));
    device->name = SDL_JoystickName(device->joystick);
    fprintf(stderr, "input: Setting up %s (GUID %s)\n", device->name, device->guid);

    char buffer[512];
    const char *spec = kvstore_get(&args.input_configs, device->guid);
    if (spec) {
        strncpy(buffer, spec, sizeof(buffer) - 1);
    } else {
        fprintf(stderr, "input: No config found; will use defaults\n");
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
            fprintf(stderr, "input: Unrecognized button '%s' in input config for '%s'\n",
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
