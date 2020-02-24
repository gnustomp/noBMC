#include "ArduinoJson.h"

#define POWER_SW        2
#define RESET_SW        3
#define PWR_LED_LO      A0
#define PWR_LED_HI      A1
#define HDD_LED_LO      A2
#define HDD_LED_HI      A3
#define BUFFER_SIZE     32

#define PERIOD          100 // 10 Hz
#define CONTACT_CYCLES  1   // 100 ms
#define HOLD_CYCLES     40  // 4000 ms
#define LOW_THRESHOLD   307 // 1.5 V

static char buffer[BUFFER_SIZE];
static unsigned int buf_start;
static unsigned int buf_end;
static unsigned int buf_chars;
static StaticJsonDocument<BUFFER_SIZE * 2> json;

static int release_cycles;
static int release_button;

void switch_press(int button) {
    digitalWrite(button, HIGH);
    release_button = button;
    release_cycles = CONTACT_CYCLES;
}

void switch_hold(int button) {
    digitalWrite(button, HIGH);
    release_button = button;
    release_cycles = HOLD_CYCLES;
}

bool led_on(int hi, int lo) {
    int hival = analogRead(hi);
    int loval = analogRead(lo);
    int val = hival - loval;

    if (val > LOW_THRESHOLD) {
        return true;
    }

    return false;
}

void setup() {
    pinMode(POWER_SW, OUTPUT);
    pinMode(RESET_SW, OUTPUT);
    pinMode(PWR_LED_HI, INPUT_PULLUP);
    pinMode(PWR_LED_LO, INPUT_PULLUP);
    pinMode(HDD_LED_HI, INPUT_PULLUP);
    pinMode(HDD_LED_LO, INPUT_PULLUP);
    digitalWrite(POWER_SW, LOW);
    digitalWrite(RESET_SW, LOW);

    Serial.begin(115200);
}

void loop() {
    if (release_cycles > 0) {
        release_cycles--;

        if (release_cycles == 0) {
            digitalWrite(release_button, LOW);
        }
    }

    bool pwr = led_on(PWR_LED_HI, PWR_LED_LO);
    bool hdd = led_on(HDD_LED_HI, HDD_LED_LO);
    Serial.print("{\"power\":");
    Serial.print(pwr ? "true" : "false");
    Serial.print(",\"disk\":");
    Serial.print(hdd ? "true" : "false");
    Serial.println("}");

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') {
            char str[BUFFER_SIZE + 1];
            unsigned int dst = 0;
            unsigned int src = 0;
            while (src != buf_end) {
                str[dst] = buffer[src];

                src = (src + 1) % BUFFER_SIZE;
                dst++;
            }
            str[dst] = 0;

            buf_start = 0;
            buf_end = 0;
            buf_chars = 0;

            DeserializationError err = deserializeJson(json, str);
            if (!err) {
                const char *action = json["action"];

                if (strcmp(action, "power") == 0) {
                    switch_press(POWER_SW);
                } else if (strcmp(action, "reset") == 0) {
                    switch_press(RESET_SW);
                } else if (strcmp(action, "forceoff") == 0) {
                    switch_hold(POWER_SW);
                }
            }
        } else if (0x20 <= c && c <= 0x7e) {
            buffer[buf_end] = c;

            buf_end = (buf_end + 1) % BUFFER_SIZE;
            buf_start = (buf_start + 1) % BUFFER_SIZE;
        }
    }

    delay(PERIOD);
}
