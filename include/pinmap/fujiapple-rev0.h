/* FujiApple Rev0 for C64 Hardware Pin Mapping */
#ifndef PINMAP_FUJIAPPLE_REV0_H
#define PINMAP_FUJIAPPLE_REV0_H

#ifdef PINMAP_A2_REV0

/* SD Card */
#define PIN_CARD_DETECT 12 // fnSystem.h
#define PIN_CARD_DETECT_FIX 15 // fnSystem.h

#define PIN_SD_HOST_CS GPIO_NUM_5
#define PIN_SD_HOST_MISO GPIO_NUM_19
#define PIN_SD_HOST_MOSI GPIO_NUM_23
#define PIN_SD_HOST_SCK GPIO_NUM_18

/* UART */
#define PIN_UART0_RX 3 // fnUART.cpp
#define PIN_UART0_TX 1
#define PIN_UART1_RX 9
#define PIN_UART1_TX 10
#define PIN_UART2_RX 33
#define PIN_UART2_TX 21

/* Buttons */
#define PIN_BUTTON_A 0 // keys.cpp
#define PIN_BUTTON_B -1 // No Button B
#define PIN_BUTTON_C 14

/* LEDs */
#define PIN_LED_WIFI 2 // led.cpp
#define PIN_LED_BUS 12 // 4 FN
#define PIN_LED_BT -1 // No BT LED

/* LED Strip NEW */
#define NUM_LEDS 3
#define DATA_PIN_1 27 
//#define DATA_PIN_2 13 // pick unused gpio
#define BRIGHTNESS  25
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB


/* Audio Output */
#define PIN_DAC1 25 // samlib.h

/* Commodore IEC Pins */
// CLK & DATA lines in/out are split between two pins
//#define IEC_SPLIT_LINES

// Line values are inverted (7406 Hex Inverter Buffer)
//#define IEC_INVERTED_LINES

// Reset line is available
#define IEC_HAS_RESET

#define PIN_IEC_RESET       GPIO_NUM_21
#define PIN_IEC_ATN         GPIO_NUM_22
#define PIN_IEC_CLK_IN		GPIO_NUM_33
#define PIN_IEC_CLK_OUT	    GPIO_NUM_33
#define PIN_IEC_DATA_IN    	GPIO_NUM_32
#define PIN_IEC_DATA_OUT   	GPIO_NUM_32
#define PIN_IEC_SRQ			GPIO_NUM_26


/* Modem/Parallel Switch */
#define PIN_MDMPAR_SW1       2  // High = Modem enabled
#define PIN_MDMPAR_SW2       15 // High = UP9600 enabled

/* I2C GPIO Expander */
#define PIN_GPIOX_SDA         GPIO_NUM_21
#define PIN_GPIOX_SCL         GPIO_NUM_22
#define PIN_GPIOX_INT         GPIO_NUM_34
//#define GPIOX_ADDRESS     0x20  // PCF8575
#define GPIOX_ADDRESS     0x24  // PCA9673
//#define GPIOX_SPEED       400   // PCF8575 - 400Khz
#define GPIOX_SPEED       1000  // PCA9673 - 1000Khz / 1Mhz

#endif // PINMAP_A2_REV0
#endif // PINMAP_FUJIAPPLE_REV0_H