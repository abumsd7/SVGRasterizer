#include "../includes/Hooks.h"
#include "../includes/SVGRasterizer.h"

extern "C" __declspec(dllexport) RwTexture* LoadSVGToRwTexture(const char* filePath, uint32_t width, uint32_t height) {
    return SVGRasterizer::LoadSVGToRwTexture(filePath, width, height);
}