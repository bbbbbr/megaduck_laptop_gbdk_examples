#include <gbdk/platform.h>
#include <stdint.h>
#include <stdbool.h>


#include "megaduck_model.h"


uint8_t megaduck_model = MEGADUCK_HANDHELD_STANDARD;


#define MEGADUCK_MODEL_TILE_ADDR_CHECK 0x8D00u  // First tile at 0x8D00, Second tile at 0x8D10

// 2 consecutive Tiles:
// * Spanish model: Upside-down black Question Mark and Exclamation Point
// * German  model: 2 pixel tall black Underscore and Inverted 0 on dark grey background
//
// Note: It may be sufficient to just check 1 tile
const uint8_t model_spanish_tiles[32] = {
    0x00u, 0x00u, 0x18u, 0x18u, 0x00u, 0x00u, 0x38u, 0x38u, 0x70u, 0x70u, 0x72u, 0x72u, 0x76u, 0x76u, 0x3Cu, 0x3Cu, // Upside-down black Question Mark
    0x00u, 0x00u, 0x18u, 0x18u, 0x00u, 0x00u, 0x18u, 0x18u, 0x3Cu, 0x3Cu, 0x3Cu, 0x3Cu, 0x3Cu, 0x3Cu, 0x18u, 0x18u, // Upside-down black Exclamation Point
};

const uint8_t model_german_tiles[32] = {
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xFFu, 0xFFu, 0xFFu, 0xFFu,  // 2 pixel tall black Underscore
    0x00u, 0xFFu, 0x00u, 0xC3u, 0x00u, 0x99u, 0x00u, 0x99u, 0x00u, 0x99u, 0x00u, 0x99u, 0x00u, 0xC3u, 0x00u, 0xFFu,  // Inverted 0 on dark grey background
};

#define MODEL_SPANISH_TILES_SZ (sizeof(model_spanish_tiles) / sizeof(model_spanish_tiles[0]))
#define MODEL_GERMAN_TILES_SZ  (sizeof(model_german_tiles)  / sizeof(model_german_tiles[0]))


static bool buf_cmp_vmem(const uint8_t * p_buf, uint8_t * p_vram, uint8_t cmp_size) {

    while (cmp_size--) {
        if (get_vram_byte(p_vram++) != *p_buf++) {
            return false;
        }
    }
    return true;
}

// This detection only works immediately after a program is
// launched from the cart slot from the MegaDuck laptop System ROM
// main menu. It works by checking the difference in Font VRAM Tile Patterns
// (which aren't cleared before cart launch) between the Spanish and German
// models, which have slightly different character sets.
//
// Disclaimer: It has not been widely tested due to limited hardware availability
void megaduck_laptop_check_model_vram_on_startup(void) {

    megaduck_model = MEGADUCK_HANDHELD_STANDARD; // Default    

    if (buf_cmp_vmem(model_spanish_tiles, (uint8_t *)MEGADUCK_MODEL_TILE_ADDR_CHECK, MODEL_SPANISH_TILES_SZ)) {
        megaduck_model = MEGADUCK_LAPTOP_SPANISH;
        return;
    }

    // Check German
    if (buf_cmp_vmem(model_german_tiles, (uint8_t *)MEGADUCK_MODEL_TILE_ADDR_CHECK, MODEL_SPANISH_TILES_SZ)) {
        megaduck_model = MEGADUCK_LAPTOP_GERMAN;
        return;
    }

}