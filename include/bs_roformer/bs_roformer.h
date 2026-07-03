#ifndef BS_ROFORMER_H
#define BS_ROFORMER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#    if defined(BSR_BUILD_DLL)
#        define BSR_API __declspec(dllexport)
#    else
#        define BSR_API __declspec(dllimport)
#    endif
#elif defined(__GNUC__) || defined(__clang__)
#    define BSR_API __attribute__((visibility("default")))
#else
#    define BSR_API
#endif

    struct bs_roformer_context;

    /** Load model from GGUF file. Returns NULL on failure. */
    BSR_API struct bs_roformer_context* bs_roformer_init_from_file(const char* model_path);

    /** Free model context. */
    BSR_API void bs_roformer_free(struct bs_roformer_context* ctx);

    /** Get expected sample rate (typically 44100). */
    BSR_API int bs_roformer_sample_rate(struct bs_roformer_context* ctx);

    /** Get number of output stems (e.g., 2 for vocals + accompaniment). */
    BSR_API int bs_roformer_num_stems(struct bs_roformer_context* ctx);

    /** Get default chunk size. */
    BSR_API int bs_roformer_default_chunk_size(struct bs_roformer_context* ctx);

    /** Get default number of overlap chunks. */
    BSR_API int bs_roformer_default_num_overlap(struct bs_roformer_context* ctx);

    /**
     * Process interleaved stereo float32 audio.
     *
     * input  — [n_samples] interleaved stereo float32
     * output — pre-allocated [num_stems * n_samples] floats
     *          layout: stem0[0..n_samples-1], stem1[0..n_samples-1], ...
     *
     * Returns 0 on success, -1 on error.
     */
    BSR_API int bs_roformer_process(
        struct bs_roformer_context* ctx,
        const float* input,
        int n_samples,
        float* output,
        int chunk_size,
        int num_overlap
    );

#ifdef __cplusplus
}
#endif

#endif /* BS_ROFORMER_H */
