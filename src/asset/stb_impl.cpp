/// @file stb_impl.cpp
/// @brief STB library implementations
///
/// This file provides the single-compilation-unit implementations for
/// STB header-only libraries used throughout void_engine.

// stb_image - Image loading/decoding
#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include <stb_image.h>

// stb_image_write - Image encoding/saving
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
