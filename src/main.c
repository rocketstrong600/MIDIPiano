#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "tusb.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/irq.h"

#define NUMBER_KEYS 88
#define VELOCITY_TD_MAX 50000
#define VELOCITY_TD_MIN 7000

#define SUSTAIN_PIN 27

typedef enum {
    KEY_IDLE,
    KEY_FIRST_CONTACT,
    KEY_PRESSED
} KeyState;

typedef struct {
    unsigned int row_pin_first;
    unsigned int col_pin_first;
    unsigned int row_pin_second;
    unsigned int col_pin_second;
    uint8_t note_number;
    uint64_t s_event_time;
    KeyState state;
} KeyInfo;

typedef union {
    struct {
        uint8_t note;
        uint8_t on;
        uint16_t time_diff;
    } data;
    uint32_t bits;
} MidiNoteMessage;

static KeyInfo keys[NUMBER_KEYS];
static uint8_t Sustain_Pressed = 0;
static uint64_t last_sustain_time = 0;

static inline uint8_t calculateVelocity(uint32_t timeDiff) {
    if (timeDiff < VELOCITY_TD_MIN) timeDiff = VELOCITY_TD_MIN;
    if (timeDiff > VELOCITY_TD_MAX) timeDiff = VELOCITY_TD_MAX;
    float norm = (float)(VELOCITY_TD_MAX - timeDiff) / (VELOCITY_TD_MAX - VELOCITY_TD_MIN);
    float curved = powf(norm, 0.5f); // velocity curve for more natural feel
    uint8_t vel = (uint8_t)(curved * 127.0f);
    if (vel < 1) vel = 1;
    if (vel > 127) vel = 127;
    return vel;
}

static void send_note_event(uint8_t note, uint8_t on, uint16_t time_diff) {
    MidiNoteMessage msg;
    msg.data.note = note;
    msg.data.on = on;
    msg.data.time_diff = time_diff;
    multicore_fifo_push_blocking(msg.bits);
}

static void gpio_isr_handler(uint gpio, uint32_t events) {
    // Called when a row pin changes
    for (int i = 0; i < NUMBER_KEYS; i++) {
        KeyInfo *key = &keys[i];
        if (gpio == key->row_pin_first || gpio == key->row_pin_second) {
            uint8_t first_state = gpio_get(key->row_pin_first);
            uint8_t second_state = gpio_get(key->row_pin_second);
            uint64_t now = time_us_64();

            switch (key->state) {
                case KEY_IDLE:
                    if (first_state || second_state) {
                        key->state = KEY_FIRST_CONTACT;
                        key->s_event_time = now;
                    }
                    break;
                case KEY_FIRST_CONTACT:
                    if (first_state && second_state) {
                        uint32_t td = (uint32_t)(now - key->s_event_time);
                        send_note_event(key->note_number, 1, td);
                        key->state = KEY_PRESSED;
                    } else if (!first_state && !second_state) {
                        key->state = KEY_IDLE;
                    }
                    break;
                case KEY_PRESSED:
                    if (!first_state && !second_state) {
                        send_note_event(key->note_number, 0, 0);
                        key->state = KEY_IDLE;
                    }
                    break;
            }
        }
    }
}

static void init_key(unsigned int key_index, uint8_t note_number,
                     unsigned int r_pin_f, unsigned int c_pin_f,
                     unsigned int r_pin_s, unsigned int c_pin_s) {
    keys[key_index].row_pin_first = r_pin_f;
    keys[key_index].col_pin_first = c_pin_f;
    keys[key_index].row_pin_second = r_pin_s;
    keys[key_index].col_pin_second = c_pin_s;
    keys[key_index].note_number = note_number;
    keys[key_index].state = KEY_IDLE;

    gpio_init(c_pin_f);
    gpio_set_dir(c_pin_f, GPIO_OUT);
    gpio_put(c_pin_f, 0);

    gpio_init(c_pin_s);
    gpio_set_dir(c_pin_s, GPIO_OUT);
    gpio_put(c_pin_s, 0);

    gpio_init(r_pin_f);
    gpio_set_dir(r_pin_f, GPIO_IN);
    gpio_pull_down(r_pin_f);

    gpio_init(r_pin_s);
    gpio_set_dir(r_pin_s, GPIO_IN);
    gpio_pull_down(r_pin_s);

    gpio_set_irq_enabled_with_callback(r_pin_f, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_isr_handler);
    gpio_set_irq_enabled(r_pin_s, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
}

static void init_keys(void) {
    // reuse your mapping table here
    // e.g., init_key(0, 21, 14, 17, 14, 23); etc.
}

static void midi_task(void) {
    uint8_t const cable_num = 0;
    uint8_t const channel = 0;

    uint8_t packet[4];
    while (tud_midi_available()) tud_midi_packet_read(packet);

    if (multicore_fifo_rvalid()) {
        MidiNoteMessage note;
        note.bits = multicore_fifo_pop_blocking();
        uint8_t vel = note.data.on ? calculateVelocity(note.data.time_diff) : 0;
        uint8_t msg[3] = { (note.data.on ? 0x90 : 0x80) | channel, note.data.note, vel };
        tud_midi_stream_write(cable_num, msg, 3);
    }

    uint64_t now = time_us_64();
    if (gpio_get(SUSTAIN_PIN) == 0 && Sustain_Pressed == 0 && now - last_sustain_time > 25000) {
        Sustain_Pressed = 1;
        last_sustain_time = now;
        uint8_t cc[3] = { 0xB0 | channel, 64, 127 };
        tud_midi_stream_write(cable_num, cc, 3);
    }
    if (gpio_get(SUSTAIN_PIN) == 1 && Sustain_Pressed == 1 && now - last_sustain_time > 25000) {
        Sustain_Pressed = 0;
        last_sustain_time = now;
        uint8_t cc[3] = { 0xB0 | channel, 64, 0 };
        tud_midi_stream_write(cable_num, cc, 3);
    }
}

int main(void) {
    stdio_init_all();
    tud_init(BOARD_TUD_RHPORT);
    multicore_launch_core1(midi_task);

    gpio_init(SUSTAIN_PIN);
    gpio_set_dir(SUSTAIN_PIN, GPIO_IN);
    gpio_pull_up(SUSTAIN_PIN);

    init_keys();
    while (1) tight_loop_contents();
}
