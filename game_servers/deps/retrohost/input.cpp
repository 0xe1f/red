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

#define MAX_JOYSTICKS 16
#define JOY_DEADZONE  0x4000

struct InputState {
    bool joypad_left;
    bool joypad_right;
    bool joypad_up;
    bool joypad_down;
    bool joypad_a;
    bool joypad_b;
    bool joypad_x;
    bool joypad_y;
    bool joypad_l;
    bool joypad_r;
    bool joypad_start;
    bool joypad_select;
};

extern struct retro_input_descriptor *input_descriptors;

static struct InputState input_states[MAX_JOYSTICKS] = { 0 };
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

        struct InputState *state = &input_states[joy];
		// Directions
        state->joypad_up = (y < -JOY_DEADZONE);
        state->joypad_right = (x > JOY_DEADZONE);
        state->joypad_down = (y > JOY_DEADZONE);
        state->joypad_left = (x < -JOY_DEADZONE);
        state->joypad_a = SDL_JoystickGetButton(joystick, 0);
        state->joypad_b = SDL_JoystickGetButton(joystick, 1);
        state->joypad_x = SDL_JoystickGetButton(joystick, 2);
        state->joypad_y = SDL_JoystickGetButton(joystick, 3);
        state->joypad_l = SDL_JoystickGetButton(joystick, 4);
        state->joypad_r = SDL_JoystickGetButton(joystick, 5);
        state->joypad_start = SDL_JoystickGetButton(joystick, 6);
        state->joypad_select = SDL_JoystickGetButton(joystick, 7);
    }
}

int16_t callback_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    if (port >= MAX_JOYSTICKS) {
        return 0;
    }
    struct InputState *state = &input_states[port];
    if (device == RETRO_DEVICE_JOYPAD) {
        switch (id) {
            case RETRO_DEVICE_ID_JOYPAD_UP:
                return state->joypad_up;
            case RETRO_DEVICE_ID_JOYPAD_RIGHT:
                return state->joypad_right;
            case RETRO_DEVICE_ID_JOYPAD_DOWN:
                return state->joypad_down;
            case RETRO_DEVICE_ID_JOYPAD_LEFT:
                return state->joypad_left;
            case RETRO_DEVICE_ID_JOYPAD_A:
                return state->joypad_a;
            case RETRO_DEVICE_ID_JOYPAD_B:
                return state->joypad_b;
            case RETRO_DEVICE_ID_JOYPAD_X:
                return state->joypad_x;
            case RETRO_DEVICE_ID_JOYPAD_Y:
                return state->joypad_y;
            case RETRO_DEVICE_ID_JOYPAD_L:
                return state->joypad_l;
            case RETRO_DEVICE_ID_JOYPAD_R:
                return state->joypad_r;
            case RETRO_DEVICE_ID_JOYPAD_START:
                return state->joypad_start;
            case RETRO_DEVICE_ID_JOYPAD_SELECT:
                return state->joypad_select;
            default:
                return 0;
        }
    }

    return 0;
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
