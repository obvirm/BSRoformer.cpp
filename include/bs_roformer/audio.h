#pragma once
#include <vector>
#include <string>
#include <stdexcept>

/**
 * Audio buffer structure for storing audio data.
 * Data is stored in interleaved format (L, R, L, R, ...) for stereo.
 */
struct AudioBuffer {
    std::vector<float> data;  // Interleaved samples
    unsigned int channels;
    unsigned int sampleRate;
    size_t samples;           // Total samples (frames * channels)
};

/**
 * Audio file I/O utilities.
 * Supports WAV format (via dr_wav).
 */
class AudioFile {
public:
    /**
     * Load audio from a WAV file.
     * @param path Path to the WAV file
     * @return AudioBuffer containing the loaded audio data
     * @throws std::runtime_error if the file cannot be opened
     */
    static AudioBuffer Load(const std::string& path);
    
    /**
     * Save audio to a WAV file.
     * @param path Path to save the WAV file
     * @param buffer AudioBuffer containing audio data to save
     * @throws std::runtime_error if the file cannot be written
     */
    static void Save(const std::string& path, const AudioBuffer& buffer);

    /** Load WAV from raw memory buffer. */
    static AudioBuffer LoadFromMemory(const void* data, size_t size);

    /** Save WAV to a memory buffer (caller must free with drwav_free or use returned vector). */
    static std::vector<char> SaveToMemory(const AudioBuffer& buffer);
};
