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

#define NUMBER_KEYS 88

#define VELOCITY_TD_MAX 17000
#define VELOCITY_TD_MIN 3600

//#define VELOCITY_ENABLED


typedef struct {
  unsigned int row_pin_first;
  unsigned int col_pin_first;
  unsigned int row_pin_second;
  unsigned int col_pin_second;
  uint8_t note_number;  
  uint64_t s_event_time;
  uint8_t pressed;
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
void scan_matrix(void);

void init_key(unsigned int key_index, uint8_t note_number, unsigned int r_pin_f, unsigned int c_pin_f, unsigned int r_pin_s, unsigned int c_pin_s) {
  keys[key_index].row_pin_first = r_pin_f;
  keys[key_index].col_pin_first = c_pin_f;
  keys[key_index].row_pin_second = r_pin_s;
  keys[key_index].col_pin_second = c_pin_s;
  keys[key_index].note_number = note_number;

  
  gpio_init(c_pin_f);
  gpio_set_dir(c_pin_f, GPIO_OUT);
  gpio_set_drive_strength(c_pin_f, GPIO_DRIVE_STRENGTH_12MA);

  gpio_init(c_pin_s);
  gpio_set_dir(c_pin_s, GPIO_OUT);
  gpio_set_drive_strength(c_pin_s, GPIO_DRIVE_STRENGTH_12MA);

  
  gpio_init(r_pin_f);
  gpio_set_dir(r_pin_f, GPIO_IN);
  
  gpio_init(r_pin_s);
  gpio_set_dir(r_pin_s, GPIO_IN);
}

void init_keys(void) {
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
  uart_deinit(uart0);
  uart_deinit(uart1);

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  multicore_launch_core1(usb_core_task);

  init_keys();


  while (1)
  {
    scan_matrix();
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
    tud_task(); // tinyusb device task
    midi_task();
  }
}

uint8_t scan_row_col(unsigned int col, unsigned int row) {
    //turn on column pin
    gpio_put(col, 1);
    //sleep_us(5);
    // Wait For Pin to Stabalise Voltage
    busy_wait_at_least_cycles(120);
    //Test RowPin
    uint8_t state = gpio_get(row);
    gpio_put(col, 0);
    return state;
} 

void scan_matrix(void)
{
  for (int i = 0; i < NUMBER_KEYS; i++) {
    KeyInfo *key = &keys[i];
    uint8_t first_state = scan_row_col(key->col_pin_first, key->row_pin_first);
    uint8_t second_state = scan_row_col(key->col_pin_second,key->row_pin_second);

    if ((first_state || second_state) && key->s_event_time == 0) {
      key->s_event_time = time_us_64();
    #ifndef VELOCITY_ENABLED
      key->pressed = 1;
      MidiNoteMessage note;
      note.data.note = key->note_number;
      note.data.on = 1;
      note.data.time_diff = (uint16_t) VELOCITY_TD_MIN;
      multicore_fifo_push_blocking(note.bits);
    #endif
    }

    if ((second_state && first_state) && !key->pressed) {
      key->pressed = 1;
      uint32_t time_diff = abs(time_us_64() - key->s_event_time);
      time_diff = time_diff > 65534 ? 65534 : time_diff;

      key->s_event_time = time_us_64();

    #ifdef VELOCITY_ENABLED
      MidiNoteMessage note;
      note.data.note = key->note_number;
      note.data.on = 1;
      note.data.time_diff = (uint16_t) time_diff;
      multicore_fifo_push_blocking(note.bits);
    #endif
      
      //printf("Note %u, TimeDiff %u\n", key->note_number, time_diff);
    }
    
    if (!second_state && !first_state && key->pressed && abs(key->s_event_time - time_us_64()) >= 5000) {      
      MidiNoteMessage note;
      note.data.note = key->note_number;
      note.data.on = 0;
      note.data.time_diff = 0;
      multicore_fifo_push_blocking(note.bits);
      
      key->pressed = 0;
      key->s_event_time = 0;
    }


  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
}

//--------------------------------------------------------------------+
// MIDI Task
//--------------------------------------------------------------------+


uint8_t calculateVelocity(uint32_t timeDiff, uint32_t minTime, uint32_t maxTime) {
    // Clamp the time difference to ensure it’s within the min and max bounds
    if (timeDiff < minTime) timeDiff = minTime;
    if (timeDiff > maxTime) timeDiff = maxTime;

    // Calculate the range
    uint32_t timeRange = maxTime - minTime;
    
    uint8_t velocity = (uint8_t)(127 * (maxTime - timeDiff) / (maxTime - minTime));    // Clamp to the maximum MIDI velocity range
    if (velocity > 127) velocity = 127;
    if (velocity < 1) velocity = 1;
    return velocity;
}

void midi_task(void)
{

  uint8_t const cable_num = 0; // MIDI jack associated with USB endpoint
  uint8_t const channel   = 0; // 0 for channel 1

  // The MIDI interface always creates input and output port/jack descriptors
  // regardless of these being used or not. Therefore incoming traffic should be read
  // (possibly just discarded) to avoid the sender blocking in IO
  uint8_t packet[4];
  while ( tud_midi_available() ) tud_midi_packet_read(packet);

  
  if (multicore_fifo_rvalid()) {
    MidiNoteMessage note;
    note.bits = multicore_fifo_pop_blocking();


    uint8_t velocity = note.data.on ? calculateVelocity(note.data.time_diff, VELOCITY_TD_MIN, VELOCITY_TD_MAX) : 0;
    uint8_t note_stream[3] = {
      (note.data.on ? 0x90 : 0x80) | channel,
      note.data.note,
      velocity
    };
    printf("Sending Note %s %u Velocity %u\n", (note.data.on ? "ON" : "OFF"), note.data.note, velocity);
    if (note.data.on) {
      printf("%u TimeDiff For Note %u\n", note.data.time_diff, note.data.note);
    }
    tud_midi_stream_write(cable_num, note_stream, 3);
  }

  uint64_t current_time = time_us_64();

  if (gpio_get(SUSTAIN_PIN) == 0 && Sustain_Pressed == 0 && current_time - last_sustain_time > 25000) {
    Sustain_Pressed = 1;
    last_sustain_time = time_us_64();
    uint8_t cc_stream[3] = {
      0xB0 | channel,
      64,
      127
    };
    printf("Sending CC 64 ON (Sustain)\n");
    tud_midi_stream_write(cable_num, cc_stream, 3);
  }

  if (gpio_get(SUSTAIN_PIN) == 1 && Sustain_Pressed == 1 && current_time - last_sustain_time > 25000) {
    Sustain_Pressed = 0;

    uint8_t cc_stream[3] = {
      0xB0 | channel,
      64,
      0
    };
    printf("Sending CC 64 OFF (Sustin)\n");
    tud_midi_stream_write(cable_num, cc_stream, 3);
  }

}
