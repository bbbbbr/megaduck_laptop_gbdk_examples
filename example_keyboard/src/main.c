#include <gbdk/platform.h>
#include <gbdk/console.h>
#include <gb/isr.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <megaduck_laptop_io.h>
#include <megaduck_model.h>

#include "megaduck_keyboard.h"

bool megaduck_laptop_detected = false;

uint8_t cursor_x, cursor_y;

bool keyboard_read_ok;
bool logging_enabled = false;

// A dashed underscore cursor
const uint8_t cursor_tile[16] = {
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b01010101, 0b01010101,
    0b10101010, 0b10101010,
};

#define SPR_CURSOR 0u

static void update_cursor(int8_t delta_x, int8_t delta_y);
static void log_key_data(void);
static void use_keypress_data(void);
static void main_init(void);



static void main_init(void) {

    // Set up sprite cursor
    set_sprite_data(0,1,cursor_tile);
    set_sprite_tile(SPR_CURSOR,0);
    hide_sprite(SPR_CURSOR);

    SPRITES_8x8;
    SHOW_SPRITES;
    SHOW_BKG;
    printf("Initializing..\n");

    megaduck_laptop_detected = megaduck_laptop_init();
}


// If requested, log some data about the incoming keyboard packet
static void log_key_data(void) {

    printf("*%hx %hx %hx %hx=",
        (uint8_t)megaduck_io_packet_length,
        (uint8_t)megaduck_key_flags,
        (uint8_t)megaduck_key_code,
        (uint8_t)megaduck_io_checksum_calc);

//    printf("*%hx %hx %hx %hx:%hx=%c\n",
//        (uint8_t)megaduck_io_packet_length,
//        (uint8_t)megaduck_key_flags,
//        (uint8_t)megaduck_key_code,
//        (uint8_t)megaduck_io_checksum_calc,
//        (uint8_t)keyboard_read_ok,
//        (char)megaduck_key_pressed);
//
//    putchar(megaduck_key_pressed);
}


// Moves sprite based cursor and changes console.h current text position
static void update_cursor(int8_t delta_x, int8_t delta_y) {
    gotoxy(posx() + delta_x, posy() + delta_y);
    move_sprite(SPR_CURSOR, (posx() * 8) + DEVICE_SPRITE_PX_OFFSET_X, (posy() * 8) + DEVICE_SPRITE_PX_OFFSET_Y);
}


// Example of typing and moving a cursor around the screen with arrow keys
static void use_keypress_data(void) {

    switch (megaduck_key_pressed) {

        case NO_KEY: break;

        case KEY_ARROW_UP:    update_cursor( 0, -1); break;
        case KEY_ARROW_DOWN:  update_cursor( 0,  1); break;
        case KEY_ARROW_LEFT:  update_cursor(-1,  0); break;
        case KEY_ARROW_RIGHT: update_cursor( 1,  0); break;

        case KEY_ESCAPE: logging_enabled = !logging_enabled; break;

        // Clears the screen
        case KEY_HELP:
            cls();
            update_cursor(1, 0);
            break;

        case KEY_ENTER:
            gotoxy(0, posy() + 1);
            update_cursor(1, 0);
            break;

        case KEY_BACKSPACE:
            gotoxy(posx() - 1, posy());
            putchar(' ');
            update_cursor(-1, 0);
            break;

        // All other keys
        default:
            putchar(megaduck_key_pressed);
            update_cursor(0, 0);
            break;
    }
}


void main(void) {
	
    megaduck_laptop_check_model_vram_on_startup();  // This must be called before any vram tiles are loaded

    main_init();

	if (!megaduck_laptop_detected) {
	    printf("Laptop not Detected\n");
	}
	else {
	    printf("Laptop Detected!\n");

        if (megaduck_model == MEGADUCK_LAPTOP_SPANISH)
			printf("Spanish model\n");
        else if (megaduck_model == MEGADUCK_LAPTOP_GERMAN)
            printf("German model\n");

	    update_cursor(1,1);

		while(1) {
		    vsync();

		    // Poll for keys every other frame
		    // (Polling intervals below 20ms may cause keyboard lockup)
		    if (sys_time & 0x01u) {

		        keyboard_read_ok = megaduck_keyboard_poll_keys();

		            if (logging_enabled)
		                log_key_data();

		        if (keyboard_read_ok) {
		            // Convert from keycodes to ascii and apply key repeat
		            megaduck_keyboard_process_keys();

		            use_keypress_data();
		        }
	            if (logging_enabled)
                    putchar('\n');
		    }
		}
	}
}

