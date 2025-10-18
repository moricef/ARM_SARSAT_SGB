/**
 * @file pluto_control.c
 * @brief PlutoSDR Control via libiio
 *
 * Handles PlutoSDR configuration and I/Q transmission:
 * - Device initialization
 * - TX channel configuration
 * - I/Q buffer transmission
 * - Cleanup
 *
 * Based on SARSAT_FGB implementation, adapted for 2G (OQPSK)
 */

#include "pluto_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static struct iio_channel* get_channel(struct iio_device *dev, const char *id, int output) {
    struct iio_channel *chn = iio_device_find_channel(dev, id, output);
    if (!chn) {
        fprintf(stderr, "Channel %s not found\n", id);
    }
    return chn;
}

static int set_channel_attr_longlong(struct iio_channel *chn, const char *attr, long long val) {
    int ret = iio_channel_attr_write_longlong(chn, attr, val);
    if (ret < 0) {
        fprintf(stderr, "Failed to set %s: %s\n", attr, strerror(-ret));
    }
    return ret;
}

/* Unused - reserved for future use
static int set_device_attr_longlong(struct iio_device *dev, const char *attr, long long val) {
    int ret = iio_device_attr_write_longlong(dev, attr, val);
    if (ret < 0) {
        fprintf(stderr, "Failed to set device %s: %s\n", attr, strerror(-ret));
    }
    return ret;
}
*/

// =============================================================================
// INITIALIZATION & CONFIGURATION
// =============================================================================

int pluto_init(pluto_ctx_t *ctx, const char *uri) {
    if (!ctx) {
        fprintf(stderr, "Invalid context pointer\n");
        return -1;
    }

    memset(ctx, 0, sizeof(pluto_ctx_t));

    // Create IIO context
    if (uri) {
        ctx->ctx = iio_create_context_from_uri(uri);
        printf("Connecting to PlutoSDR at %s...\n", uri);
    } else {
        ctx->ctx = iio_create_default_context();
        printf("Connecting to PlutoSDR (auto-detect)...\n");
    }

    if (!ctx->ctx) {
        fprintf(stderr, "Failed to create IIO context\n");
        return -1;
    }

    // Get TX device (cf-ad9361-dds-core-lpc for TX data)
    ctx->tx_dev = iio_context_find_device(ctx->ctx, "cf-ad9361-dds-core-lpc");
    if (!ctx->tx_dev) {
        fprintf(stderr, "TX device (cf-ad9361-dds-core-lpc) not found\n");
        iio_context_destroy(ctx->ctx);
        ctx->ctx = NULL;
        return -1;
    }

    // Get TX I/Q channels (voltage0 = I, voltage1 = Q)
    ctx->tx_i = get_channel(ctx->tx_dev, "voltage0", 1);
    ctx->tx_q = get_channel(ctx->tx_dev, "voltage1", 1);

    if (!ctx->tx_i || !ctx->tx_q) {
        fprintf(stderr, "Failed to get TX I/Q channels\n");
        iio_context_destroy(ctx->ctx);
        ctx->ctx = NULL;
        return -1;
    }

    // Enable TX channels
    iio_channel_enable(ctx->tx_i);
    iio_channel_enable(ctx->tx_q);

    ctx->initialized = 1;
    printf("✓ PlutoSDR initialized successfully\n");

    return 0;
}

int pluto_configure_tx(pluto_ctx_t *ctx,
                      uint64_t frequency,
                      int32_t gain_db,
                      uint32_t sample_rate) {
    if (!ctx || !ctx->ctx || !ctx->initialized) {
        fprintf(stderr, "PlutoSDR not initialized\n");
        return -1;
    }

    // Get physical device (ad9361-phy) for configuration
    struct iio_device *phy = iio_context_find_device(ctx->ctx, "ad9361-phy");
    if (!phy) {
        fprintf(stderr, "ad9361-phy device not found\n");
        return -1;
    }

    // Get TX LO (local oscillator) channel
    struct iio_channel *tx_lo = get_channel(phy, "altvoltage1", 1);
    if (!tx_lo) {
        fprintf(stderr, "TX LO channel not found\n");
        return -1;
    }

    // Set TX frequency
    if (set_channel_attr_longlong(tx_lo, "frequency", frequency) < 0) {
        return -1;
    }
    ctx->frequency = frequency;

    // Get TX channel for gain/sample rate configuration
    struct iio_channel *tx_chan = get_channel(phy, "voltage0", 1);
    if (!tx_chan) {
        fprintf(stderr, "TX channel not found in phy\n");
        return -1;
    }

    // Set sample rate
    if (set_channel_attr_longlong(tx_chan, "sampling_frequency", sample_rate) < 0) {
        return -1;
    }

    // Set TX hardware gain (PlutoSDR uses attenuation: negative dB)
    // Range: -89.75 dB to 0 dB (in millidB: -89750 to 0)
    int32_t hw_gain_mdb = gain_db * 1000;
    if (hw_gain_mdb > 0) hw_gain_mdb = 0;
    if (hw_gain_mdb < -89750) hw_gain_mdb = -89750;

    if (set_channel_attr_longlong(tx_chan, "hardwaregain", hw_gain_mdb) < 0) {
        return -1;
    }
    ctx->gain_db = gain_db;

    // Set RF bandwidth (typically 1.5x to 2x sample rate)
    uint32_t rf_bandwidth = sample_rate * 2;
    set_channel_attr_longlong(tx_chan, "rf_bandwidth", rf_bandwidth);

    printf("✓ PlutoSDR TX configured:\n");
    printf("  Frequency: %llu Hz (%.3f MHz)\n",
           (unsigned long long)frequency, frequency / 1e6);
    printf("  Sample rate: %u Hz (%.1f kHz)\n",
           sample_rate, sample_rate / 1e3);
    printf("  TX gain: %d dB\n", gain_db);
    printf("  RF bandwidth: %u Hz (%.1f kHz)\n",
           rf_bandwidth, rf_bandwidth / 1e3);

    return 0;
}

// =============================================================================
// TRANSMISSION FUNCTIONS
// =============================================================================

int pluto_transmit_iq(pluto_ctx_t *ctx,
                     const float complex *iq_samples,
                     uint32_t num_samples) {
    if (!ctx || !ctx->tx_dev || !iq_samples || num_samples == 0) {
        fprintf(stderr, "Invalid parameters for transmission\n");
        return -1;
    }

    // Create TX buffer
    ctx->tx_buf = iio_device_create_buffer(ctx->tx_dev, num_samples, 0);
    if (!ctx->tx_buf) {
        fprintf(stderr, "Failed to create TX buffer\n");
        return -1;
    }

    // Get buffer pointer
    int16_t *buf = (int16_t *)iio_buffer_start(ctx->tx_buf);
    if (!buf) {
        fprintf(stderr, "Failed to get buffer pointer\n");
        iio_buffer_destroy(ctx->tx_buf);
        ctx->tx_buf = NULL;
        return -1;
    }

    // Convert float complex to int16 I/Q samples
    // PlutoSDR expects interleaved I/Q: [I0, Q0, I1, Q1, ...]
    // Range: -2048 to +2047 (12-bit DAC)
    for (uint32_t i = 0; i < num_samples; i++) {
        float i_val = crealf(iq_samples[i]);
        float q_val = cimagf(iq_samples[i]);

        // Scale float ±1.0 to int16 ±2047
        int16_t i_sample = (int16_t)(i_val * 2047.0f);
        int16_t q_sample = (int16_t)(q_val * 2047.0f);

        // Clamp to valid range
        if (i_sample > 2047) i_sample = 2047;
        if (i_sample < -2048) i_sample = -2048;
        if (q_sample > 2047) q_sample = 2047;
        if (q_sample < -2048) q_sample = -2048;

        buf[2*i]     = i_sample;  // I
        buf[2*i + 1] = q_sample;  // Q
    }

    // Push buffer to PlutoSDR
    ssize_t nbytes_tx = iio_buffer_push(ctx->tx_buf);
    if (nbytes_tx < 0) {
        fprintf(stderr, "TX buffer push failed: %s\n", strerror(-nbytes_tx));
        iio_buffer_destroy(ctx->tx_buf);
        ctx->tx_buf = NULL;
        return -1;
    }

    printf("✓ Transmitted %u I/Q samples (%zd bytes)\n", num_samples, nbytes_tx);

    // Cleanup buffer
    iio_buffer_destroy(ctx->tx_buf);
    ctx->tx_buf = NULL;

    return num_samples;
}

// =============================================================================
// TX ENABLE/DISABLE
// =============================================================================

int pluto_enable_tx(pluto_ctx_t *ctx, uint8_t enable) {
    if (!ctx || !ctx->ctx || !ctx->initialized) {
        fprintf(stderr, "PlutoSDR not initialized\n");
        return -1;
    }

    // Get physical device
    struct iio_device *phy = iio_context_find_device(ctx->ctx, "ad9361-phy");
    if (!phy) {
        fprintf(stderr, "ad9361-phy device not found\n");
        return -1;
    }

    // Get TX channel
    struct iio_channel *tx_chan = get_channel(phy, "voltage0", 1);
    if (!tx_chan) {
        fprintf(stderr, "TX channel not found\n");
        return -1;
    }

    // Enable/disable TX power
    const char *powerdown_attr = "powerdown";
    int ret = iio_channel_attr_write_bool(tx_chan, powerdown_attr, !enable);
    if (ret < 0) {
        fprintf(stderr, "Failed to %s TX: %s\n",
                enable ? "enable" : "disable", strerror(-ret));
        return -1;
    }

    printf("TX %s\n", enable ? "enabled" : "disabled");
    return 0;
}

// =============================================================================
// CLEANUP
// =============================================================================

void pluto_cleanup(pluto_ctx_t *ctx) {
    if (!ctx) return;

    if (ctx->tx_buf) {
        iio_buffer_destroy(ctx->tx_buf);
        ctx->tx_buf = NULL;
    }

    if (ctx->ctx) {
        iio_context_destroy(ctx->ctx);
        ctx->ctx = NULL;
    }

    ctx->initialized = 0;
    printf("PlutoSDR cleaned up\n");
}

// =============================================================================
// INFO & UTILITY FUNCTIONS
// =============================================================================

void pluto_print_info(const pluto_ctx_t *ctx) {
    if (!ctx || !ctx->ctx) {
        printf("PlutoSDR: Not connected\n");
        return;
    }

    printf("\nPlutoSDR Information:\n");
    printf("  Context: %s\n", iio_context_get_name(ctx->ctx));
    printf("  Description: %s\n", iio_context_get_description(ctx->ctx));

    // Get device attributes
    struct iio_device *phy = iio_context_find_device(ctx->ctx, "ad9361-phy");
    if (phy) {
        char fw_version[256];
        ssize_t ret = iio_device_attr_read(phy, "ensm_mode", fw_version, sizeof(fw_version));
        if (ret > 0) {
            printf("  ENSM Mode: %s\n", fw_version);
        }
    }

    if (ctx->initialized) {
        printf("  TX Frequency: %llu Hz (%.3f MHz)\n",
               (unsigned long long)ctx->frequency, ctx->frequency / 1e6);
        printf("  TX Gain: %d dB\n", ctx->gain_db);
    }

    printf("\n");
}

// =============================================================================
// QUERY FUNCTIONS
// =============================================================================

uint8_t pluto_is_connected(const pluto_ctx_t *ctx) {
    return (ctx && ctx->ctx && ctx->initialized);
}

uint64_t pluto_get_tx_frequency(const pluto_ctx_t *ctx) {
    if (!ctx || !ctx->ctx) return 0;

    struct iio_device *phy = iio_context_find_device(ctx->ctx, "ad9361-phy");
    if (!phy) return 0;

    struct iio_channel *tx_lo = get_channel(phy, "altvoltage1", 1);
    if (!tx_lo) return 0;

    long long freq = 0;
    iio_channel_attr_read_longlong(tx_lo, "frequency", &freq);
    return (uint64_t)freq;
}

uint32_t pluto_get_sample_rate(const pluto_ctx_t *ctx) {
    if (!ctx || !ctx->ctx) return 0;

    struct iio_device *phy = iio_context_find_device(ctx->ctx, "ad9361-phy");
    if (!phy) return 0;

    struct iio_channel *tx_chan = get_channel(phy, "voltage0", 1);
    if (!tx_chan) return 0;

    long long rate = 0;
    iio_channel_attr_read_longlong(tx_chan, "sampling_frequency", &rate);
    return (uint32_t)rate;
}

// =============================================================================
// FILE I/O FUNCTIONS
// =============================================================================

int pluto_save_iq_file(const char *filename,
                       const float complex *iq_samples,
                       uint32_t num_samples) {
    if (!filename || !iq_samples || num_samples == 0) {
        fprintf(stderr, "Invalid parameters for file save\n");
        return -1;
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open output file '%s': %s\n",
                filename, strerror(errno));
        return -1;
    }

    // Write interleaved float I/Q samples (GNU Radio/inspectrum compatible)
    // Format: [I0, Q0, I1, Q1, I2, Q2, ...]
    for (uint32_t i = 0; i < num_samples; i++) {
        float i_val = crealf(iq_samples[i]);
        float q_val = cimagf(iq_samples[i]);

        if (fwrite(&i_val, sizeof(float), 1, fp) != 1 ||
            fwrite(&q_val, sizeof(float), 1, fp) != 1) {
            fprintf(stderr, "Failed to write sample %u: %s\n", i, strerror(errno));
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);

    // Calculate file size
    size_t file_size = num_samples * 2 * sizeof(float);
    printf("✓ Saved %u I/Q samples to '%s' (%.2f KB)\n",
           num_samples, filename, file_size / 1024.0);
    printf("  Format: 32-bit float interleaved I/Q\n");
    printf("  Sample rate: 2.5 MHz\n");
    printf("  Duration: %.3f ms\n", (num_samples / 2500000.0) * 1000.0);

    return 0;
}
