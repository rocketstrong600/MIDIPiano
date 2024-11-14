#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "tusb.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"

#define NUM_ROWS 15
#define NUM_COLS 12
#define NUM_KEYS 88

//temp pins for key determination
const unsigned int ROW_PINS[NUM_ROWS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
const unsigned int COL_PINS[NUM_COLS] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26};

uint8_t key_pressed[NUM_ROWS][NUM_COLS];
unsigned int key_frequency[NUM_ROWS][NUM_COLS];

uint64_t start_time;

typedef struct {
  uint64_t first_s_time;
  uint64_t second_s_time;
  uint8_t first_pressed;
  uint8_t second_pressed;
} KeyState;

typedef struct {
  unsigned int row_pin_first;
  unsigned int col_pin_first;
  unsigned int row_pin_second;
  unsigned int col_pin_second;
  uint8_t note_number;
  KeyState key_state;
} KeyInfo;

KeyInfo keys[88];


void midi_task(void);
void usb_core_task(void);

void init_key(unsigned int key_index, uint8_t note_number, unsigned int r_pin_f, unsigned int c_pin_f, unsigned int r_pin_s, unsigned int c_pin_s) {
  keys[key_index].row_pin_first = r_pin_f;
  keys[key_index].col_pin_first = c_pin_f;
  keys[key_index].row_pin_second = r_pin_s;
  keys[key_index].col_pin_second = c_pin_s;
  keys[key_index].note_number = note_number;
}

void init_keys(void) {
  init_key(0, 21, 14, 17, 14, 23)
  init_key(1, 22, 14, 18, 14, 24)
  init_key(2, 23, 14, 19, 14, 25)
  init_key(3, 24, 14, 20, 14, 26)
  init_key(4, 25, 13, 15, 13, 21)
  init_key(5, 26, 13, 16, 13, 22)
  init_key(6, 27, 13, 17, 13, 23)
  init_key(7, 28, 13, 18, 13, 24)
  init_key(8, 29, 13, 19, 13, 25)
  init_key(9, 30, 13, 20, 13, 26)
  init_key(10, 31, 12, 15, 12, 21)
  init_key(11, 32, 12, 16, 12, 22)
  init_key(12, 33, 12, 17, 12, 23)
  init_key(13, 34, 12, 18, 12, 24)
  init_key(14, 35, 12, 19, 12, 25)
  init_key(15, 36, 12, 20, 12, 26)
  init_key(16, 37, 11, 15, 11, 21)
  init_key(17, 38, 11, 16, 11, 22)
  init_key(18, 39, 11, 17, 11, 23)
  init_key(19, 40, 11, 18, 11, 24)
  init_key(20, 41, 11, 19, 11, 25)
  init_key(21, 42, 11, 20, 11, 26)
  init_key(22, 43, 10, 15, 10, 21)
  init_key(23, 44, 10, 16, 10, 22)
  init_key(24, 45, 10, 17, 10, 23)
  init_key(25, 46, 10, 18, 10, 24)
  init_key(26, 47, 10, 19, 10, 25)
  init_key(27, 48, 10, 20, 10, 26)
  init_key(28, 49, 9, 15, 9, 21)
  init_key(29, 50, 9, 16, 9, 22)
  init_key(30, 51, 9, 17, 9, 23)
  init_key(31, 52, 9, 18, 9, 24)
  init_key(32, 53, 9, 19, 9, 25)
  init_key(33, 54, 9, 20, 9, 26)
  init_key(34, 55, 8, 15, 8, 21)
  init_key(35, 56, 8, 16, 8, 22)
  init_key(36, 57, 8, 17, 8, 23)
  init_key(37, 58, 8, 18, 8, 24)
  init_key(38, 59, 8, 19, 8, 25)
  init_key(39, 60, 8, 20, 8, 26)
  init_key(40, 61, 7, 15, 7, 21)
  init_key(41, 62, 7, 16, 7, 22)
  init_key(42, 63, 7, 17, 7, 23)
  init_key(43, 64, 7, 18, 7, 24)
  init_key(44, 65, 7, 19, 7, 25)
  init_key(45, 66, 7, 20, 7, 26)
  init_key(46, 67, 6, 15, 6, 21)
  init_key(47, 68, 6, 16, 6, 22)
  init_key(48, 69, 6, 17, 6, 23)
  init_key(49, 70, 6, 18, 6, 24)
  init_key(50, 71, 6, 19, 6, 25)
  init_key(51, 72, 6, 20, 6, 26)
  init_key(52, 73, 5, 15, 5, 21)
  init_key(53, 74, 5, 16, 5, 22)
  init_key(54, 75, 5, 17, 5, 23)
  init_key(55, 76, 5, 18, 5, 24)
  init_key(56, 77, 5, 19, 5, 25)
  init_key(57, 78, 5, 20, 5, 26)
  init_key(58, 79, 4, 15, 4, 21)
  init_key(59, 80, 4, 16, 4, 22)
  init_key(60, 81, 4, 17, 4, 23)
  init_key(61, 82, 4, 18, 4, 24)
  init_key(62, 83, 4, 19, 4, 25)
  init_key(63, 84, 4, 20, 4, 26)
  init_key(64, 85, 3, 15, 3, 21)
  init_key(65, 86, 3, 16, 3, 22)
  init_key(66, 87, 3, 17, 3, 23)
  init_key(67, 88, 3, 18, 3, 24)
  init_key(68, 89, 3, 19, 3, 25)
  init_key(69, 90, 3, 20, 3, 26)
  init_key(70, 91, 2, 15, 2, 21)
  init_key(71, 92, 2, 16, 2, 22)
  init_key(72, 93, 2, 17, 2, 23)
  init_key(73, 94, 2, 18, 2, 24)
  init_key(74, 95, 2, 19, 2, 25)
  init_key(75, 96, 2, 20, 2, 26)
  init_key(76, 97, 1, 15, 1, 21)
  init_key(77, 98, 1, 16, 1, 22)
  init_key(78, 99, 1, 17, 1, 23)
  init_key(79, 100, 1, 18, 1, 24)
  init_key(80, 101, 1, 19, 1, 25)
  init_key(81, 102, 1, 20, 1, 26)
  init_key(82, 103, 0, 15, 0, 21)
  init_key(83, 104, 0, 16, 0, 22)
  init_key(84, 105, 0, 17, 0, 23)
  init_key(85, 106, 0, 18, 0, 24)
  init_key(86, 107, 0, 19, 0, 25)
  init_key(87, 108, 0, 20, 0, 26)
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

  for (int i = 0; i < NUM_COLS; i++) {
    gpio_init(COL_PINS[i]);
    gpio_set_dir(COL_PINS[i], GPIO_OUT);
  }
  
  for (int i = 0; i < NUM_COLS; i++) {
    gpio_init(ROW_PINS[i]);
    gpio_set_dir(ROW_PINS[i], GPIO_IN);
  }

  while (1)
  {
    for (int j = 0; j < NUM_ROWS; j++) {
      for (int i = 0; i < NUM_COLS; i++) {
        gpio_put(COL_PINS[i], 1);
        sleep_us(5);
        if (gpio_get(ROW_PINS[j])) {
          if (key_pressed[j][i] == 0) {
            key_frequency[j][i] += 1;
            printf("ColPin %u RowPin %u Scanned %u\n", COL_PINS[i], ROW_PINS[j], key_frequency[j][i]);
            start_time = to_ms_since_boot(get_absolute_time());
            key_pressed[j][i] = 1;
          }
        } else {
          if ((to_ms_since_boot(get_absolute_time()) - start_time) > 20) {
            key_pressed[j][i] = 0;
          }          
        }
        gpio_put(COL_PINS[i], 0);
      }
    }
  }

  return 0;
}

void usb_core_task(void)
{  
  while (1)
  {
    tud_task(); // tinyusb device task
    midi_task();
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

// Variable that holds the current position in the sequence.
uint32_t note_pos = 0;

// Store example melody as an array of note values
uint8_t note_sequence[] =
{
  74,78,81,86,90,93,98,102,57,61,66,69,73,78,81,85,88,92,97,100,97,92,88,85,81,78,
  74,69,66,62,57,62,66,69,74,78,81,86,90,93,97,102,97,93,90,85,81,78,73,68,64,61,
  56,61,64,68,74,78,81,86,90,93,98,102
};

void midi_task(void)
{
  static uint32_t start_ms = 0;

  uint8_t const cable_num = 0; // MIDI jack associated with USB endpoint
  uint8_t const channel   = 0; // 0 for channel 1

  // The MIDI interface always creates input and output port/jack descriptors
  // regardless of these being used or not. Therefore incoming traffic should be read
  // (possibly just discarded) to avoid the sender blocking in IO
  uint8_t packet[4];
  while ( tud_midi_available() ) tud_midi_packet_read(packet);

  // send note periodically
  if (to_ms_since_boot(get_absolute_time()) - start_ms < 286) return; // not enough time
  start_ms += 286;

  // Previous positions in the note sequence.
  int previous = (int) (note_pos - 1);

  // If we currently are at position 0, set the
  // previous position to the last note in the sequence.
  if (previous < 0) previous = sizeof(note_sequence) - 1;

  // Send Note On for current position at full velocity (127) on channel 1.
  uint8_t note_on[3] = { 0x90 | channel, note_sequence[note_pos], 127 };
  tud_midi_stream_write(cable_num, note_on, 3);
  printf("Sending Note %u\n", note_sequence[note_pos]);

  // Send Note Off for previous note.
  uint8_t note_off[3] = { 0x80 | channel, note_sequence[previous], 0};
  tud_midi_stream_write(cable_num, note_off, 3);

  // Increment position
  note_pos++;

  // If we are at the end of the sequence, start over.
  if (note_pos >= sizeof(note_sequence)) note_pos = 0;
}
