#include <gbdk/platform.h>
#include <stdint.h>
#include <stdbool.h>

#include "megaduck_laptop_io.h"
#include "megaduck_keyboard.h"
#include "megaduck_key2ascii.h"
#include "megaduck_keycodes.h"


uint8_t int_enable_saved;


uint8_t serial_io_packet_length;
uint8_t serial_key_flags;
uint8_t serial_key_code;
uint8_t serial_io_checksum_calc;

#define REPEAT_OFF                 0u
#define REPEAT_FIRST_THRESHOLD     8u
#define REPEAT_CONTINUE_THRESHOLD  4u

char keyboard_key_pressed        = NO_KEY;
char keyboard_key_previous       = NO_KEY;
bool keyboard_repeat_allowed     = false;
uint8_t keyboard_flags           = 0x00u;
uint8_t keyboard_repeat_timeout  = REPEAT_OFF;


// RX Bytes for Keyboard Serial Reply Packet
// - 1st:
//   -  Always 0x04 (Length)
// - 2nd:
//    - KEY REPEAT : |= 0x01  (so far looks like with no key value set in 3rd Byte)
//    - CAPS_LOCK: |= 0x02
//    - SHIFT: |= 0x04
//    - LEFT_PRINTSCREEN: |= 0x08
// - 3rd:
//    - Carries the keyboard key scan code
//    - 0x00 when no key pressed
// - 4th:
//     - Two's complement checksum byte
//     - It should be: #4 == (((#1 + #2 + #3) XOR 0xFF) + 1) [two's complement]
//     - I.E: (#4 + #1 + #2 + #3) == 0x100 -> unsigned overflow -> 0x00



// Request keyboard input and handle the response
//
// Returns success or failure, resulting key data is in:
//   serial_key_flags & serial_key_code
//
bool megaduck_keyboard_poll_keys(void) {

    if (serial_io_send_command_and_receive_buffer(SYS_CMD_KBD_START)) {
        if (serial_rx_buf_len == SYS_REPLY_KBD_LEN) {
            serial_key_flags = serial_rx_buf[0];
            serial_key_code  = serial_rx_buf[1];
            return true;
        }
    }
    return false;
}


// Translates key codes to ascii
// Handles Shift/Caps Lock and Repeat flags
void megaduck_keyboard_process_keys(void) {

    // Key repeat processing is optional
    if ((serial_key_flags & KEY_FLAG_KEY_REPEAT) && (keyboard_repeat_allowed)) {
        // Default to no key repeat
        keyboard_key_pressed = NO_KEY;

        if (keyboard_repeat_timeout) {
            keyboard_repeat_timeout--;
        } else {
            // Small delay than initial threshold until next repeat
            keyboard_key_pressed = keyboard_key_previous;
            keyboard_repeat_timeout = REPEAT_CONTINUE_THRESHOLD;
        }
    }
    else {
        keyboard_flags = serial_key_flags;
        uint8_t temp_serial_key_code = serial_key_code;

        // If only shift is enabled, use keycode translation for shift alternate keys (-= 0x80u)
        if ((keyboard_flags & (KEY_FLAG_CAPSLOCK | KEY_FLAG_SHIFT)) == KEY_FLAG_SHIFT)
            temp_serial_key_code -= MEGADUCK_KEY_BASE;

        keyboard_key_pressed = megaduck_keycode_to_ascii(temp_serial_key_code);

        // If only caps lock is enabled, just translate a-z -> A-Z with no other shift alternates
        if ((keyboard_flags & (KEY_FLAG_CAPSLOCK | KEY_FLAG_SHIFT)) == KEY_FLAG_CAPSLOCK)
            if ((keyboard_key_pressed >= 'a') && (keyboard_key_pressed <= 'z'))
                keyboard_key_pressed -= ('a' - 'A');

        // Repeat ok for ascii 32 (space) and higher + arrow keys
        if ((keyboard_key_pressed >= ' ') ||
            ((keyboard_key_pressed >= KEY_ARROW_UP) && (keyboard_key_pressed <= KEY_ARROW_LEFT))) {
            keyboard_repeat_allowed = true;
            keyboard_repeat_timeout = REPEAT_FIRST_THRESHOLD;
        } else
            keyboard_repeat_allowed = false;

        // Save key for repeat
        keyboard_key_previous = keyboard_key_pressed;        
    }
}
