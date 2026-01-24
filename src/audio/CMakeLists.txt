# void_audio - Audio system
void_add_module(NAME void_audio
    SOURCES
        types.cpp
        buffer.cpp
        source.cpp
        listener.cpp
        mixer.cpp
        effects.cpp
        backend.cpp
        miniaudio_backend.cpp
    DEPENDENCIES
        void_core
        void_asset
        void_ecs
)

# Link miniaudio (header-only)
target_link_libraries(void_audio PRIVATE miniaudio)

# Platform-specific audio libraries
if(WIN32)
    # Windows audio APIs are loaded dynamically by miniaudio
elseif(UNIX AND NOT APPLE)
    # Linux: link pthread for miniaudio threading
    find_package(Threads REQUIRED)
    target_link_libraries(void_audio PRIVATE Threads::Threads)
    # ALSA or PulseAudio are loaded dynamically
elseif(APPLE)
    # macOS: CoreAudio framework
    target_link_libraries(void_audio PRIVATE "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
endif()

# Platform audio backends
if(WIN32)
    target_sources(void_audio PRIVATE
        # backends/xaudio2.cpp  # Optional: Native XAudio2 backend
    )
    # target_link_libraries(void_audio PRIVATE xaudio2)
elseif(UNIX AND NOT APPLE)
    target_sources(void_audio PRIVATE
        # backends/openal.cpp   # Optional: OpenAL backend
    )
    # find_package(OpenAL REQUIRED)
    # target_link_libraries(void_audio PRIVATE OpenAL::OpenAL)
elseif(APPLE)
    target_sources(void_audio PRIVATE
        # backends/coreaudio.cpp # Optional: CoreAudio backend
    )
    # target_link_libraries(void_audio PRIVATE "-framework AudioToolbox" "-framework CoreAudio")
endif()

# Optional: FMOD integration
option(VOID_AUDIO_USE_FMOD "Use FMOD as audio backend" OFF)
if(VOID_AUDIO_USE_FMOD)
    find_package(FMOD REQUIRED)
    target_compile_definitions(void_audio PRIVATE VOID_AUDIO_FMOD)
    target_link_libraries(void_audio PRIVATE FMOD::Core FMOD::Studio)
endif()

# Optional: Wwise integration
option(VOID_AUDIO_USE_WWISE "Use Wwise as audio backend" OFF)
if(VOID_AUDIO_USE_WWISE)
    find_package(Wwise REQUIRED)
    target_compile_definitions(void_audio PRIVATE VOID_AUDIO_WWISE)
    target_link_libraries(void_audio PRIVATE Wwise::SoundEngine)
endif()
