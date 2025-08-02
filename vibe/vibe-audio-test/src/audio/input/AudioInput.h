#ifndef AUDIO_INPUT_H
#define AUDIO_INPUT_H

#include <stdint.h>

class AudioInput {
public:
    AudioInput();
    ~AudioInput();
    void init();
    void captureChunk();
    const int16_t* getBuffer() const;

private:
    int16_t* audio_chunk_buffer;
};

#endif // AUDIO_INPUT_H