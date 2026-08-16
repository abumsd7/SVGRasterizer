#pragma once

#include "Hooks.h"

class DirectImageRasterizer {
public:
    static RwTexture* LoadSVGToRwTexture(const char* filePath, uint32_t width, uint32_t height, bool generateMipmaps = true, uint32_t mipLevels = 0);
    static RwTexture* LoadPNGToRwTexture(const char* filePath, uint32_t width = 0, uint32_t height = 0, bool generateMipmaps = true, uint32_t mipLevels = 0);
};
