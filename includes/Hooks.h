#pragma once

#include <windows.h>
#include <RenderWare.h>
#include <cstdint>

extern "C" {
    __declspec(dllexport) RwTexture* LoadSVGToRwTexture(const char* filePath, uint32_t width, uint32_t height, bool generateMipmaps = true, uint32_t mipLevels = 0);
    __declspec(dllexport) RwTexture* LoadPNGToRwTexture(const char* filePath, uint32_t width = 0, uint32_t height = 0, bool generateMipmaps = true, uint32_t mipLevels = 0);
}