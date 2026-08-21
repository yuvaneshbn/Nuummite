// portaudio_static.h
// Drop-in static-linking replacement for the PortAudioApi LoadLibrary pattern.
// Include this instead of portaudio_dyn.h when PA_USE_STATIC is defined.
// The rest of audio_engine.cpp (all pa.OpenStream() etc. calls) is unchanged.

#ifndef PORTAUDIO_STATIC_H
#define PORTAUDIO_STATIC_H

#include <portaudio.h>   // real API from portaudio_static library
#include <string>
#include <vector>

// ── Thin static wrapper ──────────────────────────────────────────────────────
// Mirrors the interface of the dynamic PortAudioApi struct so that all call
// sites in audio_engine.cpp (pa.OpenStream, pa.GetDeviceCount, etc.) compile
// identically whether PA_USE_STATIC is set or not.

struct PortAudioApi {
    // Function pointers — initialised to the real Pa_ symbols at construction.
    // Using pointers (rather than calling Pa_ directly) keeps all call sites
    // in audio_engine.cpp identical between the dynamic and static builds.
    using Pa_Initialize_Fn          = PaError (*)();
    using Pa_Terminate_Fn           = PaError (*)();
    using Pa_GetErrorText_Fn        = const char* (*)(PaError);
    using Pa_GetDeviceCount_Fn      = PaDeviceIndex (*)();
    using Pa_GetDefaultInputDevice_Fn  = PaDeviceIndex (*)();
    using Pa_GetDefaultOutputDevice_Fn = PaDeviceIndex (*)();
    using Pa_GetDeviceInfo_Fn       = const PaDeviceInfo* (*)(PaDeviceIndex);
    using Pa_GetHostApiInfo_Fn      = const PaHostApiInfo* (*)(PaHostApiIndex);
    using Pa_OpenStream_Fn          = PaError (*)(PaStream**,
                                                   const PaStreamParameters*,
                                                   const PaStreamParameters*,
                                                   double, unsigned long,
                                                   PaStreamFlags,
                                                   PaStreamCallback*, void*);
    using Pa_CloseStream_Fn         = PaError (*)(PaStream*);
    using Pa_StartStream_Fn         = PaError (*)(PaStream*);
    using Pa_StopStream_Fn          = PaError (*)(PaStream*);
    using Pa_AbortStream_Fn         = PaError (*)(PaStream*);
    using Pa_ReadStream_Fn          = PaError (*)(PaStream*, void*, unsigned long);

    Pa_Initialize_Fn           Initialize          = Pa_Initialize;
    Pa_Terminate_Fn            Terminate           = Pa_Terminate;
    Pa_GetErrorText_Fn         GetErrorText        = Pa_GetErrorText;
    Pa_GetDeviceCount_Fn       GetDeviceCount      = Pa_GetDeviceCount;
    Pa_GetDefaultInputDevice_Fn  GetDefaultInputDevice  = Pa_GetDefaultInputDevice;
    Pa_GetDefaultOutputDevice_Fn GetDefaultOutputDevice = Pa_GetDefaultOutputDevice;
    Pa_GetDeviceInfo_Fn        GetDeviceInfo       = Pa_GetDeviceInfo;
    Pa_GetHostApiInfo_Fn       GetHostApiInfo      = Pa_GetHostApiInfo;
    Pa_OpenStream_Fn           OpenStream          = Pa_OpenStream;
    Pa_CloseStream_Fn          CloseStream         = Pa_CloseStream;
    Pa_StartStream_Fn          StartStream         = Pa_StartStream;
    Pa_StopStream_Fn           StopStream          = Pa_StopStream;
    Pa_AbortStream_Fn          AbortStream         = Pa_AbortStream;
    Pa_ReadStream_Fn           ReadStream          = Pa_ReadStream;

    bool initialized = false;

    // load() is a no-op for static builds — symbols are resolved at link time.
    bool load(std::string& /*error*/) { return true; }

    bool ensureReady(std::string& error) {
        if (initialized) return true;
        const PaError err = Pa_Initialize();
        if (err != paNoError) {
            const char* errText = Pa_GetErrorText(err);
            error = std::string("PortAudio static init failed: ") +
                    (errText ? errText : "unknown error");
            return false;
        }
        initialized = true;
        return true;
    }

    void unload() {
        if (initialized) {
            Pa_Terminate();
        }
        initialized = false;
    }
};

#endif // PORTAUDIO_STATIC_H
