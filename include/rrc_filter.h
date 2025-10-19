/**
 * @file rrc_filter.h
 * @brief Root-Raised Cosine (RRC) Filter for OQPSK modulation
 *
 * Software implementation of RRC pulse shaping filter for T.018 2G beacons.
 * Improves spectral efficiency by limiting bandwidth while maintaining
 * signal integrity.
 *
 * Parameters:
 * - Chip rate: 38.4 kchips/s
 * - Sample rate: 2.5 MHz (PlutoSDR)
 * - Samples/chip: 65.1
 * - Roll-off (α): 0.5 (standard for OQPSK)
 * - Filter taps: 129 (odd for symmetry, ~2 chip periods)
 */

#ifndef RRC_FILTER_H
#define RRC_FILTER_H

#include <stdint.h>
#include <stddef.h>
#include <complex.h>

// RRC filter parameters
#define RRC_NUM_TAPS        129         // Number of filter taps (must be odd)
#define RRC_ROLLOFF         0.5         // Roll-off factor (α)
#define RRC_SAMPLES_PER_CHIP 65.104167  // 2.5 MHz / 38.4 kHz
#define RRC_CENTER_TAP      64          // Center tap index

// Filter state structure
typedef struct {
    float i_history[RRC_NUM_TAPS];  // I-channel sample history
    float q_history[RRC_NUM_TAPS];  // Q-channel sample history
    uint32_t write_idx;              // Circular buffer write index
} rrc_state_t;

/**
 * @brief Initialize RRC filter state
 * @param state Filter state structure
 */
void rrc_init(rrc_state_t *state);

/**
 * @brief Apply RRC filter to complex I/Q samples
 * @param state Filter state
 * @param input Input I/Q samples
 * @param output Output filtered I/Q samples
 * @param num_samples Number of samples to process
 */
void rrc_filter(rrc_state_t *state,
                const float complex *input,
                float complex *output,
                uint32_t num_samples);

/**
 * @brief Get RRC filter coefficients (for verification/analysis)
 * @param coeffs Output buffer for coefficients
 * @param num_taps Number of taps to retrieve
 */
void rrc_get_coefficients(float *coeffs, uint32_t num_taps);

#endif // RRC_FILTER_H
