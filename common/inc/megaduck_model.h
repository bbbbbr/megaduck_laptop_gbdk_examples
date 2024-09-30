#include <gbdk/platform.h>
#include <stdint.h>

#ifndef _MEGADUCK_MODEL_H
#define _MEGADUCK_MODEL_H

#define MEGADUCK_HANDHELD_STANDARD 0u
#define MEGADUCK_LAPTOP_SPANISH    1u
#define MEGADUCK_LAPTOP_GERMAN     2u

extern uint8_t megaduck_model;

void megaduck_laptop_check_model_vram_on_startup(void);

#endif // _MEGADUCK_MODEL_H
