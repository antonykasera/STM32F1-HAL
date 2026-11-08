/**
 * @file test_main.c
 * @brief Host-side test runner: owns the shared counters, summary, and main().
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Each module's tests live in test_<mod>.c as `void test_xxx(void)`. Declare
 * them here and add a TEST_RUN line in main().
 */

#include "test_assert.h"

unsigned tests_run = 0;
unsigned tests_failed = 0;
unsigned checks_failed = 0;

int test_summary(void) {
  printf("\n%u tests, %u failed, %u checks failed\n", tests_run, tests_failed,
         checks_failed);
  return tests_failed == 0 ? 0 : 1;
}

/* --- gpio (test_gpio.c) --- */
void test_encode_output_modes(void);
void test_encode_input_modes(void);
void test_cr_shift(void);
void test_pin_init_pc13(void);
void test_pin_init_crl_isolation(void);
void test_pin_init_pull_direction(void);
void test_write_and_toggle(void);
void test_afio_sets_field(void);
void test_afio_keeps_other_remaps(void);
void test_afio_never_writes_back_swj(void);

/* --- clock (test_clock.c) --- */
void test_flash_latency_boundaries(void);
void test_sysclk_from_config(void);
void test_default_config_picks_2ws(void);
void test_tick_expired(void);

/* --- timer (test_timer.c) --- */
void test_solver_no_prescale(void);
void test_solver_1khz_needs_prescale(void);
void test_solver_needs_prescale(void);
void test_solver_1khz_from_36mhz(void);
void test_solver_guards_bad_input(void);
void test_solver_rejects_unreachable_freq(void);
void test_solver_psc_edges(void);

/* --- uart (test_uart.c) --- */
void test_brr_72mhz_115200(void);
void test_brr_72mhz_9600(void);
void test_brr_36mhz_115200(void);
void test_brr_zero_baud_guarded(void);
void test_try_read_empty(void);
void test_try_read_byte(void);
void test_try_read_drops_parity_bit(void);

/* --- pwm (test_pwm.c) --- */
void test_pwm_duty_zero(void);
void test_pwm_duty_full(void);
void test_pwm_duty_half(void);
void test_pwm_duty_quarter(void);
void test_pwm_duty_clamps_over_1000(void);
void test_pwm_duty_rounds_down(void);
void test_pwm_duty_full_at_max_arr(void);
void test_pwm_channel_init_ch2(void);
void test_pwm_channel_init_ch4_uses_ccmr2(void);
void test_pwm_channel_init_running_no_ug(void);
void test_pwm_stop_forces_low_start_restores(void);

/* --- i2c (test_i2c.c) --- */
void test_i2c_ccr_standard(void);
void test_i2c_ccr_fast(void);
void test_i2c_trise_standard(void);
void test_i2c_trise_fast(void);

/* --- can (test_can.c) --- */
void test_can_timing_common_rates(void);
void test_can_timing_tie_prefers_more_tq(void);
void test_can_timing_hsi_default_clock(void);
void test_can_timing_rejects_impossible(void);
void test_can_btr_stores_minus_one(void);
void test_can_frame_to_mailbox_standard(void);
void test_can_frame_to_mailbox_extended(void);
void test_can_frame_to_mailbox_remote(void);
void test_can_mailbox_to_frame(void);
void test_can_filter_values(void);
void test_can_filter_mask_bank(void);
void test_can_try_send(void);
void test_can_try_receive(void);
void test_can_get_errors(void);
void test_can_queue_fifo_order(void);
void test_can_queue_full_drops_newest(void);
void test_can_queue_index_wrap(void);

/* --- iwdg (test_iwdg.c) --- */
void test_iwdg_calc_values(void);
void test_iwdg_calc_rejects(void);
void test_iwdg_start_and_feed(void);

/* --- adc (test_adc.c) --- */
void test_adc_prescaler(void);
void test_adc_vdda_and_mv(void);
void test_adc_temp(void);
void test_adc_start_sets_sequence(void);
void test_adc_try_result(void);
void test_adc_sequence_short(void);
void test_adc_sequence_full(void);

/* --- dma (test_dma.c) --- */
void test_dma_ccr_uart_tx(void);
void test_dma_ccr_adc_scan(void);
void test_dma_ccr_mem_to_mem(void);
void test_dma_channel_index(void);

/* --- spi (test_spi.c) --- */
void test_spi_calc_br(void);
void test_spi_calc_br_rejects(void);
void test_spi_calc_cr1(void);
void test_spi_transfer_loop(void);

/* --- encoder (test_encoder.c) --- */
void test_encoder_delta_wraps(void);
void test_encoder_init_registers(void);
void test_encoder_update_accumulates(void);

/* --- capture (test_capture.c) --- */
void test_capture_psc(void);
void test_capture_init_registers(void);
void test_capture_read(void);

/* --- tim1 (test_tim1.c) --- */
void test_tim1_dtg_values(void);
void test_tim1_dtg_rejects(void);
void test_tim1_dtg_shortest_safe(void);
void test_tim1_bdtr(void);
void test_tim1_channel_complementary(void);
void test_tim1_start_stop_trip(void);

/* --- rtc (test_rtc.c) --- */
void test_rtc_set_get(void);
void test_rtc_alarm_at(void);
void test_rtc_alarm_fired_once(void);

/* --- error (test_error.c) --- */
void test_status_str_known(void);
void test_status_str_unknown(void);
void test_assert_true_is_noop(void);

int main(void) {
  TEST_RUN(test_encode_output_modes);
  TEST_RUN(test_encode_input_modes);
  TEST_RUN(test_cr_shift);
  TEST_RUN(test_pin_init_pc13);
  TEST_RUN(test_pin_init_crl_isolation);
  TEST_RUN(test_pin_init_pull_direction);
  TEST_RUN(test_write_and_toggle);
  TEST_RUN(test_afio_sets_field);
  TEST_RUN(test_afio_keeps_other_remaps);
  TEST_RUN(test_afio_never_writes_back_swj);

  TEST_RUN(test_flash_latency_boundaries);
  TEST_RUN(test_sysclk_from_config);
  TEST_RUN(test_default_config_picks_2ws);
  TEST_RUN(test_tick_expired);

  TEST_RUN(test_solver_no_prescale);
  TEST_RUN(test_solver_1khz_needs_prescale);
  TEST_RUN(test_solver_needs_prescale);
  TEST_RUN(test_solver_1khz_from_36mhz);
  TEST_RUN(test_solver_guards_bad_input);
  TEST_RUN(test_solver_rejects_unreachable_freq);
  TEST_RUN(test_solver_psc_edges);

  TEST_RUN(test_brr_72mhz_115200);
  TEST_RUN(test_brr_72mhz_9600);
  TEST_RUN(test_brr_36mhz_115200);
  TEST_RUN(test_brr_zero_baud_guarded);
  TEST_RUN(test_try_read_empty);
  TEST_RUN(test_try_read_byte);
  TEST_RUN(test_try_read_drops_parity_bit);

  TEST_RUN(test_pwm_duty_zero);
  TEST_RUN(test_pwm_duty_full);
  TEST_RUN(test_pwm_duty_half);
  TEST_RUN(test_pwm_duty_quarter);
  TEST_RUN(test_pwm_duty_clamps_over_1000);
  TEST_RUN(test_pwm_duty_rounds_down);
  TEST_RUN(test_pwm_duty_full_at_max_arr);
  TEST_RUN(test_pwm_channel_init_ch2);
  TEST_RUN(test_pwm_channel_init_ch4_uses_ccmr2);
  TEST_RUN(test_pwm_channel_init_running_no_ug);
  TEST_RUN(test_pwm_stop_forces_low_start_restores);

  TEST_RUN(test_i2c_ccr_standard);
  TEST_RUN(test_i2c_ccr_fast);
  TEST_RUN(test_i2c_trise_standard);
  TEST_RUN(test_i2c_trise_fast);

  TEST_RUN(test_can_timing_common_rates);
  TEST_RUN(test_can_timing_tie_prefers_more_tq);
  TEST_RUN(test_can_timing_hsi_default_clock);
  TEST_RUN(test_can_timing_rejects_impossible);
  TEST_RUN(test_can_btr_stores_minus_one);
  TEST_RUN(test_can_frame_to_mailbox_standard);
  TEST_RUN(test_can_frame_to_mailbox_extended);
  TEST_RUN(test_can_frame_to_mailbox_remote);
  TEST_RUN(test_can_mailbox_to_frame);
  TEST_RUN(test_can_filter_values);
  TEST_RUN(test_can_filter_mask_bank);
  TEST_RUN(test_can_try_send);
  TEST_RUN(test_can_try_receive);
  TEST_RUN(test_can_get_errors);
  TEST_RUN(test_can_queue_fifo_order);
  TEST_RUN(test_can_queue_full_drops_newest);
  TEST_RUN(test_can_queue_index_wrap);

  TEST_RUN(test_iwdg_calc_values);
  TEST_RUN(test_iwdg_calc_rejects);
  TEST_RUN(test_iwdg_start_and_feed);

  TEST_RUN(test_adc_prescaler);
  TEST_RUN(test_adc_vdda_and_mv);
  TEST_RUN(test_adc_temp);
  TEST_RUN(test_adc_start_sets_sequence);
  TEST_RUN(test_adc_try_result);
  TEST_RUN(test_adc_sequence_short);
  TEST_RUN(test_adc_sequence_full);

  TEST_RUN(test_dma_ccr_uart_tx);
  TEST_RUN(test_dma_ccr_adc_scan);
  TEST_RUN(test_dma_ccr_mem_to_mem);
  TEST_RUN(test_dma_channel_index);

  TEST_RUN(test_spi_calc_br);
  TEST_RUN(test_spi_calc_br_rejects);
  TEST_RUN(test_spi_calc_cr1);
  TEST_RUN(test_spi_transfer_loop);

  TEST_RUN(test_encoder_delta_wraps);
  TEST_RUN(test_encoder_init_registers);
  TEST_RUN(test_encoder_update_accumulates);

  TEST_RUN(test_capture_psc);
  TEST_RUN(test_capture_init_registers);
  TEST_RUN(test_capture_read);

  TEST_RUN(test_tim1_dtg_values);
  TEST_RUN(test_tim1_dtg_rejects);
  TEST_RUN(test_tim1_dtg_shortest_safe);
  TEST_RUN(test_tim1_bdtr);
  TEST_RUN(test_tim1_channel_complementary);
  TEST_RUN(test_tim1_start_stop_trip);

  TEST_RUN(test_rtc_set_get);
  TEST_RUN(test_rtc_alarm_at);
  TEST_RUN(test_rtc_alarm_fired_once);

  TEST_RUN(test_status_str_known);
  TEST_RUN(test_status_str_unknown);
  TEST_RUN(test_assert_true_is_noop);

  return TEST_SUMMARY();
}
