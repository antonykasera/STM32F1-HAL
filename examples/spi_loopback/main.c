/**
 * @file main.c
 * @brief SPI1 loopback: jumper MOSI (PA7) to MISO (PA6), no device needed
 *
 * Every 500 ms: an 8-byte blocking transfer, then a 64-byte DMA transfer, each
 * checked byte for byte against what came back. SCK 562.5 kHz (slow enough for
 * a 24 MS/s analyzer). Prints on USART1 (PA9, 115200), e.g.
 *   blocking ok, dma ok
 * Probe PA4 (CS), PA5 (SCK), PA7 (MOSI) with an SPI decoder, mode 0.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_spi.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

#define PORT GPIOA
#define CS_PIN 4U
#define SCK_PIN 5U
#define MISO_PIN 6U
#define MOSI_PIN 7U
#define UART_TX 9U
#define DMA_LEN 64U

void SystemInit(void) {}

static uint8_t dma_tx[DMA_LEN];
static uint8_t dma_rx[DMA_LEN];

static bool same(const uint8_t *a, const uint8_t *b, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

int main(void) {
  GPIO_Config af_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config in_cfg = {GPIO_MODE_INPUT, GPIO_SPEED_INPUT, GPIO_PULL_NONE};
  GPIO_Config cs_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};
  SPI_Config spi_cfg = {1000000U, SPI_MODE_0, false};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(PORT);
  hal_gpio_pin_init(PORT, UART_TX, &af_cfg);
  hal_gpio_pin_init(PORT, SCK_PIN, &af_cfg);
  hal_gpio_pin_init(PORT, MOSI_PIN, &af_cfg);
  hal_gpio_pin_init(PORT, MISO_PIN, &in_cfg);
  hal_gpio_pin_init(PORT, CS_PIN, &cs_cfg);
  hal_gpio_pin_write(PORT, CS_PIN, true);
  hal_afio_enable();
  hal_uart_init(USART1, &uart_cfg);
  hal_spi_init(SPI1, &spi_cfg);

  uint8_t seed = 0U;

  while (1) {
    uint8_t tx[8];
    uint8_t rx[8] = {0};
    for (uint32_t i = 0U; i < 8U; i++) {
      tx[i] = (uint8_t)(seed + i);
    }

    hal_gpio_pin_write(PORT, CS_PIN, false);
    HAL_Status st = hal_spi_transfer(SPI1, tx, rx, 8U);
    hal_gpio_pin_write(PORT, CS_PIN, true);
    hal_uart_write_string(USART1, ((st == HAL_OK) && same(tx, rx, 8U))
                                      ? "blocking ok, "
                                      : "blocking FAIL, ");

    for (uint32_t i = 0U; i < DMA_LEN; i++) {
      dma_tx[i] = (uint8_t)(seed ^ (i * 7U));
      dma_rx[i] = 0U;
    }
    hal_gpio_pin_write(PORT, CS_PIN, false);
    (void)hal_spi_transfer_dma(SPI1, dma_tx, dma_rx, DMA_LEN);
    while (hal_spi_busy(SPI1)) {
      /* the CPU is free here */
    }
    hal_gpio_pin_write(PORT, CS_PIN, true);
    hal_uart_write_string(USART1, same(dma_tx, dma_rx, DMA_LEN)
                                      ? "dma ok\r\n"
                                      : "dma FAIL\r\n");

    seed++;
    hal_delay_ms(500U);
  }
}
