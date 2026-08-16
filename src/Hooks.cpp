#include "../includes/Hooks.h"
#include "../includes/DirectImageRasterizer.h"

extern "C" __declspec(dllexport) RwTexture* LoadSVGToRwTexture(const char* filePath, uint32_t width, uint32_t height, bool generateMipmaps, uint32_t mipLevels) {
    return DirectImageRasterizer::LoadSVGToRwTexture(filePath, width, height, generateMipmaps, mipLevels);
}

extern "C" __declspec(dllexport) RwTexture* LoadPNGToRwTexture(const char* filePath, uint32_t width, uint32_t height, bool generateMipmaps, uint32_t mipLevels) {
    return DirectImageRasterizer::LoadPNGToRwTexture(filePath, width, height, generateMipmaps, mipLevels);
}