
#ifndef BUS_PARALLEL_H
#define BUS_PARALLEL_H

#include <I2Cbus.hpp>

#define I2C_ADDRESS   0x20
#define I2C_REGISTER  0X00

/* PCF8575 port bits */
#define P00  1
#define P01  2
#define P02  3
#define P03  4
#define P04  5
#define P05  6
#define P06  7
#define P07  8
#define P10  9
#define P11  10
#define P12  11
#define P13  12
#define P14  13
#define P15  14
#define P16  15
#define P17  16

/* User Port to pin mapping */
// #define FLAG2  P07  // B
// #define CNT1   P06  // 4
// #define SP1    P05  // 5
// #define CNT2   P04  // 6
// #define SP2    P03  // 7
// #define PC2    P02  // 8
// #define ATN    P01  // 9
// #define PA2    P00  // M

// #define PB0    P10   // C
// #define PB1    P11   // D
// #define PB2    P12   // E
// #define PB3    P13   // F
// #define PB4    P14   // H - G
// #define PB5    P15   // J - H
// #define PB6    P16   // K - I
// #define PB7    P17   // L - J

typedef enum {
  FLAG2 = P07,  // B
  CNT1  = P06,  // 4
  SP1   = P05,  // 5
  CNT2  = P04,  // 6
  SP2   = P03,  // 7
  PC2   = P02,  // 8
  ATN   = P01,  // 9
  PA2   = P00,  // M - K

  PB0   = P10,   // C
  PB1   = P11,   // D
  PB2   = P12,   // E
  PB3   = P13,   // F
  PB4   = P14,   // H - G
  PB5   = P15,   // J - H
  PB6   = P16,   // K - I
  PB7   = P17,   // L - J
} user_port_pin_t;

typedef enum {
  MODE_SEND = 0,
  MODE_RECEIVE = 1
} parallel_mode_t;

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

class parallelBus
{
  public:
    void setup();

    void handShake();
    uint8_t readByte();
    void sendByte( uint8_t byte );
    bool status( user_port_pin_t pin );

    uint8_t changed = 0x00;
    uint8_t last = 0xFF;
    uint8_t flags = 255;
    uint8_t data = 0;
    parallel_mode_t mode;

};

void wic64_command();

extern parallelBus PARALLEL;

#endif // BUS_PARALLEL_H