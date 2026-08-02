#pragma once

#include <windows.h>
#include <RenderWare.h>
#include <cstdint>

class SVGRasterizer {
public:
    typedef RwTexture* (*LoadSVGToRwTextureFn)(const char*, uint32_t, uint32_t);

    static RwTexture* LoadSVGToRwTexture(const char* filePath, uint32_t width, uint32_t height) {
        static LoadSVGToRwTextureFn fn = nullptr;
        if (!fn) {
            HMODULE hMod = GetModuleHandleA("SVGRasterizer.VC.asi");
            if (!hMod) hMod = GetModuleHandleA("SVGRasterizer.asi");
            if (!hMod) hMod = GetModuleHandleA("SVGRasterizer.VC.dll");
            if (!hMod) hMod = GetModuleHandleA("SVGRasterizer.dll");
            if (!hMod) hMod = LoadLibraryA("scripts\\SVGRasterizer.VC.asi");
            if (!hMod) hMod = LoadLibraryA("scripts\\SVGRasterizer.asi");
            if (!hMod) hMod = LoadLibraryA("SVGRasterizer.VC.asi");
            if (!hMod) hMod = LoadLibraryA("SVGRasterizer.asi");
            if (hMod) {
                fn = (LoadSVGToRwTextureFn)GetProcAddress(hMod, "LoadSVGToRwTexture");
            }
        }
        if (fn) {
            return fn(filePath, width, height);
        }
        return nullptr;
    }
};
