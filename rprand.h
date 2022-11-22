#ifndef _RPRAND_H_
#define _RPRAND_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// This module does not work while using ROSC as the system clock.

// Configure ROSC for maximum frequency, which causes random numbers to be generated faster.
void rprand_maximize_rosc();

// Causes random numbers to be calculated and asynchronously added to the random number pool.
// Must be called before generating random numbers and after changing the system clock or ROSC
// frequency. One might also call it periodically to compensate from changes in ROSC frequency.
// Pass 0 as sample_freq_hz to calculate a good sampling frequency based on the measured ROSC
// frequency.
void rprand_init(int sample_freq_hz);

// Returns the next random number from the pool, busy-waiting until the pool is non-empty.
// Thread safe.
uint32_t rprand_get_32();
uint64_t rprand_get_64();

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // _RPRAND_H_
