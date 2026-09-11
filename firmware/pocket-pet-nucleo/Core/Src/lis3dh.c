#include "lis3dh.h"
#include "i2c.h"
#include "main.h"

#define LIS3DH_I2C_TIMEOUT   100  /* ms; a stalled bus should fail, not hang */
#define LIS3DH_AUTO_INC      0x80 /* OR into the register address for bursts */

/*
 * Write one register.
 *
 * Returns 1 on success, 0 on failure, so callers can check without dragging
 * HAL_StatusTypeDef through the rest of the codebase.
 */
static uint8_t lis3dh_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };

    return (HAL_I2C_Master_Transmit(&hi2c1, LIS3DH_I2C_ADDR_HAL,
                                    buf, 2, LIS3DH_I2C_TIMEOUT) == HAL_OK);
}

/*
 * Read one or more consecutive registers.
 *
 * Bit 7 of the register address must be set for the part to auto-increment
 * its internal pointer across a burst. Without it, every byte of a multi-byte
 * read returns the same register, which looks exactly like a dead sensor.
 */
static uint8_t lis3dh_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t addr = (len > 1) ? (uint8_t)(reg | LIS3DH_AUTO_INC) : reg;

    if (HAL_I2C_Master_Transmit(&hi2c1, LIS3DH_I2C_ADDR_HAL,
                                &addr, 1, LIS3DH_I2C_TIMEOUT) != HAL_OK) {
        return 0;
    }

    return (HAL_I2C_Master_Receive(&hi2c1, LIS3DH_I2C_ADDR_HAL,
                                   buf, len, LIS3DH_I2C_TIMEOUT) == HAL_OK);
}

/*
 * Bring the part up and confirm it is what we think it is.
 *
 * Returns 1 only if WHO_AM_I reads back the expected value, so a wiring fault
 * or a wrong address fails loudly here rather than producing plausible-looking
 * garbage later.
 */
uint8_t lis3dh_init(void)
{
    uint8_t who = 0;

    if (!lis3dh_read_regs(LIS3DH_WHO_AM_I, &who, 1)) {
        return 0;
    }

    if (who != LIS3DH_WHO_AM_I_VALUE) {
        return 0;
    }

    /* 50 Hz, all three axes on, normal (not low-power) mode. 50 Hz is far
     * more than a shake detector needs, but it costs little and leaves room
     * for tap detection later. */
    if (!lis3dh_write_reg(LIS3DH_CTRL_REG1,
                          LIS3DH_ODR_50HZ | LIS3DH_AXES_ENABLE)) {
        return 0;
    }

    /* +/- 2 g, high resolution, block data update on. BDU stops the part
     * updating a register pair between our read of the low byte and the high
     * byte, which would otherwise stitch halves of two samples together. */
    if (!lis3dh_write_reg(LIS3DH_CTRL_REG4,
                          LIS3DH_SCALE_2G | LIS3DH_HIGH_RES | LIS3DH_BDU)) {
        return 0;
    }

    return 1;
}

/*
 * Read all three axes in one burst.
 *
 * The part is little-endian by default, so the low byte arrives first. Values
 * are left-justified 16-bit; in high-resolution mode only the top 12 bits are
 * real, so each is shifted right by 4.
 *
 * The shift is done on a signed value so it sign-extends: an arithmetic shift
 * preserves negative readings, a logical shift on an unsigned type would turn
 * -1 into 4095.
 */
uint8_t lis3dh_read_accel(lis3dh_accel_t *accel)
{
    uint8_t buf[6];

    if (!lis3dh_read_regs(LIS3DH_OUT_X_L, buf, 6)) {
        return 0;
    }

    accel->x = (int16_t)((int16_t)((buf[1] << 8) | buf[0]) >> 4);
    accel->y = (int16_t)((int16_t)((buf[3] << 8) | buf[2]) >> 4);
    accel->z = (int16_t)((int16_t)((buf[5] << 8) | buf[4]) >> 4);

    return 1;
}
