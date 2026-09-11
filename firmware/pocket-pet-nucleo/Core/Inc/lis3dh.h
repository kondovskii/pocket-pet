#ifndef LIS3DH_H
#define LIS3DH_H

#include <stdint.h>

/*
 * LIS3DH 3-axis accelerometer, I2C mode.
 *
 * Address is 0x19 because the module strapps SA0 high on-board. The 7-bit
 * address is shifted left by one for the HAL, which takes 8-bit addresses.
 */
#define LIS3DH_I2C_ADDR        0x19
#define LIS3DH_I2C_ADDR_HAL    (LIS3DH_I2C_ADDR << 1)

/* --- Registers (datasheet section 8) ------------------------------------- */
#define LIS3DH_WHO_AM_I        0x0F  /* reads 0x33 on a working part         */
#define LIS3DH_WHO_AM_I_VALUE  0x33

#define LIS3DH_CTRL_REG1       0x20  /* data rate, axis enables, low power   */
#define LIS3DH_CTRL_REG2       0x21  /* high-pass filter                     */
#define LIS3DH_CTRL_REG3       0x22  /* INT1 routing                         */
#define LIS3DH_CTRL_REG4       0x23  /* full scale, resolution, endianness   */
#define LIS3DH_CTRL_REG5       0x24  /* FIFO, latching                       */
#define LIS3DH_CTRL_REG6       0x25  /* INT2 routing                         */

#define LIS3DH_STATUS_REG      0x27  /* data-ready and overrun flags         */

#define LIS3DH_OUT_X_L         0x28  /* X low byte; Y and Z follow in order  */

/* --- CTRL_REG1 values ---------------------------------------------------- */
/* Bits [7:4] set the output data rate, bits [2:0] enable Z, Y, X.           */
#define LIS3DH_ODR_50HZ        0x40  /* 0100 in the ODR field                */
#define LIS3DH_AXES_ENABLE     0x07  /* Z, Y and X all on                    */

/* --- CTRL_REG4 values ---------------------------------------------------- */
#define LIS3DH_SCALE_2G        0x00  /* +/- 2 g, the most sensitive range    */
#define LIS3DH_HIGH_RES        0x08  /* 12-bit output instead of 10-bit      */
#define LIS3DH_BDU             0x80  /* block data update, see below         */

/*
 * Raw acceleration sample, one signed value per axis.
 *
 * The part returns 16-bit left-justified values; in high-resolution mode the
 * low 4 bits are meaningless, so these are shifted right by 4 to give a true
 * 12-bit signed reading.
 */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} lis3dh_accel_t;

uint8_t lis3dh_init(void);
uint8_t lis3dh_read_accel(lis3dh_accel_t *accel);

#endif /* LIS3DH_H */
