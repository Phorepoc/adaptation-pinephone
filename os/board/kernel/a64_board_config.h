// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// What the PinePhone gives the A64 drivers (a64_board.h in the SoC repository): the panel, the pins its LEDs,
// vibrator, modem and touch controller are wired to, and the names it shows. A pin is a PIO port number (A = 0, B = 1,
// C = 2, D = 3 ... H = 7) and a pin number in that port; "R" pins are on the separate R_PIO block (port L).
// Wiring as in the Linux device tree sun50i-a64-pinephone.dtsi and the eg25-manager PinePhone 1.2 data.
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

// The modem (Quectel EG25-G): control lines, all through transistors; the levels written are the GPIO levels
#define A64_MODEM_DTR_PORT 1			// PB2: low = awake
#define A64_MODEM_DTR_PIN 2
#define A64_MODEM_PWRKEY_PORT 1			// PB3
#define A64_MODEM_PWRKEY_PIN 3
#define A64_MODEM_RESET_PORT 2			// PC4
#define A64_MODEM_RESET_PIN 4
#define A64_MODEM_APREADY_PORT 7		// PH7: low = host awake
#define A64_MODEM_APREADY_PIN 7
#define A64_MODEM_DISABLE_PORT 7		// PH8 (W_DISABLE#): low = RF enabled
#define A64_MODEM_DISABLE_PIN 8
#define A64_MODEM_STATUS_PORT 7			// PH9: input, low = the modem is on
#define A64_MODEM_STATUS_PIN 9
#define A64_MODEM_VBAT_PIN 7			// PL7 (R_PIO port 0): the modem's supply regulator enable
#define A64_MODEM_UART_TX_PORT 3		// UART3 on PD0 / PD1, pin function 3
#define A64_MODEM_UART_TX_PIN 0
#define A64_MODEM_UART_RX_PORT 3
#define A64_MODEM_UART_RX_PIN 1
#define A64_MODEM_UART_PIN_FUNC 3

#endif
