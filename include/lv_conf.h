#pragma once

// Shared by the touch-diagnostics PlatformIO target only. Keeping LVGL lean
// leaves headroom for the live motion, radio, and load-cell services that are
// intentionally still present in that target.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (64U * 1024U)
#define LV_USE_LOG 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

#endif // LV_CONF_H
