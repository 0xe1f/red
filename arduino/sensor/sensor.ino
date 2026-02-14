/**
// Copyright (C) 2025 Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
**/

struct PinState {
    int pin;
    int previous;
    int isClosed;
    int orientation;
    unsigned long millis;
};

#define ORIENT_UNKNOWN   0
#define ORIENT_LANDSCAPE 1
#define ORIENT_PORTRAIT  2

struct PinState pins[] = {
    { 2, LOW, 0, ORIENT_LANDSCAPE, 0UL },
    { 11, LOW, 0, ORIENT_PORTRAIT, 0UL },
    { -1, },
};

#define DEBOUNCE_DELAY_MS 50
#define LOOP_DELAY_MS (1000/60)

void setup()
{
    for (struct PinState *p = pins; p->pin != -1; p++) {
        pinMode(p->pin, INPUT);
        digitalWrite(p->pin, HIGH);
    }
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(9600);
}

static void scanPins()
{
    for (struct PinState *p = pins; p->pin != -1; p++) {
        int reading = digitalRead(p->pin);
        unsigned long now = millis();
        if (reading != p->previous) {
            p->millis = now;
        }
        if ((now - p->millis) > DEBOUNCE_DELAY_MS) {
            p->isClosed = !reading;
        }
        p->previous = reading;
    }
}

static int getOrientation()
{
    for (struct PinState *p = pins; p->pin != -1; p++) {
        if (p->isClosed) {
            return p->orientation;
        }
    }
    return ORIENT_UNKNOWN;
}

static const char* handleRequest(const char *req)
{
    static char buffer[512];
    if (strstr(req, "orientation")) {
        int orientation = getOrientation();
        if (orientation == ORIENT_PORTRAIT) {
            snprintf(buffer, sizeof(buffer), "%s:PORTRAIT", req);
        } else if (orientation == ORIENT_LANDSCAPE) {
            snprintf(buffer, sizeof(buffer), "%s:LANDSCAPE", req);
        } else {
            snprintf(buffer, sizeof(buffer), "%s:UNKNOWN", req);
        }
    } else {
        snprintf(buffer, sizeof(buffer), "?");
    }
    return buffer;
}

void loop()
{
    scanPins();

    int orientation = getOrientation();
    digitalWrite(LED_BUILTIN, orientation != ORIENT_UNKNOWN ? HIGH : LOW);

    if (Serial.available() > 0) {
        const char *request = Serial.readStringUntil('\n').c_str();
        const char *response = handleRequest(request);
        Serial.println(response);
    }

    delay(LOOP_DELAY_MS);
}
