/*
 * lib/RoasterIcons.h - Multicolor badge icons (Heater / Air / Drum / Cool / Manual / Auto), native 38 x 38 px (tier 1)
 *
 * RGB565A8 (RGB565 + alpha), data in flash (const).
 * Image data is in RoasterIcons_std.h (generated, do not edit by hand).
 *
 * Depends on: lvgl.h.
 * ----------------------------------------------------------------------------------------------------------------------------
 */
#pragma once
#include <lvgl.h>
#include <string.h>
#include "RoasterIcons_std.h"      // 38 px image data (tier 0)

enum UiIconId : uint8_t { UIICON_FLAME = 0, UIICON_FAN, UIICON_DRUM, UIICON_SNOW, UIICON_MANUAL, UIICON_AUTO, UIICON_COUNT };

static lv_image_dsc_t g_uiIconDsc[UIICON_COUNT];     // image descriptors (RAM), built once

static inline void uiIconsInit() {
    static bool done = false;
    if (done) return;
    done = true;
    static const uint8_t *const data[UIICON_COUNT] = { icon_flame_38, icon_fan_38, icon_drum_38, icon_snow_38, icon_manual_38, icon_auto_38 };
    memset(g_uiIconDsc, 0, sizeof(g_uiIconDsc));
    for (int i = 0; i < UIICON_COUNT; i++) {
        lv_image_dsc_t &d = g_uiIconDsc[i];
        d.header.magic  = LV_IMAGE_HEADER_MAGIC;
        d.header.cf     = LV_COLOR_FORMAT_RGB565A8;
        d.header.w      = ICON_STD_PX;
        d.header.h      = ICON_STD_PX;
        d.header.stride = ICON_STD_PX * 2;
        d.data_size     = ICON_STD_PX * ICON_STD_PX * 3;
        d.data          = data[i];
    }
}

static inline const lv_image_dsc_t* uiIconV3(UiIconId id) {
    uiIconsInit();
    return &g_uiIconDsc[id];
}

// Create an icon image (original colors), not clickable (the parent handles touch)
static inline lv_obj_t* uiIconV3Create(lv_obj_t *parent, UiIconId id) {
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, uiIconV3(id));
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

// On = original colors; off = recolored with the same-tone OFF color (offHex), slightly faded
static inline void uiIconV3SetOn(lv_obj_t *img, bool on, uint32_t offHex) {
    if (!img) return;
    lv_obj_set_style_image_recolor(img, lv_color_hex(offHex), LV_PART_MAIN);
    lv_obj_set_style_image_recolor_opa(img, on ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_image_opa(img, on ? LV_OPA_COVER : LV_OPA_70, LV_PART_MAIN);
}
