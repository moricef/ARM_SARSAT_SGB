/**
 * @file oqpsk_modulator.h
 * @brief OQPSK Modulator with DSSS spreading
 *
 * Generates I/Q samples for T.018 2nd generation beacons:
 * - OQPSK modulation with Tc/2 offset
 * - DSSS spreading (128 chips/bit)
 * - Sample rate: 2.5 MHz (65.1 samples/chip)
 */

#ifndef OQPSK_MODULATOR_H
#define OQPSK_MODULATOR_H

#include <stdint.h>
#include <complex.h>

// T.018 modulation parameters
#define OQPSK_CHIP_RATE         38400       // 38.4 kchips/s
#define OQPSK_SAMPLE_RATE       2500000     // 2.5 MHz (PlutoSDR validated with FGB)
#define OQPSK_SAMPLES_PER_CHIP  65.104167   // 2.5M / 38.4k = 65.1 samples/chip
#define OQPSK_DATA_RATE         300         // 300 bps
#define OQPSK_CHIPS_PER_BIT     128         // 38.4k chips/s ÷ 300 bps = 128 chips/bit

// Frame timing
#define OQPSK_PREAMBLE_BITS     50          // Preamble duration
#define OQPSK_MESSAGE_BITS      250         // Message data
#define OQPSK_TOTAL_BITS        300         // Preamble + Message
#define OQPSK_TOTAL_SAMPLES     2600000     // 300 bits × 128 chips × 65.1 samp/chip + margin

// OQPSK modulator state
typedef struct {
    uint16_t current_bit;                   // Current bit position
    uint16_t current_chip;                  // Current chip in bit
    float prev_i_chip;                      // Previous I chip (for interpolation)
    float prev_q_chip;                      // Previous Q chip (OQPSK delay)
    uint32_t sample_count;                  // Total samples generated
} oqpsk_state_t;

/**
 * @brief Initialize OQPSK modulator
 * @param state Modulator state
 */
void oqpsk_init(oqpsk_state_t *state);

/**
 * @brief Generate I/Q samples for complete T.018 frame
 * @param frame_bits 252-bit frame (2 header + 250 data)
 * @param iq_samples Output buffer (960000 complex samples)
 * @return Number of samples generated
 *
 * Frame structure:
 * - 50 bits preamble (all 0s)
 * - 2 bits header
 * - 250 bits message (202 info + 48 BCH)
 */
uint32_t oqpsk_modulate_frame(const uint8_t *frame_bits,
                              float complex *iq_samples);

/**
 * @brief Generate I/Q samples for single data bit
 * @param bit Data bit (0 or 1)
 * @param i_chips I-channel PRN (128 chips)
 * @param q_chips Q-channel PRN (128 chips)
 * @param iq_samples Output buffer (~8333 samples)
 * @param state Modulator state
 * @return Number of samples generated
 */
uint32_t oqpsk_modulate_bit(uint8_t bit,
                            const int8_t *i_chips,
                            const int8_t *q_chips,
                            float complex *iq_samples,
                            oqpsk_state_t *state);

/**
 * @brief Verify modulator output (sanity check)
 * @param iq_samples Complex samples
 * @param num_samples Number of samples
 * @return 1 if valid, 0 if errors
 */
uint8_t oqpsk_verify_output(const float complex *iq_samples,
                            uint32_t num_samples);

#endif // OQPSK_MODULATOR_H
