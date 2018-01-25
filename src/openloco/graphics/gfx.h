#pragma once

#include "../localisation/stringmgr.h"
#include "../openloco.h"
#include <cstdint>

namespace openloco::gfx
{
#pragma pack(push, 1)

    struct drawpixelinfo_t
    {
        uint8_t* bits;       // 0x00
        int16_t x;           // 0x04
        int16_t y;           // 0x06
        int16_t width;       // 0x08
        int16_t height;      // 0x0A
        int16_t pitch;       // 0x0C note: this is actually (pitch - width)
        uint16_t zoom_level; // 0x0E
    };

    drawpixelinfo_t& screen_dpi();


    struct g1_header_t
    {
        uint32_t num_entries;
        uint32_t total_size;
    };

    struct g1_element32_t
    {
        uint32_t offset;        // 0x00
        int16_t width;          // 0x04
        int16_t height;         // 0x06
        int16_t x_offset;       // 0x08
        int16_t y_offset;       // 0x0A
        uint16_t flags;         // 0x0C
        int16_t unused;         // 0x0E
    };

    // A version that can be 64-bit when ready...
    struct g1_element
    {
        uint8_t * offset = nullptr;
        int16_t width = 0;
        int16_t height = 0;
        int16_t x_offset = 0;
        int16_t y_offset = 0;
        uint16_t flags = 0;
        int16_t unused = 0;

        g1_element(const g1_element32_t &src)
            : offset((uint8_t *)src.offset),
              width(src.width),
              height(src.height),
              x_offset(src.x_offset),
              y_offset(src.y_offset),
              flags(src.flags),
              unused(src.unused)
        {
        }
    };

    #pragma pack(pop)

    drawpixelinfo_t& screen_dpi();

    void load_g1();
    void clear(drawpixelinfo_t& dpi, uint32_t fill);
    void clear_single(drawpixelinfo_t& dpi, uint8_t paletteId);
    void draw_string_494B3F(
        drawpixelinfo_t& dpi,
        int16_t x,
        int16_t y,
        uint8_t colour,
        string_id stringId,
        const void* args);
    void draw_string_494BBF(
        drawpixelinfo_t& dpi,
        int16_t x,
        int16_t y,
        int16_t width,
        uint8_t colour,
        string_id stringId,
        const void* args);
    void invalidate_screen();
    void set_dirty_blocks(int32_t left, int32_t top, int32_t right, int32_t bottom);
}
