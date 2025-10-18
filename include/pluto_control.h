/**
 * @file pluto_control.h
 * @brief PlutoSDR Control via libiio
 *
 * Handles PlutoSDR configuration and I/Q transmission:
 * - Device initialization
 * - TX channel configuration
 * - I/Q buffer transmission
 * - Cleanup
 */

#ifndef PLUTO_CONTROL_H
#define PLUTO_CONTROL_H

#include <stdint.h>
#include <complex.h>
#include <iio.h>

// PlutoSDR default parameters
#define PLUTO_DEFAULT_URI       "ip:192.168.2.1"
#define PLUTO_SAMPLE_RATE       2500000     // 2.5 MHz (same as FGB, validated)
#define PLUTO_BANDWIDTH         1000000     // 1 MHz RF bandwidth
#define PLUTO_DEFAULT_FREQ      403000000   // 403 MHz (training)
#define PLUTO_DEFAULT_GAIN_DB   -10         // Conservative TX gain

// PlutoSDR context
typedef struct {
    struct iio_context *ctx;                // IIO context
    struct iio_device *tx_dev;              // TX device (cf-ad9361-dds-core-lpc)
    struct iio_channel *tx_i;               // TX I channel
    struct iio_channel *tx_q;               // TX Q channel
    struct iio_buffer *tx_buf;              // TX buffer
    uint64_t frequency;                     // TX frequency (Hz)
    int32_t gain_db;                        // TX attenuation (dB)
    uint8_t initialized;                    // Init flag
} pluto_ctx_t;

/**
 * @brief Initialize PlutoSDR
 * @param ctx PlutoSDR context
 * @param uri Device URI (NULL = auto-detect)
 * @return 0 on success, -1 on error
 */
int pluto_init(pluto_ctx_t *ctx, const char *uri);

/**
 * @brief Configure TX parameters
 * @param ctx PlutoSDR context
 * @param frequency TX frequency in Hz
 * @param gain_db TX attenuation in dB (negative)
 * @param sample_rate Sample rate in Hz
 * @return 0 on success, -1 on error
 */
int pluto_configure_tx(pluto_ctx_t *ctx,
                      uint64_t frequency,
                      int32_t gain_db,
                      uint32_t sample_rate);

/**
 * @brief Transmit I/Q samples
 * @param ctx PlutoSDR context
 * @param iq_samples Complex I/Q samples
 * @param num_samples Number of samples
 * @return Number of samples transmitted, or -1 on error
 */
int pluto_transmit_iq(pluto_ctx_t *ctx,
                     const float complex *iq_samples,
                     uint32_t num_samples);

/**
 * @brief Enable/disable TX
 * @param ctx PlutoSDR context
 * @param enable 1=enable, 0=disable
 * @return 0 on success, -1 on error
 */
int pluto_enable_tx(pluto_ctx_t *ctx, uint8_t enable);

/**
 * @brief Cleanup PlutoSDR resources
 * @param ctx PlutoSDR context
 */
void pluto_cleanup(pluto_ctx_t *ctx);

/**
 * @brief Print PlutoSDR device info
 * @param ctx PlutoSDR context
 */
void pluto_print_info(const pluto_ctx_t *ctx);

/**
 * @brief Save I/Q samples to file (GNU Radio compatible format)
 * @param filename Output filename (.iq extension recommended)
 * @param iq_samples Complex I/Q samples
 * @param num_samples Number of samples
 * @return 0 on success, -1 on error
 */
int pluto_save_iq_file(const char *filename,
                       const float complex *iq_samples,
                       uint32_t num_samples);

#endif // PLUTO_CONTROL_H
