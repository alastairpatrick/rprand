#include <memory.h>
#include <stdio.h>

#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/structs/rosc.h"
#include "hardware/sync.h"
#include "pico/stdio.h"

#define NUM_SAMPLES 100

#ifndef RPRAND_POOL_SIZE_BITS
#define RPRAND_POOL_SIZE_BITS 3
#endif

#define POOL_SIZE (1 << RPRAND_POOL_SIZE_BITS)

static spin_lock_t* g_spin_lock;
static int g_dma_timer = -1;
static int g_sample_dma_channel = -1;
static int g_store_dma_channel = -1;

static volatile uint32_t g_primary_pool[POOL_SIZE] __attribute__ ((aligned (1 << (RPRAND_POOL_SIZE_BITS + 2))));
static volatile uint32_t g_secondary_pool[POOL_SIZE];
static volatile int g_pool_idx;

void rprand_maximize_rosc() {
  rosc_hw->ctrl = (ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB) | (ROSC_CTRL_FREQ_RANGE_VALUE_HIGH << ROSC_CTRL_FREQ_RANGE_LSB);
  rosc_hw->freqa = (ROSC_FREQA_PASSWD_VALUE_PASS << ROSC_FREQA_PASSWD_LSB) | 0xFFFF;
  rosc_hw->freqb = (ROSC_FREQB_PASSWD_VALUE_PASS << ROSC_FREQB_PASSWD_LSB) | 0xFFFF;
}

void rprand_init(int sample_freq_hz) {
  static uint32_t dummy;
  dma_channel_config c;

  if (g_dma_timer < 0) {
    g_spin_lock = spin_lock_instance(next_striped_spin_lock_num());
    g_dma_timer = dma_claim_unused_timer(true);
  }

  if (sample_freq_hz <= 0) {
    // Sampling RANDOMBIT at 1/16th of ROSC seems to eliminate most of the correlation between consecutive samples.
    sample_freq_hz = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC) * 1000 / 16;
    assert(sample_freq_hz >= 0);
  }

  int divider = clock_get_hz(clk_sys) / sample_freq_hz;
  assert(divider >= 1 && divider < 0x10000);

  dma_timer_set_fraction(g_dma_timer, 1, divider);

  if (g_sample_dma_channel < 0) {
    g_sample_dma_channel = dma_claim_unused_channel(true);
    if (RPRAND_POOL_SIZE_BITS != 0) {
      g_store_dma_channel = dma_claim_unused_channel(true);
    }

    // DMA shannel samples RANDOMBITS several times with a delay between samples.
    c = dma_channel_get_default_config(g_sample_dma_channel);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, dma_get_timer_dreq(g_dma_timer));
    channel_config_set_sniff_enable(&c, true);
    if (RPRAND_POOL_SIZE_BITS != 0) {
      channel_config_set_chain_to(&c, g_store_dma_channel);
    }
    dma_channel_configure(g_sample_dma_channel, &c, &dummy, &rosc_hw->randombit, NUM_SAMPLES, false);
    dma_sniffer_enable(g_sample_dma_channel, DMA_SNIFF_CTRL_CALC_VALUE_CRC32, true);
    dma_hw->sniff_data = 0;

    if (RPRAND_POOL_SIZE_BITS != 0) {
      // Stores DMA sniffer's generated CRC-32 checksum in output ring buffer.
      c = dma_channel_get_default_config(g_store_dma_channel);
      channel_config_set_read_increment(&c, false);
      channel_config_set_write_increment(&c, true);
      channel_config_set_ring(&c, true /* write */, RPRAND_POOL_SIZE_BITS + 2);
      channel_config_set_chain_to(&c, g_sample_dma_channel);
      dma_channel_configure(g_store_dma_channel, &c, g_primary_pool, &dma_hw->sniff_data, 1, false);
    }

    dma_channel_start(g_sample_dma_channel);
  }
}

uint32_t rprand_get_32() {
  uint32_t result, save;
  bool done = false;

  for (;;) {
    if (RPRAND_POOL_SIZE_BITS == 0) {
      dma_channel_wait_for_finish_blocking(g_sample_dma_channel);
    } else {
      do {
        result = g_primary_pool[g_pool_idx];
      } while (result == g_secondary_pool[g_pool_idx]);
    }

    save = spin_lock_blocking(g_spin_lock);

    if (RPRAND_POOL_SIZE_BITS == 0) {
      if (!dma_channel_is_busy(g_sample_dma_channel)) {
        result = dma_hw->sniff_data;
        dma_channel_start(g_sample_dma_channel);
        done = true;
      }
    } else {
      result = g_primary_pool[g_pool_idx];
      if (result != g_secondary_pool[g_pool_idx]) {
        g_secondary_pool[g_pool_idx] = result;
        g_pool_idx = (g_pool_idx + 1) & (POOL_SIZE - 1);
        done = true;
      }
    }

    spin_unlock(g_spin_lock, save);

    if (done) return result;
  }
}

uint64_t rprand_get_64() {
  uint64_t result = rprand_get_32();
  result = (result << 32) | rprand_get_32();
  return result;
}
