

#ifndef COMMON_H
#define COMMON_H

#include "stdlib.h"
#include "string.h"

#define RNN_INLINE inline
#define OPUS_INLINE inline


/** RNNoise wrapper for malloc(). To do your own dynamic allocation, all you need t
o do is replace this function and rnnoise_free */
#ifndef OVERRIDE_RNNOISE_ALLOC
static RNN_INLINE void *rnnoise_alloc (size_t size)
{
   return malloc(size);
}
#endif

/** RNNoise wrapper for free(). To do your own dynamic allocation, all you need to do is replace this function and rnnoise_alloc */
#ifndef OVERRIDE_RNNOISE_FREE
static RNN_INLINE void rnnoise_free (void *ptr)
{
   free(ptr);
}
#endif

/** Copy n elements from src to dst. The 0* term provides compile-time type checking  */
#ifndef OVERRIDE_RNN_COPY
#define RNN_COPY(dst, src, n) (memcpy((dst), (src), (n)*sizeof(*(dst)) + 0*((dst)-(src)) ))
#endif

/** Copy n elements from src to dst, allowing overlapping regions. The 0* term
    provides compile-time type checking */
#ifndef OVERRIDE_RNN_MOVE
#define RNN_MOVE(dst, src, n) (memmove((dst), (src), (n)*sizeof(*(dst)) + 0*((dst)-(src)) ))
#endif

/** Set n elements of dst to zero */
#ifndef OVERRIDE_RNN_CLEAR
#define RNN_CLEAR(dst, n) (memset((dst), 0, (n)*sizeof(*(dst))))
#endif
/** Prefix internal RNNoise symbols to avoid collisions with Opus/CELT in static builds */
#define _celt_lpc              rnnoise__celt_lpc
#define _celt_autocorr         rnnoise__celt_autocorr
#define celt_fir               rnnoise_celt_fir
#define celt_iir               rnnoise_celt_iir
#define celt_pitch_xcorr       rnnoise_celt_pitch_xcorr
#define pitch_downsample       rnnoise_pitch_downsample
#define pitch_search           rnnoise_pitch_search
#define remove_doubling        rnnoise_remove_doubling
#define pitch_filter           rnnoise_pitch_filter
#define opus_fft_alloc         rnnoise_opus_fft_alloc
#define opus_fft_alloc_arch_c  rnnoise_opus_fft_alloc_arch_c
#define opus_fft_alloc_twiddles rnnoise_opus_fft_alloc_twiddles
#define opus_fft_c             rnnoise_opus_fft_c
#define opus_fft_free          rnnoise_opus_fft_free
#define opus_fft_free_arch_c   rnnoise_opus_fft_free_arch_c
#define opus_fft_impl          rnnoise_opus_fft_impl
#define opus_ifft_c            rnnoise_opus_ifft_c
#define opus_ifft_impl         rnnoise_opus_ifft_impl
#define compute_band_corr      rnnoise_compute_band_corr
#define compute_band_energy    rnnoise_compute_band_energy
#define compute_dense          rnnoise_compute_dense
#define compute_gru            rnnoise_compute_gru
#define compute_rnn            rnnoise_compute_rnn
#define interp_band_gain       rnnoise_interp_band_gain
#define rnvad_compute_rnn      rnnoise_rnvad_compute_rnn

#endif
