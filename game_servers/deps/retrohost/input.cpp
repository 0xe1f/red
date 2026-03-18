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

#define MAX_JOYSTICKS  16
#define LAST_BUTTON_ID RETRO_DEVICE_ID_JOYPAD_R3
#define JOY_DEADZONE   0x4000

struct GamepadState {
    bool buttons[LAST_BUTTON_ID + 1];
};

extern struct retro_input_descriptor *input_descriptors;

static struct GamepadState input_states[MAX_JOYSTICKS] = { 0 };
static SDL_Joystick *joysticks[MAX_JOYSTICKS] = { NULL };
static int joystick_count = 0;

void input_init()
{
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);

    joystick_count = SDL_NumJoysticks();
    if (joystick_count > MAX_JOYSTICKS) {
        joystick_count = MAX_JOYSTICKS;
    }

    for (int i = 0; i < joystick_count; i++) {
		joysticks[i] = SDL_JoystickOpen(i);
	}
	SDL_JoystickEventState(SDL_IGNORE);
}

void input_poll()
{
    SDL_JoystickUpdate();

    for (int joy = 0; joy < joystick_count; joy++) {
        SDL_Joystick *joystick = joysticks[joy];

        int x = SDL_JoystickGetAxis(joystick, 0);
        int y = SDL_JoystickGetAxis(joystick, 1);

        struct GamepadState *state = &input_states[joy];

        state->buttons[RETRO_DEVICE_ID_JOYPAD_UP] = (y < -JOY_DEADZONE);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_RIGHT] = (x > JOY_DEADZONE);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_DOWN] = (y > JOY_DEADZONE);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_LEFT] = (x < -JOY_DEADZONE);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_A] = SDL_JoystickGetButton(joystick, 0);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_B] = SDL_JoystickGetButton(joystick, 1);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_X] = SDL_JoystickGetButton(joystick, 2);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_Y] = SDL_JoystickGetButton(joystick, 3);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_L] = SDL_JoystickGetButton(joystick, 4);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_R] = SDL_JoystickGetButton(joystick, 5);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_START] = SDL_JoystickGetButton(joystick, 6);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_SELECT] = SDL_JoystickGetButton(joystick, 7);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_L2] = SDL_JoystickGetButton(joystick, 8);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_R2] = SDL_JoystickGetButton(joystick, 9);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_L3] = SDL_JoystickGetButton(joystick, 10);
        state->buttons[RETRO_DEVICE_ID_JOYPAD_R3] = SDL_JoystickGetButton(joystick, 11);
    }
}

int16_t callback_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    if (port >= MAX_JOYSTICKS) {
        return 0;
    }

    const struct GamepadState *state = &input_states[port];
    unsigned short ret = 0;

    switch (device) {
        case RETRO_DEVICE_JOYPAD:
            switch (id) {
                case RETRO_DEVICE_ID_JOYPAD_MASK:
                    for (int i = 0; i <= LAST_BUTTON_ID; i++) {
                        if (state->buttons[i]) {
                            ret |= (1 << i);
                        }
                    }
                    break;
                default:
                    if (id <= LAST_BUTTON_ID) {
                        ret = state->buttons[id];
                    }
                    break;
            }
            break;
    }
    return ret;
}

void input_clean_up()
{
    for (int i = 0; i < joystick_count; i++) {
        if (joysticks[i]) {
            SDL_JoystickClose(joysticks[i]);
            joysticks[i] = NULL;
        }
    }
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}
