#pragma once

#include <windows.h>
#include <RenderWare.h>
#include <cstdint>

extern "C" {
    __declspec(dllexport) RwTexture* LoadSVGToRwTexture(const char* filePath, uint32_t width, uint32_t height);
}