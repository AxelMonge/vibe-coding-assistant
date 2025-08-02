#ifndef AUDIO_INPUT_H
#define AUDIO_INPUT_H

#include <stdint.h>

class AudioInput {
public:
    AudioInput();
    ~AudioInput();
    void init();
    // La función de captura ahora vuelve a ser síncrona.
    void captureChunk(int16_t* buffer);

private:
    // No necesitamos miembros privados en esta versión simple.
};

#endif // AUDIO_INPUT_H