# DirectImageRasterizer

Dynamic DirectX & WIC image rasterizer plugin for GTA III, Vice City, and San Andreas with full mipmapping support.

## Supported Formats
- **SVG**: Direct2D vector rasterization with dynamic Gaussian blur filters and layered compositing.
- **PNG / Images (JPG, BMP, TIFF, WebP)**: Native Windows Imaging Component (WIC) decoding with high-quality Fant downsampling and full mipmap chain generation.

## How to use:
- Build the project (`DirectImageRasterizer.sln`).
- Drop the `.asi` in the `scripts` folder.
- Include the header file from `/APIHeader/DirectImageRasterizer.h` and call:
  - `DirectImageRasterizer::LoadSVGToRwTexture(filePath, width, height)`
  - `DirectImageRasterizer::LoadPNGToRwTexture(filePath, width, height)`
