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
    typedef RwTexture* (*FindSVGinFolderPathFn)(const char*, const char*, uint32_t, uint32_t, bool, uint32_t);
    typedef RwTexture* (*FindPNGinFolderPathFn)(const char*, const char*, uint32_t, uint32_t, bool, uint32_t);

    static bool IsLoaded() {
        return GetModule() != nullptr;
    }

    static void EnsureLoaded() {
        if (!IsLoaded()) {
            MessageBoxA(nullptr,
                "DirectImageRasterizer plugin is required but could not be loaded.\n\n"
                "Please ensure 'DirectImageRasterizer.VC.asi' (or 'DirectImageRasterizer.asi') is installed in your game's scripts folder.",
                "Missing Dependency Error",
                MB_ICONERROR | MB_OK);
            ExitProcess(1);
        }
    }

    static RwTexture* LoadSVGToRwTexture(const char* filePath, uint32_t width = 0, uint32_t height = 0, bool generateMipmaps = true, uint32_t mipLevels = 0) {
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
        EnsureLoaded();
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
        EnsureLoaded();
        return nullptr;
    }

    static RwTexture* FindSVGinFolderPath(const char* folderPath, const char* fileName, uint32_t width = 0, uint32_t height = 0, bool generateMipmaps = true, uint32_t mipLevels = 0) {
        static FindSVGinFolderPathFn fn = nullptr;
        if (!fn) {
            HMODULE hMod = GetModule();
            if (hMod) {
                fn = (FindSVGinFolderPathFn)GetProcAddress(hMod, "FindSVGinFolderPath");
            }
        }
        if (fn) {
            return fn(folderPath, fileName, width, height, generateMipmaps, mipLevels);
        }
        EnsureLoaded();
        return nullptr;
    }

    static RwTexture* FindPNGinFolderPath(const char* folderPath, const char* fileName, uint32_t width = 0, uint32_t height = 0, bool generateMipmaps = true, uint32_t mipLevels = 0) {
        static FindPNGinFolderPathFn fn = nullptr;
        if (!fn) {
            HMODULE hMod = GetModule();
            if (hMod) {
                fn = (FindPNGinFolderPathFn)GetProcAddress(hMod, "FindPNGinFolderPath");
            }
        }
        if (fn) {
            return fn(folderPath, fileName, width, height, generateMipmaps, mipLevels);
        }
        EnsureLoaded();
        return nullptr;
    }
};
