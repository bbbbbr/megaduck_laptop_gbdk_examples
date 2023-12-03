#include <gbdk/platform.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef _MEGADUCK_KEYBOARD_H
#define _MEGADUCK_KEYBOARD_H

#define NO_KEY        0

// TODO: Arrow keys are temporary, for testing, they match the GBDK ibm_fixed font
#define KEY_ARROW_UP    1
#define KEY_ARROW_DOWN  2
#define KEY_ARROW_RIGHT 3
#define KEY_ARROW_LEFT  4
#define KEY_HELP        5


#define KEY_BACKSPACE   8
#define KEY_ENTER       13
#define KEY_ESCAPE      27
#define KEY_DELETE      127


#define KEY_FLAG_KEY_REPEAT              0x01u
#define KEY_FLAG_KEY_REPEAT_BIT          0u
#define KEY_FLAG_CAPSLOCK                0x02u
#define KEY_FLAG_CAPSLOCK_BIT            1u
#define KEY_FLAG_SHIFT                   0x04u
#define KEY_FLAG_SHIFT_BIT               2u
#define KEY_FLAG_PRINTSCREEN_LEFT        0x08u
#define KEY_FLAG_PRINTSCREEN_LEFT_BIT    3u


// Raw key data
extern uint8_t megaduck_io_packet_length;
extern uint8_t megaduck_key_flags;
extern uint8_t megaduck_key_code;
extern uint8_t megaduck_io_checksum_calc;


// Post-Processed key data
extern char    megaduck_key_pressed;
extern char    megaduck_key_previous;
extern uint8_t megaduck_key_flags;


bool megaduck_keyboard_poll_keys(void);
void megaduck_keyboard_process_keys(void);


#endif // _MEGADUCK_KEYBOARD_H