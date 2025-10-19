/**
 * @file oqpsk_modulator.c
 * @brief OQPSK Modulator with DSSS spreading
 *
 * Complete port from dsPIC33CK implementation:
 * - OQPSK modulation with Tc/2 offset (Q-channel delayed by half chip)
 * - DSSS spreading (128 chips/bit using PRN sequences)
 * - Sample rate: 2.5 MHz (65.1 samples/chip)
 * - Chip rate: 38.4 kchips/s
 * - Data rate: 300 bps
 * - Linear interpolation between chips
 * - RRC pulse shaping filter (α=0.5)
 */

#include "oqpsk_modulator.h"
#include "prn_generator.h"
#include "rrc_filter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// =============================================================================
// CONSTANTS
// =============================================================================

#define PREAMBLE_BITS       50      // T.018 preamble duration
#define FRAME_TOTAL_BITS    300     // Preamble (50) + Data (250)

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static float interpolate_chip(float prev_chip, float curr_chip, float fraction) {
    // Linear interpolation between chips
    return prev_chip + (curr_chip - prev_chip) * fraction;
}

// =============================================================================
// OQPSK INITIALIZATION
// =============================================================================

void oqpsk_init(oqpsk_state_t *state) {
    memset(state, 0, sizeof(oqpsk_state_t));
    state->prev_i_chip = 0.0f;
    state->prev_q_chip = 0.0f;
    state->current_bit = 0;
    state->current_chip = 0;
    state->sample_count = 0;

    printf("✓ OQPSK modulator initialized\n");
}

// =============================================================================
// FRAME BUILDING
// =============================================================================

static void build_transmission_frame(const uint8_t *frame_bits, uint8_t *output_frame) {
    memset(output_frame, 0, FRAME_TOTAL_BITS);

    // T.018 Preamble (50 bits): Alternating 0,1 pattern for synchronization
    for (int i = 0; i < PREAMBLE_BITS; i++) {
        output_frame[i] = (i % 2);
    }

    // Copy 252-bit frame (2 header + 250 data)
    memcpy(&output_frame[PREAMBLE_BITS], frame_bits, 250);  // Copy 250 data bits
}

// =============================================================================
// OQPSK MODULATION
// =============================================================================

uint32_t oqpsk_modulate_bit(uint8_t bit,
                            const int8_t *i_chips,
                            const int8_t *q_chips,
                            float complex *iq_samples,
                            oqpsk_state_t *state) {
    uint32_t sample_idx = 0;

    // DSSS spreading: XOR data bit with PRN chips
    // T.018 Table 2.3: bit=0 → invert PRN, bit=1 → keep PRN
    int8_t spread_i[PRN_CHIPS_PER_BIT];
    int8_t spread_q[PRN_CHIPS_PER_BIT];

    for (int i = 0; i < PRN_CHIPS_PER_BIT; i++) {
        spread_i[i] = bit ? i_chips[i] : -i_chips[i];
        spread_q[i] = bit ? q_chips[i] : -q_chips[i];
    }

    // Generate samples for each chip with interpolation
    // Use fractional accumulator to handle non-integer samples/chip
    float sample_accumulator = 0.0f;

    for (int chip_idx = 0; chip_idx < PRN_CHIPS_PER_BIT; chip_idx++) {
        float curr_i_chip = (float)spread_i[chip_idx];
        float curr_q_chip = (float)spread_q[chip_idx];

        // Add samples for this chip (fractional)
        sample_accumulator += OQPSK_SAMPLES_PER_CHIP;
        int num_samples = (int)sample_accumulator;
        sample_accumulator -= num_samples;

        for (int s = 0; s < num_samples; s++) {
            // Safety check (each bit generates ~8,333 samples: 128 chips × 65.1 samp/chip)
            if (sample_idx >= 9000) {
                fprintf(stderr, "OVERFLOW in oqpsk_modulate_bit: sample_idx=%u (max ~8333)\n", sample_idx);
                return sample_idx;
            }

            float fraction = (float)s / OQPSK_SAMPLES_PER_CHIP;

            // I-channel: linear interpolation
            float i_value = interpolate_chip(state->prev_i_chip, curr_i_chip, fraction);

            // Q-channel: OQPSK half-chip delay
            // Use previous Q chip for first half, current for second half
            float q_value;
            if (fraction < 0.5f) {
                q_value = state->prev_q_chip;
            } else {
                q_value = interpolate_chip(state->prev_q_chip, curr_q_chip, (fraction - 0.5f) * 2.0f);
            }

            // Generate complex I/Q sample
            iq_samples[sample_idx++] = i_value + I * q_value;

            state->sample_count++;
        }

        // Update previous chips
        state->prev_i_chip = curr_i_chip;
        state->prev_q_chip = curr_q_chip;
    }

    return sample_idx;
}

uint32_t oqpsk_modulate_frame(const uint8_t *frame_bits,
                              float complex *iq_samples) {
    // Build complete transmission frame (50 preamble + 250 data)
    uint8_t tx_frame[FRAME_TOTAL_BITS];
    build_transmission_frame(frame_bits, tx_frame);

    // Initialize OQPSK state
    oqpsk_state_t state;
    oqpsk_init(&state);

    // Initialize PRN generator
    prn_state_t prn_state;
    prn_init(&prn_state, 0);  // Normal mode

    uint32_t total_samples = 0;
    int8_t i_chips[PRN_CHIPS_PER_BIT];
    int8_t q_chips[PRN_CHIPS_PER_BIT];

    printf("Modulating T.018 frame (%d bits)...\n", FRAME_TOTAL_BITS);

    // Modulate each bit
    for (int bit_idx = 0; bit_idx < FRAME_TOTAL_BITS; bit_idx++) {
        uint8_t data_bit = tx_frame[bit_idx];

        // Generate PRN sequences for this bit
        prn_generate_i(&prn_state, i_chips);
        prn_generate_q(&prn_state, q_chips);

        // Check buffer overflow before modulation
        if (total_samples > OQPSK_TOTAL_SAMPLES - 20000) {
            fprintf(stderr, "ERROR: Buffer overflow risk at bit %d (samples: %u)\n", bit_idx, total_samples);
            return total_samples;
        }

        // Modulate this bit
        uint32_t samples = oqpsk_modulate_bit(data_bit, i_chips, q_chips,
                                              &iq_samples[total_samples], &state);
        total_samples += samples;

        // Progress indicator every 50 bits
        if ((bit_idx + 1) % 50 == 0) {
            printf("  %d/%d bits modulated (samples: %u)\n", bit_idx + 1, FRAME_TOTAL_BITS, total_samples);
        }
    }

    printf("✓ Modulation complete: %u samples generated\n", total_samples);

    // Apply RRC pulse shaping filter
    printf("Applying RRC pulse shaping filter...\n");

    // Allocate temporary buffer for unfiltered samples
    float complex *unfiltered = malloc(total_samples * sizeof(float complex));
    if (!unfiltered) {
        fprintf(stderr, "Failed to allocate memory for RRC filtering\n");
        return total_samples;  // Return unfiltered if allocation fails
    }

    // Copy unfiltered samples
    memcpy(unfiltered, iq_samples, total_samples * sizeof(float complex));

    // Initialize and apply RRC filter
    rrc_state_t rrc_state;
    rrc_init(&rrc_state);
    rrc_filter(&rrc_state, unfiltered, iq_samples, total_samples);

    free(unfiltered);

    printf("✓ RRC filtering complete\n");

    return total_samples;
}

// =============================================================================
// VERIFICATION
// =============================================================================

uint8_t oqpsk_verify_output(const float complex *iq_samples, uint32_t num_samples) {
    printf("Verifying OQPSK output...\n");

    // Check for valid range (I and Q should be ±1 with interpolation)
    float max_i = 0.0f, max_q = 0.0f;
    float min_i = 0.0f, min_q = 0.0f;

    for (uint32_t i = 0; i < num_samples; i++) {
        float i_val = crealf(iq_samples[i]);
        float q_val = cimagf(iq_samples[i]);

        if (i_val > max_i) max_i = i_val;
        if (i_val < min_i) min_i = i_val;
        if (q_val > max_q) max_q = q_val;
        if (q_val < min_q) min_q = q_val;

        // Check for invalid values
        if (isnan(i_val) || isnan(q_val) || isinf(i_val) || isinf(q_val)) {
            printf("✗ Invalid sample at index %u: I=%f, Q=%f\n", i, i_val, q_val);
            return 0;
        }
    }

    printf("  I range: [%.3f, %.3f]\n", min_i, max_i);
    printf("  Q range: [%.3f, %.3f]\n", min_q, max_q);

    // Verify reasonable bounds (should be within ±1.5 due to interpolation)
    if (max_i > 1.5f || min_i < -1.5f || max_q > 1.5f || min_q < -1.5f) {
        printf("✗ Sample values out of expected range\n");
        return 0;
    }

    // Calculate average power
    float avg_power = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        float i_val = crealf(iq_samples[i]);
        float q_val = cimagf(iq_samples[i]);
        avg_power += (i_val * i_val + q_val * q_val);
    }
    avg_power /= num_samples;

    printf("  Average power: %.3f\n", avg_power);

    // Expected power should be around 1.0 (±1 signals)
    if (avg_power < 0.5f || avg_power > 2.0f) {
        printf("✗ Average power out of expected range (0.5-2.0)\n");
        return 0;
    }

    printf("✓ OQPSK output verified\n");
    return 1;
}

// =============================================================================
// DEBUG FUNCTIONS
// =============================================================================

void oqpsk_print_stats(const float complex *iq_samples, uint32_t num_samples) {
    printf("\nOQPSK Statistics:\n");
    printf("  Total samples: %u\n", num_samples);
    printf("  Duration: %.3f ms\n", (float)num_samples / OQPSK_SAMPLE_RATE * 1000.0f);
    printf("  Sample rate: %u Hz\n", OQPSK_SAMPLE_RATE);
    printf("  Chip rate: %u chips/s\n", OQPSK_CHIP_RATE);
    printf("  Data rate: %u bps\n", OQPSK_DATA_RATE);

    // Calculate RMS power
    float rms_i = 0.0f, rms_q = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        float i_val = crealf(iq_samples[i]);
        float q_val = cimagf(iq_samples[i]);
        rms_i += i_val * i_val;
        rms_q += q_val * q_val;
    }
    rms_i = sqrtf(rms_i / num_samples);
    rms_q = sqrtf(rms_q / num_samples);

    printf("  RMS power: I=%.3f, Q=%.3f\n", rms_i, rms_q);

    // Calculate peak-to-average power ratio (PAPR)
    float peak_power = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        float i_val = crealf(iq_samples[i]);
        float q_val = cimagf(iq_samples[i]);
        float power = i_val * i_val + q_val * q_val;
        if (power > peak_power) peak_power = power;
    }
    float avg_power = rms_i * rms_i + rms_q * rms_q;
    float papr = 10.0f * log10f(peak_power / avg_power);

    printf("  PAPR: %.2f dB\n", papr);
}
