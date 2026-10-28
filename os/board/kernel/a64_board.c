// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// The PinePhone's side of the A64 display driver (a64_board.h in the SoC repository): NuttX's PinePhone panel and
// PMIC drivers (nuttx/, Apache-2.0, vendored unmodified) behind the board interface.
#include <stdint.h>
#include <stdbool.h>

int pinephone_pmic_init(void);
int pinephone_lcd_panel_reset(bool val);
int pinephone_lcd_panel_init(void);
int pinephone_lcd_backlight_enable(uint32_t percent);
int pinephone_lcd_backlight_ticks(uint32_t ticks);
int pinephone_lcd_backlight_disable(void);

int a64_board_pmic_init(void) { return pinephone_pmic_init(); }
int a64_board_panel_reset(int high) { return pinephone_lcd_panel_reset(high != 0); }
int a64_board_panel_init(void) { return pinephone_lcd_panel_init(); }
int a64_board_backlight_enable(unsigned percent) { return pinephone_lcd_backlight_enable(percent); }
int a64_board_backlight_ticks(unsigned ticks) { return pinephone_lcd_backlight_ticks(ticks); }
int a64_board_backlight_disable(void) { return pinephone_lcd_backlight_disable(); }
