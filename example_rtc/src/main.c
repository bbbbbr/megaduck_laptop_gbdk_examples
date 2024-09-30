#include <gbdk/platform.h>
#include <gbdk/console.h>
#include <gb/isr.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <megaduck_laptop_io.h>
#include <megaduck_model.h>

#include "megaduck_rtc.h"

bool megaduck_laptop_detected = false;

bool rtc_read_ok;
bool logging_enabled = false;

static void log_rtc_data(void);
static void log_rtc_data(void);
static void main_init(void);



static void main_init(void) {

    SPRITES_8x8;
    SHOW_SPRITES;
    SHOW_BKG;
    printf("Initializing..\n");

    megaduck_laptop_detected = megaduck_laptop_init();
}


// If requested, log some data about the incoming keyboard packet
static void log_rtc_data(void) {

    gotoxy(0,14);
    printf("*%hx %hx %hx %hx - %hx %hx %hx %hx\n",
        (uint8_t)megaduck_rtc_year,
        (uint8_t)megaduck_rtc_mon,
        (uint8_t)megaduck_rtc_day,
        (uint8_t)megaduck_rtc_weekday,

        (uint8_t)megaduck_rtc_ampm,
        (uint8_t)megaduck_rtc_hour,
        (uint8_t)megaduck_rtc_min,
        (uint8_t)megaduck_rtc_sec);
}



// Example of displaying RTC date info
static void use_rtc_data(void) {

    const char * ampm_str[] = {"am", "pm"};
    const char * dow_str[]  = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

    gotoxy(0,6);

    printf("Year:  %d  \n"
           "Month: %d  \n"
           "Day:   %d  \n"
           "DoW:   %s  \n"
           "Time:  %d:%d:%d %s   \n",
        (uint16_t)megaduck_rtc_year,
        (uint16_t)megaduck_rtc_mon,
        (uint16_t)megaduck_rtc_day,
        (uint16_t)dow_str[megaduck_rtc_weekday],

        (uint16_t)megaduck_rtc_hour,
        (uint16_t)megaduck_rtc_min,
        (uint16_t)megaduck_rtc_sec,
        ampm_str[megaduck_rtc_ampm] );
}


void main(void) {
	
    uint8_t gamepad;
    uint8_t gamepad_last = 0x00;
    megaduck_laptop_check_model_vram_on_startup();  // This must be called before any vram tiles are loaded

    main_init();

	if (!megaduck_laptop_detected) {
	    printf("Laptop not Detected\n");
	}
	else {
	    printf("Laptop Detected!\n");

        if (megaduck_model == MEGADUCK_LAPTOP_SPANISH)
			printf("-> Spanish model\n");
        else if (megaduck_model == MEGADUCK_LAPTOP_GERMAN)
            printf("-> German model\n");

        printf("\n*SELECT to Set Time\n to Sys rom default");

		while(1) {
		    vsync();
            gamepad = joypad();

            logging_enabled = (gamepad & (J_A | J_B | J_START));

		    // Poll for RTC every other frame
		    // (Polling intervals below 20ms may cause keyboard lockup)
		    if (sys_time & 0x01u) {

                // Send RTC data to device if SELECT is pressed
                // otherwise Read RTC data
                if (gamepad & J_SELECT) {
                    bool rtc_send_ok = megaduck_send_rtc();
                    gotoxy(0,12);

                    if (rtc_send_ok) {
                        printf("Send RTC: Success");
                    } else {
                        printf("Send RTC: Failed!");
                    }

                    waitpadup();
                }
                else {
		            bool rtc_read_ok = megaduck_poll_rtc();

		                if (logging_enabled)
		                    log_rtc_data();

		            if (rtc_read_ok) {
		                megaduck_keyboard_process_rtc();

		                use_rtc_data();
                    }
		        }
		    }
		}
	}
}

