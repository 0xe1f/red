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
    unsigned long millis;
};

struct PinState pins[] = {
    { 2, LOW, 0, 0UL, }, // landscape pin
    { 3, LOW, 0, 0UL, }, // portrait pin
    { -1, },
};

#define DEBOUNCE_DELAY_MS 50

#define ORIENT_NONE      -1
#define ORIENT_LANDSCAPE 0 // same as pin index
#define ORIENT_PORTRAIT  1 // same as pin index
 
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
        if (reading != p->previous) {
            p->millis = millis();
        }
        if ((millis() - p->millis) > DEBOUNCE_DELAY_MS) {
            p->isClosed = !reading;
        }
        p->previous = reading;
    }
}

static int getOrientation()
{
    int i = 0;
    for (struct PinState *p = pins; p->pin != -1; p++, i++) {
        if (p->isClosed) {
            return i;
        }
    }
    return ORIENT_NONE;
}

void loop()
{
    scanPins();

    int orientation = getOrientation();
    static int prevOrientation = ORIENT_NONE;
    if (orientation != prevOrientation) {
        Serial.printf("Orientation changed to %s\n",
            orientation == ORIENT_PORTRAIT ? "PORTRAIT" :
            orientation == ORIENT_LANDSCAPE ? "LANDSCAPE" : "NONE");
        prevOrientation = orientation;
    }
    digitalWrite(LED_BUILTIN, orientation != ORIENT_NONE ? HIGH : LOW);
}
