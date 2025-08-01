#ifndef AUDIO_INPUT_H
#define AUDIO_INPUT_H

#include <stdint.h>

class AudioInput {
public:
    AudioInput();
    ~AudioInput();
    void init();
    void captureChunk();
    const uint16_t* getBuffer() const;

private:
    uint16_t* audio_chunk_buffer;
};

#endif // AUDIO_INPUT_H