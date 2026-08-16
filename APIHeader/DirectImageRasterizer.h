#pragma once

#include <windows.h>
#include <RenderWare.h>
#include <cstdint>

class DirectImageRasterizer {
private:
    static HMODULE GetModule() {
        static HMODULE hMod = nullptr;
        if (!hMod) {
            hMod = GetModuleHandleA("DirectImageRasterizer.VC.asi");
            if (!hMod) hMod = GetModuleHandleA("DirectImageRasterizer.asi");
            if (!hMod) hMod = GetModuleHandleA("DirectImageRasterizer.SA.asi");
            if (!hMod) hMod = GetModuleHandleA("DirectImageRasterizer.III.asi");
            if (!hMod) hMod = GetModuleHandleA("DirectImageRasterizer.VC.dll");
            if (!hMod) hMod = GetModuleHandleA("DirectImageRasterizer.dll");
            if (!hMod) hMod = LoadLibraryA("scripts\\DirectImageRasterizer.VC.asi");
            if (!hMod) hMod = LoadLibraryA("scripts\\DirectImageRasterizer.asi");
            if (!hMod) hMod = LoadLibraryA("scripts\\DirectImageRasterizer.SA.asi");
            if (!hMod) hMod = LoadLibraryA("scripts\\DirectImageRasterizer.III.asi");
            if (!hMod) hMod = LoadLibraryA("DirectImageRasterizer.VC.asi");
            if (!hMod) hMod = LoadLibraryA("DirectImageRasterizer.asi");
            if (!hMod) hMod = LoadLibraryA("DirectImageRasterizer.SA.asi");
            if (!hMod) hMod = LoadLibraryA("DirectImageRasterizer.III.asi");
        }
        return hMod;
    }

public:
    typedef RwTexture* (*LoadSVGToRwTextureFn)(const char*, uint32_t, uint32_t, bool, uint32_t);
    typedef RwTexture* (*LoadPNGToRwTextureFn)(const char*, uint32_t, uint32_t, bool, uint32_t);

    static RwTexture* LoadSVGToRwTexture(const char* filePath, uint32_t width, uint32_t height, bool generateMipmaps = true, uint32_t mipLevels = 0) {
        static LoadSVGToRwTextureFn fn = nullptr;
        if (!fn) {
            HMODULE hMod = GetModule();
            if (hMod) {
                fn = (LoadSVGToRwTextureFn)GetProcAddress(hMod, "LoadSVGToRwTexture");
            }
        }
        if (fn) {
            return fn(filePath, width, height, generateMipmaps, mipLevels);
        }
        return nullptr;
    }

    static RwTexture* LoadPNGToRwTexture(const char* filePath, uint32_t width = 0, uint32_t height = 0, bool generateMipmaps = true, uint32_t mipLevels = 0) {
        static LoadPNGToRwTextureFn fn = nullptr;
        if (!fn) {
            HMODULE hMod = GetModule();
            if (hMod) {
                fn = (LoadPNGToRwTextureFn)GetProcAddress(hMod, "LoadPNGToRwTexture");
            }
        }
        if (fn) {
            return fn(filePath, width, height, generateMipmaps, mipLevels);
        }
        return nullptr;
    }
};
