// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// What the PinePhone gives the A64 drivers (a64_board.h in the SoC repository): the panel, the pins its LEDs,
// vibrator and touch controller are wired to, and the names it shows. A pin is a PIO port number (A = 0, B = 1,
// C = 2, D = 3 ... H = 7) and a pin number in that port; "R" pins are on the separate R_PIO block (port L).
// Wiring as in the Linux device tree sun50i-a64-pinephone.dtsi.
#ifndef A64_BOARD_CONFIG_H
#define A64_BOARD_CONFIG_H

// The panel (Xingbangda XBD599, Sitronix ST7703)
#define A64_PANEL_W 720
#define A64_PANEL_H 1440

// Names
#define A64_BOARD_USB_PRODUCT L"Symbian on PinePhone"	// the USB product string
#define A64_SENSOR_LOCATION "PinePhone"				// the sensors' location string
// the same string, as the bytes of a sensor channel's resource data: its length, then its characters
#define A64_SENSOR_LOCATION_BYTES 0x09, 0x50, 0x69, 0x6E, 0x65, 0x50, 0x68, 0x6F, 0x6E, 0x65

// The RGB LED: three active-high pins
#define A64_LED_GREEN_PORT 3
#define A64_LED_GREEN_PIN 18
#define A64_LED_RED_PORT 3
#define A64_LED_RED_PIN 19
#define A64_LED_BLUE_PORT 3
#define A64_LED_BLUE_PIN 20

// The vibration motor's switch: active high (the motor's supply, DCDC1, is always on)
#define A64_VIBRA_PORT 3
#define A64_VIBRA_PIN 2

// The touch controller: a Goodix GT917S on TWI0 (SCK PH0, SDA PH1, pin function 2), I2C address 0x5D (its interrupt
// line is PH4, not used: the driver polls)
#define A64_TOUCH_TWI_PHYS 0x01C2AC00u
#define A64_TOUCH_TWI_BUS_GATE_BIT 0			// the TWI's bit in the CCU bus-gate and bus-reset registers
#define A64_TOUCH_I2C_ADDR 0x5D
#define A64_TOUCH_SCK_PORT 7
#define A64_TOUCH_SCK_PIN 0
#define A64_TOUCH_SDA_PORT 7
#define A64_TOUCH_SDA_PIN 1
#define A64_TOUCH_PIN_FUNC 2

// The modem (Quectel EG25-G): its pins come with the driver (modem-eg25/port/pinephone_modem_pins.h, from Apache NuttX).

#endif
