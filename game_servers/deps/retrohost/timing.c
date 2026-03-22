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

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "timing.h"

static unsigned int current_fps = 0;

double micros()
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000000.0 + tp.tv_usec;
}

unsigned int timing_fps()
{
    return current_fps;
}

void timing_throttle(double target_fps)
{
    if (target_fps <= 0.01) {
        return;
    }

    double now_ms = micros();
    static unsigned int frame_count = 0;
    frame_count++;
    static double start_ms = 0;
    static double next_frame_ms = 0;
    double target_frame_time_ms = 1000000.0 / target_fps;

    if (next_frame_ms <= 0) {
        next_frame_ms = now_ms + target_frame_time_ms;
    } else {
        if (now_ms < next_frame_ms) {
            usleep((int)(next_frame_ms - now_ms));
            now_ms = micros();
        }

        while (next_frame_ms <= now_ms) {
            next_frame_ms += target_frame_time_ms;
        }
    }
    now_ms = micros();

    // Calculate (and optionally display) FPS every second
    if (now_ms - start_ms >= 1000000.0) {
        current_fps = frame_count;
        frame_count = 0;
        start_ms = now_ms;
    }
}
