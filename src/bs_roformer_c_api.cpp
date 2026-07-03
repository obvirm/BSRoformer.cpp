// C API wrapper for BSRoformer — enables ctypes binding from Python.
// Follows the same pattern as whisper.cpp's whisper.h C API.

#include "bs_roformer/bs_roformer.h"
#include "bs_roformer/inference.h"
#include <cstring>
#include <new>

struct bs_roformer_context {
    Inference* inf;
};

struct bs_roformer_context* bs_roformer_init_from_file(const char* model_path) {
    if (!model_path || !model_path[0]) return nullptr;
    auto ctx = new (std::nothrow) bs_roformer_context();
    if (!ctx) return nullptr;
    try {
        ctx->inf = new Inference(model_path);
    } catch (...) {
        delete ctx;
        return nullptr;
    }
    return ctx;
}

void bs_roformer_free(struct bs_roformer_context* ctx) {
    if (!ctx) return;
    delete ctx->inf;
    delete ctx;
}

int bs_roformer_sample_rate(struct bs_roformer_context* ctx) {
    if (!ctx || !ctx->inf) return 0;
    return ctx->inf->GetSampleRate();
}

int bs_roformer_num_stems(struct bs_roformer_context* ctx) {
    if (!ctx || !ctx->inf) return 0;
    return ctx->inf->GetNumStems();
}

int bs_roformer_default_chunk_size(struct bs_roformer_context* ctx) {
    if (!ctx || !ctx->inf) return 0;
    return ctx->inf->GetDefaultChunkSize();
}

int bs_roformer_default_num_overlap(struct bs_roformer_context* ctx) {
    if (!ctx || !ctx->inf) return 0;
    return ctx->inf->GetDefaultNumOverlap();
}

int bs_roformer_process(
    struct bs_roformer_context* ctx,
    const float* input,
    int n_samples,
    float* output,
    int chunk_size,
    int num_overlap
) {
    if (!ctx || !ctx->inf || !input || !output) return -1;
    if (n_samples <= 0) return -1;

    try {
        // Wrap input as vector (no copy — Inference::Process accepts vector by const ref)
        const std::vector<float> input_vec(input, input + n_samples);

        auto stems = ctx->inf->Process(input_vec, chunk_size, num_overlap);
        int num_stems = (int)stems.size();

        // Copy each stem to the flat output buffer
        // Each stem should be n_samples (same length as input)
        for (int i = 0; i < num_stems; i++) {
            const auto& stem = stems[i];
            size_t copy_len = stem.size() < (size_t)n_samples ? stem.size() : (size_t)n_samples;
            std::memcpy(output + i * n_samples, stem.data(), copy_len * sizeof(float));

            // Pad remaining with zeros if stem is shorter than input
            if (copy_len < (size_t)n_samples) {
                std::memset(output + i * n_samples + copy_len, 0,
                           ((size_t)n_samples - copy_len) * sizeof(float));
            }
        }

        return 0;
    } catch (...) {
        return -1;
    }
}
