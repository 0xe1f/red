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
#include "input.h"

#define MAX_DEVICES    16
#define LAST_BUTTON_ID RETRO_DEVICE_ID_JOYPAD_R3
#define EMULATED_BUTTON_COUNT (LAST_BUTTON_ID - 3)
#define JOY_DEADZONE   0x4000

static void setup_joystick(struct InputDevice *device, SDL_Joystick *joystick);

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
static int device_count = 0;

void input_init()
{
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);

    device_count = SDL_NumJoysticks();
    if (device_count > MAX_DEVICES) {
        device_count = MAX_DEVICES;
    }

    for (int i = 0; i < device_count; i++) {
        setup_joystick(&input_devices[i], SDL_JoystickOpen(i));
	}
	SDL_JoystickEventState(SDL_IGNORE);
}

void input_poll()
{
    SDL_JoystickUpdate();

    for (int joy = 0; joy < device_count; joy++) {
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
            state->input_ids[device->emulated_buttons[i]] = SDL_JoystickGetButton(joystick, i);
        }
    }
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
    for (int i = 0; i < device_count; i++) {
        if (input_devices[i].joystick) {
            SDL_JoystickClose(input_devices[i].joystick);
            input_devices[i].joystick = NULL;
        }
    }
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

    device->emulated_buttons[0] = RETRO_DEVICE_ID_JOYPAD_Y;
    device->emulated_buttons[1] = RETRO_DEVICE_ID_JOYPAD_X;
    device->emulated_buttons[2] = RETRO_DEVICE_ID_JOYPAD_L;
    device->emulated_buttons[3] = RETRO_DEVICE_ID_JOYPAD_B;
    device->emulated_buttons[4] = RETRO_DEVICE_ID_JOYPAD_A;
    device->emulated_buttons[5] = RETRO_DEVICE_ID_JOYPAD_R;
    device->emulated_buttons[6] = RETRO_DEVICE_ID_JOYPAD_START;
    device->emulated_buttons[7] = RETRO_DEVICE_ID_JOYPAD_SELECT;
    device->emulated_buttons[8] = RETRO_DEVICE_ID_JOYPAD_L2;
    device->emulated_buttons[9] = RETRO_DEVICE_ID_JOYPAD_R2;
    device->emulated_buttons[10] = RETRO_DEVICE_ID_JOYPAD_L3;
    device->emulated_buttons[11] = RETRO_DEVICE_ID_JOYPAD_R3;
}
