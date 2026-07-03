#define DR_WAV_IMPLEMENTATION
#include "dr_libs/dr_wav.h"
#include "bs_roformer/audio.h"
#include <iostream>

AudioBuffer AudioFile::Load(const std::string& path) {
    AudioBuffer buffer;
    drwav_uint64 totalPCMFrames;
    
    float* pData = drwav_open_file_and_read_pcm_frames_f32(
        path.c_str(), &buffer.channels, &buffer.sampleRate, &totalPCMFrames, NULL);
        
    if (!pData) {
        throw std::runtime_error("Failed to open audio file: " + path);
    }
    
    buffer.samples = totalPCMFrames * buffer.channels;
    buffer.data.assign(pData, pData + buffer.samples);
    drwav_free(pData, NULL);
    
    // Validation
    if (buffer.sampleRate != 44100) {
        std::cerr << "Warning: Input sample rate is " << buffer.sampleRate 
                  << " Hz. Model expects 44100 Hz." << std::endl;
    }
    
    return buffer;
}

void AudioFile::Save(const std::string& path, const AudioBuffer& buffer) {
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = buffer.channels;
    format.sampleRate = buffer.sampleRate;
    format.bitsPerSample = 32;
    
    drwav wav;
    if (!drwav_init_file_write(&wav, path.c_str(), &format, NULL)) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }
    
    drwav_uint64 framesWritten = drwav_write_pcm_frames(&wav, buffer.samples / buffer.channels, buffer.data.data());
    drwav_uninit(&wav);
    
    if (framesWritten != buffer.samples / buffer.channels) {
         throw std::runtime_error("Failed to write all samples to " + path);
    }
}

AudioBuffer AudioFile::LoadFromMemory(const void* data, size_t size) {
    AudioBuffer buffer;
    drwav_uint64 totalPCMFrames;
    
    float* pData = drwav_open_memory_and_read_pcm_frames_f32(
        data, size, &buffer.channels, &buffer.sampleRate, &totalPCMFrames, NULL);
        
    if (!pData) {
        throw std::runtime_error("Failed to parse WAV from memory");
    }
    
    buffer.samples = totalPCMFrames * buffer.channels;
    buffer.data.assign(pData, pData + buffer.samples);
    drwav_free(pData, NULL);
    
    if (buffer.sampleRate != 44100) {
        std::cerr << "Warning: Input sample rate is " << buffer.sampleRate 
                  << " Hz. Model expects 44100 Hz." << std::endl;
    }
    
    return buffer;
}

std::vector<char> AudioFile::SaveToMemory(const AudioBuffer& buffer) {
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = buffer.channels;
    format.sampleRate = buffer.sampleRate;
    format.bitsPerSample = 32;
    
    void* pData = NULL;
    size_t dataSize = 0;
    
    drwav wav;
    if (!drwav_init_memory_write(&wav, &pData, &dataSize, &format, NULL)) {
        throw std::runtime_error("Failed to initialize memory WAV write");
    }
    
    drwav_uint64 framesWritten = drwav_write_pcm_frames(&wav, buffer.samples / buffer.channels, buffer.data.data());
    drwav_uninit(&wav);
    
    if (framesWritten != buffer.samples / buffer.channels) {
        drwav_free(pData, NULL);
        throw std::runtime_error("Failed to write all samples to memory");
    }
    
    // Copy to vector, free dr_wav allocation
    std::vector<char> result(static_cast<const char*>(pData), static_cast<const char*>(pData) + dataSize);
    drwav_free(pData, NULL);
    
    return result;
}
