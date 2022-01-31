
/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#include <stdlib.h>
#include <xpt2046.h>

void ts_spi_setup(void) {

    gpio_mode_setup(TS_SPI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE,
                    TS_SCL_PIN | TS_CS_PIN | TS_MO_PIN | TS_MI_PIN);
    gpio_set_af(TS_SPI_PORT, GPIO_AF5, TS_SCL_PIN | TS_CS_PIN | TS_MO_PIN | TS_MI_PIN);
    gpio_set_output_options(TS_SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            TS_MO_PIN | TS_SCL_PIN | TS_CS_PIN);

    spi_reset(TS_SPI);

    spi_disable(TS_SPI);
    spi_init_master(TS_SPI,
                    SPI_CR1_BAUDRATE_FPCLK_DIV_128,
                    SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
                    SPI_CR1_CPHA_CLK_TRANSITION_1, SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);

    spi_set_full_duplex_mode(TS_SPI);
    spi_disable_software_slave_management(TS_SPI);
    spi_set_nss_high(TS_SPI);
    spi_set_master_mode(TS_SPI);
    spi_enable_ss_output(TS_SPI);
    spi_disable_crc(TS_SPI);

    spi_enable(TS_SPI);
}



uint16_t ts_get_data16(uint8_t command) {
    spi_xfer(TS_SPI, command);
    uint16_t res1 = spi_xfer(TS_SPI, 0x00);
    uint16_t res2 = spi_xfer(TS_SPI, 0x00);
    return ((res1 << 8) | (res2 && 0xFF)) >> 4;
}


uint16_t ts_get_x_raw(void) {
    int16_t res = 0;
    for (uint8_t i = 0; i < TS_EVAL_COUNT; i++) {
        res += ts_get_data16(TS_COMM_X_DPOS);
    }
    return res / TS_EVAL_COUNT;
}

uint16_t ts_get_y_raw(void) {
    int16_t res = 0;
    for (uint8_t i = 0; i < TS_EVAL_COUNT; i++) {
        res += ts_get_data16(TS_COMM_Y_DPOS);
    }
    return res / TS_EVAL_COUNT;
}

uint16_t ts_get_x(void) {

    uint16_t res = ts_get_x_raw();

    if (res >= TS_X_MAX_EDGE)
        return TS_X_SCREEN_MAX;
    if (res <= TS_X_MIN_EDGE)
        return TS_X_SCREEN_MIN;

    res = (TS_X_SCREEN_MAX * (res - TS_X_MIN_EDGE)) / (TS_X_MAX_EDGE - TS_X_MIN_EDGE);
    return TS_X_SCREEN_MAX - res;

}

uint16_t ts_get_y(void) {

    uint16_t res = ts_get_y_raw();

    if (res >= TS_Y_MAX_EDGE)
        return TS_Y_SCREEN_MIN;
    if (res <= TS_Y_MIN_EDGE)
        return TS_Y_SCREEN_MIN;

    res = (TS_Y_SCREEN_MAX * (res - TS_Y_MIN_EDGE)) / (TS_Y_MAX_EDGE - TS_Y_MIN_EDGE);
    return res;
}

uint16_t ts_get_z1_raw(void) {
    uint16_t res = 0;
    for (uint8_t i = 0; i < TS_EVAL_COUNT; i++) {
        res += ts_get_data16(TS_COMM_Z1_POS);
    }
    return res / TS_EVAL_COUNT;
}

uint16_t ts_get_z2_raw(void) {
    uint16_t res = 0;
    for (uint8_t i = 0; i < TS_EVAL_COUNT; i++) {
        res += ts_get_data16(TS_COMM_Z2_POS);
    }
    return res / TS_EVAL_COUNT;
}

/* EOF */
