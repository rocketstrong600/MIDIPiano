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

// --- TUNING PARAMETERS ---
#define VELOCITY_TD_MIN 3000
#define VELOCITY_TD_MAX 20000
#define VELOCITY_CURVE_EXPONENT 1.8f
#define DEBOUNCE_US 5000
#define VELOCITY_ENABLED

// --- NEW DATA STRUCTURES FOR INTERRUPT HANDLING ---

// We need a way to quickly find a key based on its row pin.
// This struct will hold all keys that share a single row pin.
typedef struct {
    unsigned int row_pin;
    unsigned int key_indices[12]; // Max of 12 columns per row in your design
    unsigned int num_keys_on_row;
} RowMapping;

// An array to hold the mappings for each unique row.
#define NUM_UNIQUE_ROWS 15
RowMapping row_maps[NUM_UNIQUE_ROWS];


typedef enum {
    STATE_RELEASED,
    STATE_PRESSED_FIRST, // We only need two states now, as the interrupt handles the "in-between"
} KeyState;

typedef struct {
    unsigned int row_pin_first;
    unsigned int col_pin_first;
    unsigned int row_pin_second;
    unsigned int col_pin_second;
    uint8_t note_number;
    KeyState state;
    uint64_t first_press_time; // Timestamp for the first sensor press
    volatile uint64_t last_irq_time; // Timestamp for the last interrupt on this key's row, for debouncing
} KeyInfo;

typedef union {
    struct {
        uint8_t note;
        uint8_t on;
        uint16_t time_diff;
    } data;
    uint32_t bits;
} MidiNoteMessage;


KeyInfo keys[NUMBER_KEYS];

#define SUSTAIN_PIN 27
uint8_t Sustain_Pressed;
uint64_t last_sustain_time = 0;


void midi_task(void);
void usb_core_task(void);
// The main scan_matrix function is now replaced by the interrupt handler
void gpio_callback(uint gpio, uint32_t events);


// --- NEW HELPER FUNCTIONS ---

// Helper to find a row map by its pin number
RowMapping* find_row_map(uint pin) {
    for (int i = 0; i < NUM_UNIQUE_ROWS; i++) {
        if (row_maps[i].row_pin == pin) {
            return &row_maps[i];
        }
    }
    return NULL;
}

// Helper to initialize the row mappings
void init_row_maps() {
    for (int i = 0; i < NUM_UNIQUE_ROWS; i++) {
        row_maps[i].row_pin = i; // Assigning pins 0-14 as the row pins
        row_maps[i].num_keys_on_row = 0;
    }
}

void init_key(unsigned int key_index, uint8_t note_number, unsigned int r_pin_f, unsigned int c_pin_f, unsigned int r_pin_s, unsigned int c_pin_s) {
    keys[key_index].row_pin_first = r_pin_f;
    keys[key_index].col_pin_first = c_pin_f;
    keys[key_index].row_pin_second = r_pin_s;
    keys[key_index].col_pin_second = c_pin_s;
    keys[key_index].note_number = note_number;
    keys[key_index].state = STATE_RELEASED;
    keys[key_index].first_press_time = 0;
    keys[key_index].last_irq_time = 0;

    // Add this key's index to the correct row map for fast lookup in the ISR
    RowMapping* map = find_row_map(r_pin_f);
    if (map) {
        map->key_indices[map->num_keys_on_row++] = key_index;
    }

    // Columns are now inputs by default to avoid contention.
    // They will be briefly switched to outputs inside the ISR.
    gpio_init(c_pin_f);
    gpio_set_dir(c_pin_f, GPIO_IN);
    gpio_disable_pulls(c_pin_f);

    gpio_init(c_pin_s);
    gpio_set_dir(c_pin_s, GPIO_IN);
    gpio_disable_pulls(c_pin_s);

    // Rows are inputs with pull-ups. An interrupt is triggered when a key press
    // pulls the row to ground.
    gpio_init(r_pin_f);
    gpio_set_dir(r_pin_f, GPIO_IN);
    gpio_pull_up(r_pin_f);

    gpio_init(r_pin_s);
    gpio_set_dir(r_pin_s, GPIO_IN);
    gpio_pull_up(r_pin_s);
}

void init_keys(void) {
    // Must init maps first!
    init_row_maps();

    init_key(0, 21, 14, 17, 14, 23);
    init_key(1, 22, 14, 18, 14, 24);
    init_key(2, 23, 14, 19, 14, 25);
    init_key(3, 24, 14, 20, 14, 26);
    init_key(4, 25, 13, 15, 13, 21);
    init_key(5, 26, 13, 16, 13, 22);
    init_key(6, 27, 13, 17, 13, 23);
    init_key(7, 28, 13, 18, 13, 24);
    init_key(8, 29, 13, 19, 13, 25);
    init_key(9, 30, 13, 20, 13, 26);
    init_key(10, 31, 12, 15, 12, 21);
    init_key(11, 32, 12, 16, 12, 22);
    init_key(12, 33, 12, 17, 12, 23);
    init_key(13, 34, 12, 18, 12, 24);
    init_key(14, 35, 12, 19, 12, 25);
    init_key(15, 36, 12, 20, 12, 26);
    init_key(16, 37, 11, 15, 11, 21);
    init_key(17, 38, 11, 16, 11, 22);
    init_key(18, 39, 11, 17, 11, 23);
    init_key(19, 40, 11, 18, 11, 24);
    init_key(20, 41, 11, 19, 11, 25);
    init_key(21, 42, 11, 20, 11, 26);
    init_key(22, 43, 10, 15, 10, 21);
    init_key(23, 44, 10, 16, 10, 22);
    init_key(24, 45, 10, 17, 10, 23);
    init_key(25, 46, 10, 18, 10, 24);
    init_key(26, 47, 10, 19, 10, 25);
    init_key(27, 48, 10, 20, 10, 26);
    init_key(28, 49, 9, 15, 9, 21);
    init_key(29, 50, 9, 16, 9, 22);
    init_key(30, 51, 9, 17, 9, 23);
    init_key(31, 52, 9, 18, 9, 24);
    init_key(32, 53, 9, 19, 9, 25);
    init_key(33, 54, 9, 20, 9, 26);
    init_key(34, 55, 8, 15, 8, 21);
    init_key(35, 56, 8, 16, 8, 22);
    init_key(36, 57, 8, 17, 8, 23);
    init_key(37, 58, 8, 18, 8, 24);
    init_key(38, 59, 8, 19, 8, 25);
    init_key(39, 60, 8, 20, 8, 26);
    init_key(40, 61, 7, 15, 7, 21);
    init_key(41, 62, 7, 16, 7, 22);
    init_key(42, 63, 7, 17, 7, 23);
    init_key(43, 64, 7, 18, 7, 24);
    init_key(44, 65, 7, 19, 7, 25);
    init_key(45, 66, 7, 20, 7, 26);
    init_key(46, 67, 6, 15, 6, 21);
    init_key(47, 68, 6, 16, 6, 22);
    init_key(48, 69, 6, 17, 6, 23);
    init_key(49, 70, 6, 18, 6, 24);
    init_key(50, 71, 6, 19, 6, 25);
    init_key(51, 72, 6, 20, 6, 26);
    init_key(52, 73, 5, 15, 5, 21);
    init_key(53, 74, 5, 16, 5, 22);
    init_key(54, 75, 5, 17, 5, 23);
    init_key(55, 76, 5, 18, 5, 24);
    init_key(56, 77, 5, 19, 5, 25);
    init_key(57, 78, 5, 20, 5, 26);
    init_key(58, 79, 4, 15, 4, 21);
    init_key(59, 80, 4, 16, 4, 22);
    init_key(60, 81, 4, 17, 4, 23);
    init_key(61, 82, 4, 18, 4, 24);
    init_key(62, 83, 4, 19, 4, 25);
    init_key(63, 84, 4, 20, 4, 26);
    init_key(64, 85, 3, 15, 3, 21);
    init_key(65, 86, 3, 16, 3, 22);
    init_key(66, 87, 3, 17, 3, 23);
    init_key(67, 88, 3, 18, 3, 24);
    init_key(68, 89, 3, 19, 3, 25);
    init_key(69, 90, 3, 20, 3, 26);
    init_key(70, 91, 2, 15, 2, 21);
    init_key(71, 92, 2, 16, 2, 22);
    init_key(72, 93, 2, 17, 2, 23);
    init_key(73, 94, 2, 18, 2, 24);
    init_key(74, 95, 2, 19, 2, 25);
    init_key(75, 96, 2, 20, 2, 26);
    init_key(76, 97, 1, 15, 1, 21);
    init_key(77, 98, 1, 16, 1, 22);
    init_key(78, 99, 1, 17, 1, 23);
    init_key(79, 100, 1, 18, 1, 24);
    init_key(80, 101, 1, 19, 1, 25);
    init_key(81, 102, 1, 20, 1, 26);
    init_key(82, 103, 0, 15, 0, 21);
    init_key(83, 104, 0, 16, 0, 22);
    init_key(84, 105, 0, 17, 0, 23);
    init_key(85, 106, 0, 18, 0, 24);
    init_key(86, 107, 0, 19, 0, 25);
    init_key(87, 108, 0, 20, 0, 26);
}

/*------------- MAIN -------------*/
int main(void)
{
    stdio_init_all();
    tud_init(BOARD_TUD_RHPORT);
    multicore_launch_core1(usb_core_task);
    init_keys();

    // Set up the interrupt for all row pins.
    // The same callback function will handle all of them.
    gpio_set_irq_enabled_with_callback(0, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    for (int i = 1; i <= 14; i++) {
        gpio_set_irq_enabled(i, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    }

    // The main loop for core 0 is now empty! It will just sleep
    // until an interrupt happens.
    while (1)
    {
        __wfi(); // Wait for Interrupt
    }

    return 0;
}

void usb_core_task(void)
{
    gpio_init(SUSTAIN_PIN);
    gpio_set_dir(SUSTAIN_PIN, GPIO_IN);
    gpio_pull_up(SUSTAIN_PIN);
    while (1)
    {
        tud_task();
        midi_task();
    }
}

// --- THE INTERRUPT SERVICE ROUTINE ---
// This function is the heart of the new design. It runs whenever a key is pressed or released.
// It must be fast and cannot contain any long delays.
void gpio_callback(uint gpio, uint32_t events) {
    uint64_t now = time_us_64();

    // Find all keys associated with the row that triggered the interrupt.
    RowMapping* map = find_row_map(gpio);
    if (!map) return;

    // --- DEBOUNCING ---
    // We use a simple time-based debounce. If an interrupt for this row happened
    // very recently, we ignore this one as it's likely just noise.
    // We check the first key's last_irq_time as a representative for the whole row.
    if (now - keys[map->key_indices[0]].last_irq_time < DEBOUNCE_US) {
        return;
    }
    // Update the debounce timestamp for all keys on this row.
    for (int i = 0; i < map->num_keys_on_row; i++) {
        keys[map->key_indices[i]].last_irq_time = now;
    }


    // --- IDENTIFY WHICH KEY(S) ON THE ROW CHANGED ---
    // To do this, we briefly set each column pin to an output and drive it low,
    // then check if the interrupted row pin is still low.
    for (int i = 0; i < map->num_keys_on_row; i++) {
        KeyInfo* key = &keys[map->key_indices[i]];

        // Check first sensor
        gpio_set_dir(key->col_pin_first, GPIO_OUT);
        gpio_put(key->col_pin_first, 0);
        sleep_us(1); // Allow time for the line to settle
        bool first_pressed = !gpio_get(key->row_pin_first);
        gpio_set_dir(key->col_pin_first, GPIO_IN); // Set back to input

        // Check second sensor
        gpio_set_dir(key->col_pin_second, GPIO_OUT);
        gpio_put(key->col_pin_second, 0);
        sleep_us(1);
        bool second_pressed = !gpio_get(key->row_pin_second);
        gpio_set_dir(key->col_pin_second, GPIO_IN);


        // --- STATE MACHINE LOGIC ---
        if (first_pressed && key->state == STATE_RELEASED) {
            // First sensor was pressed, start timing for velocity.
            key->state = STATE_PRESSED_FIRST;
            key->first_press_time = now;

        } else if (second_pressed && key->state == STATE_PRESSED_FIRST) {
            // Second sensor was pressed, this is a NOTE ON event.
            // We keep the state as PRESSED_FIRST because the release logic
            // depends on both sensors being released.
            uint32_t time_diff = now - key->first_press_time;

            MidiNoteMessage note;
            note.data.note = key->note_number;
            note.data.on = 1;
            note.data.time_diff = (uint16_t)time_diff;
            multicore_fifo_push_blocking(note.bits);

        } else if (!first_pressed && !second_pressed && key->state == STATE_PRESSED_FIRST) {
            // Both sensors are now released, this is a NOTE OFF event.
            key->state = STATE_RELEASED;
            key->first_press_time = 0;

            MidiNoteMessage note;
            note.data.note = key->note_number;
            note.data.on = 0;
            note.data.time_diff = 0;
            multicore_fifo_push_blocking(note.bits);
        }
    }
}


uint8_t calculateVelocity(uint32_t timeDiff) {
    if (timeDiff < VELOCITY_TD_MIN) timeDiff = VELOCITY_TD_MIN;
    if (timeDiff > VELOCITY_TD_MAX) timeDiff = VELOCITY_TD_MAX;

    float normalized_time = (float)(VELOCITY_TD_MAX - timeDiff) / (float)(VELOCITY_TD_MAX - VELOCITY_TD_MIN);
    float curved_value = powf(normalized_time, VELOCITY_CURVE_EXPONENT);
    uint8_t velocity = (uint8_t)(curved_value * 126.0f) + 1;

    if (velocity > 127) velocity = 127;
    if (velocity < 1) velocity = 1;
    return velocity;
}

void midi_task(void)
{
    uint8_t const cable_num = 0;
    uint8_t const channel   = 0;

    uint8_t packet[4];
    while ( tud_midi_available() ) tud_midi_packet_read(packet);

    if (multicore_fifo_rvalid()) {
        MidiNoteMessage note;
        note.bits = multicore_fifo_pop_blocking();

        uint8_t velocity = note.data.on ? calculateVelocity(note.data.time_diff) : 0;
        uint8_t note_stream[3] = {
            (note.data.on ? 0x90 : 0x80) | channel,
            note.data.note,
            velocity
        };
        printf("Sending Note %s %u Velocity %u (TimeDiff: %u us)\n", (note.data.on ? "ON" : "OFF"), note.data.note, velocity, note.data.time_diff);
        tud_midi_stream_write(cable_num, note_stream, 3);
    }

    uint64_t current_time = time_us_64();
    if (gpio_get(SUSTAIN_PIN) == 0 && Sustain_Pressed == 0 && current_time - last_sustain_time > 25000) {
        Sustain_Pressed = 1;
        last_sustain_time = current_time;
        uint8_t cc_stream[3] = { 0xB0 | channel, 64, 127 };
        tud_midi_stream_write(cable_num, cc_stream, 3);
    }

    if (gpio_get(SUSTAIN_PIN) == 1 && Sustain_Pressed == 1 && current_time - last_sustain_time > 25000) {
        Sustain_Pressed = 0;
        last_sustain_time = current_time;
        uint8_t cc_stream[3] = { 0xB0 | channel, 64, 0 };
        tud_midi_stream_write(cable_num, cc_stream, 3);
    }
}
