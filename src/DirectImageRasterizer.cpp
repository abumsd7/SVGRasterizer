#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <initguid.h>
#include <windows.h>
#include <shlwapi.h>
#include <wrl/client.h>

#include <d3d11.h>
#include <dxgi1_4.h>
#include <d2d1_3.h>
#include <d2d1_3helper.h>
#include <d2d1effects.h>

#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <unordered_set>

#include <wincodec.h>

#include "../includes/DirectImageRasterizer.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "windowscodecs.lib")

struct FilteredNodeInfo {
    std::wstring elementId;
    float stdDev;
};

struct FilteredNode {
    Microsoft::WRL::ComPtr<ID2D1SvgElement> element;
    float stdDev;
};

struct SavedElementState {
    Microsoft::WRL::ComPtr<ID2D1SvgElement> element;
    bool hadDisplayAttr;
    std::wstring oldDisplayVal;
};

static std::string ReadFileToString(const char* filePath) {
    std::ifstream file(filePath, std::ios::binary);
    std::stringstream buffer;
    if (!file.is_open()) return "";
    buffer << file.rdbuf();
    return buffer.str();
}

static std::vector<FilteredNodeInfo> ProcessSvgFilters(std::string& xmlContent) {
    std::map<std::string, float> filterMap;
    std::vector<FilteredNodeInfo> result;
    size_t filterPos, filterEnd, idPos, endId, blurPos, devPos, endDev, tagPos, tagEnd, autoIdCounter, spacePos;
    std::string filterBlock, filterId, devStr, tagBlock, elemIdStr, autoId, needle1, needle2, injected;
    float stdDev;
    bool foundFilter;

    filterPos = 0;
    while ((filterPos = xmlContent.find("<filter", filterPos)) != std::string::npos) {
        filterEnd = xmlContent.find("</filter>", filterPos);
        if (filterEnd == std::string::npos) break;

        filterBlock = xmlContent.substr(filterPos, filterEnd - filterPos + 9);

        filterId.clear();
        idPos = filterBlock.find("id=\"");
        if (idPos != std::string::npos) {
            idPos += 4;
            endId = filterBlock.find("\"", idPos);
            if (endId != std::string::npos) {
                filterId = filterBlock.substr(idPos, endId - idPos);
            }
        }

        stdDev = 0.0f;
        blurPos = filterBlock.find("feGaussianBlur");
        if (blurPos != std::string::npos) {
            devPos = filterBlock.find("stdDeviation=\"", blurPos);
            if (devPos != std::string::npos) {
                devPos += 14;
                endDev = filterBlock.find("\"", devPos);
                if (endDev != std::string::npos) {
                    devStr = filterBlock.substr(devPos, endDev - devPos);
                    stdDev = static_cast<float>(atof(devStr.c_str()));
                }
            }
        }

        if (!filterId.empty() && stdDev > 0.0f) {
            filterMap[filterId] = stdDev;
        }

        filterPos = filterEnd + 9;
    }

    if (filterMap.empty()) return result;

    autoIdCounter = 0;
    tagPos = 0;
    while ((tagPos = xmlContent.find("<", tagPos)) != std::string::npos) {
        if (xmlContent.compare(tagPos, 5, "<?xml") == 0 ||
            xmlContent.compare(tagPos, 4, "<!--") == 0 ||
            xmlContent.compare(tagPos, 5, "<defs") == 0 ||
            xmlContent.compare(tagPos, 7, "<filter") == 0 ||
            xmlContent.compare(tagPos, 2, "</") == 0) {
            tagPos++;
            continue;
        }

        tagEnd = xmlContent.find(">", tagPos);
        if (tagEnd == std::string::npos) break;

        tagBlock = xmlContent.substr(tagPos, tagEnd - tagPos + 1);

        foundFilter = false;
        stdDev = 0.0f;

        for (const auto& pair : filterMap) {
            needle1 = "filter:url(#" + pair.first + ")";
            needle2 = "filter=\"url(#" + pair.first + ")\"";
            if (tagBlock.find(needle1) != std::string::npos || tagBlock.find(needle2) != std::string::npos) {
                foundFilter = true;
                stdDev = pair.second;
                break;
            }
        }

        if (foundFilter) {
            elemIdStr.clear();
            idPos = tagBlock.find("id=\"");
            if (idPos != std::string::npos) {
                idPos += 4;
                endId = tagBlock.find("\"", idPos);
                if (endId != std::string::npos) {
                    elemIdStr = tagBlock.substr(idPos, endId - idPos);
                }
            } else {
                autoId = "ag_filter_elem_" + std::to_string(autoIdCounter++);
                elemIdStr = autoId;
                spacePos = tagBlock.find(' ');
                if (spacePos != std::string::npos) {
                    injected = " id=\"" + autoId + "\"";
                    xmlContent.insert(tagPos + spacePos, injected);
                    tagEnd += injected.size();
                }
            }

            if (!elemIdStr.empty()) {
                FilteredNodeInfo info;
                info.elementId = std::wstring(elemIdStr.begin(), elemIdStr.end());
                info.stdDev = stdDev;
                result.push_back(info);
            }
        }

        tagPos = tagEnd + 1;
    }

    return result;
}

static std::vector<SavedElementState> IsolateChild(const std::vector<Microsoft::WRL::ComPtr<ID2D1SvgElement>>& children, size_t activeIndex) {
    std::vector<SavedElementState> savedStates;
    wchar_t attrBuf[512];
    size_t j;

    for (j = 0; j < children.size(); j++) {
        if (j != activeIndex) {
            SavedElementState state;
            state.element = children[j];
            state.hadDisplayAttr = false;

            if (SUCCEEDED(children[j]->GetAttributeValue(L"display", D2D1_SVG_ATTRIBUTE_STRING_TYPE_SVG, attrBuf, 512))) {
                state.hadDisplayAttr = true;
                state.oldDisplayVal = attrBuf;
            }

            if (!state.hadDisplayAttr || state.oldDisplayVal != L"none") {
                children[j]->SetAttributeValue(L"display", D2D1_SVG_ATTRIBUTE_STRING_TYPE_SVG, L"none");
                savedStates.push_back(state);
            }
        }
    }
    return savedStates;
}

static void RestoreSavedElementStates(const std::vector<SavedElementState>& savedStates) {
    for (const auto& state : savedStates) {
        if (state.hadDisplayAttr) {
            state.element->SetAttributeValue(L"display", D2D1_SVG_ATTRIBUTE_STRING_TYPE_SVG, state.oldDisplayVal.c_str());
        } else {
            state.element->RemoveAttribute(L"display");
        }
    }
}

static void CollectLeafLayerElements(ID2D1SvgElement* parent, std::vector<Microsoft::WRL::ComPtr<ID2D1SvgElement>>& outElements) {
    Microsoft::WRL::ComPtr<ID2D1SvgElement> child, nextChild;
    wchar_t tagBuf[128];

    if (!parent) return;

    if (parent->HasChildren()) {
        parent->GetFirstChild(&child);
        while (child) {
            tagBuf[0] = L'\0';
            child->GetTagName(tagBuf, 128);

            if (wcscmp(tagBuf, L"defs") == 0) {
                /* Skip defs */
            } else if (wcscmp(tagBuf, L"g") == 0) {
                /* Recurse into group containers */
                CollectLeafLayerElements(child.Get(), outElements);
            } else {
                /* Individual layer elements (e.g. paths) */
                outElements.push_back(child);
            }

            parent->GetNextChild(child.Get(), &nextChild);
            child = nextChild;
        }
    }
}

static float GetFilterStdDevForChild(ID2D1SvgElement* child, const std::vector<FilteredNode>& filteredNodes) {
    size_t fnIdx;
    if (!child) return -1.0f;

    for (fnIdx = 0; fnIdx < filteredNodes.size(); fnIdx++) {
        if (!filteredNodes[fnIdx].element) continue;
        if (child == filteredNodes[fnIdx].element.Get()) {
            return filteredNodes[fnIdx].stdDev;
        }
    }
    return -1.0f;
}

static bool RenderSvgAtSize(
    ID3D11Device* d3dDevice,
    ID3D11DeviceContext* d3dCtx,
    ID2D1DeviceContext5* d2dCtx,
    ID2D1SvgDocument* svgDoc,
    ID2D1SvgElement* containerElem,
    const std::vector<FilteredNode>& filteredNodes,
    uint32_t renderW, uint32_t renderH,
    float scaleX, float scaleY,
    RwRaster* raster, RwUInt8 mipLevel)
{
    HRESULT hr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTex, stagingTex, offscreenTex;
    Microsoft::WRL::ComPtr<IDXGISurface> surface, offscreenSurface;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap, offscreenBitmap;
    Microsoft::WRL::ComPtr<ID2D1Effect> blurEffect;
    D3D11_TEXTURE2D_DESC rtDesc, stDesc;
    D2D1_BITMAP_PROPERTIES1 bmpProps;
    D3D11_MAPPED_SUBRESOURCE mapped;
    RwUInt8 *pixels, *srcRow, *dstRow;
    RwInt32 dstStride;
    uint32_t row;
    size_t k;
    float nodeStdDev, scaledStdDev;
    std::vector<Microsoft::WRL::ComPtr<ID2D1SvgElement>> layerChildren;
    std::vector<SavedElementState> savedStates;

    pixels = nullptr;
    srcRow = nullptr;
    dstRow = nullptr;

    /* Create render target texture at mip dimensions */
    memset(&rtDesc, 0, sizeof(rtDesc));
    rtDesc.Width = renderW;
    rtDesc.Height = renderH;
    rtDesc.MipLevels = 1;
    rtDesc.ArraySize = 1;
    rtDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    rtDesc.SampleDesc.Count = 1;
    rtDesc.Usage = D3D11_USAGE_DEFAULT;
    rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = d3dDevice->CreateTexture2D(&rtDesc, nullptr, &renderTex);
    if (FAILED(hr)) return false;

    hr = renderTex.As(&surface);
    if (FAILED(hr)) return false;

    memset(&bmpProps, 0, sizeof(bmpProps));
    bmpProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    bmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    bmpProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    hr = d2dCtx->CreateBitmapFromDxgiSurface(surface.Get(), &bmpProps, &bitmap);
    if (FAILED(hr)) return false;

    d2dCtx->SetTarget(bitmap.Get());
    d2dCtx->BeginDraw();
    d2dCtx->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    CollectLeafLayerElements(containerElem, layerChildren);

    if (filteredNodes.empty() || layerChildren.empty()) {
        /* Fast Path: Standard document rendering */
        d2dCtx->SetTransform(D2D1::Matrix3x2F::Scale(scaleX, scaleY));
        d2dCtx->DrawSvgDocument(svgDoc);
    } else {
        /* Exact In-Order Z-Pass Sandwich Rendering */
        for (k = 0; k < layerChildren.size(); k++) {
            savedStates = IsolateChild(layerChildren, k);
            nodeStdDev = GetFilterStdDevForChild(layerChildren[k].Get(), filteredNodes);

            if (nodeStdDev > 0.0f) {
                /* Filtered/Blurred Layer: Render offscreen -> GPU Gaussian Blur -> Blend onto canvas */
                hr = d3dDevice->CreateTexture2D(&rtDesc, nullptr, &offscreenTex);
                if (SUCCEEDED(hr) && SUCCEEDED(offscreenTex.As(&offscreenSurface))) {
                    bmpProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
                    hr = d2dCtx->CreateBitmapFromDxgiSurface(offscreenSurface.Get(), &bmpProps, &offscreenBitmap);
                    if (SUCCEEDED(hr)) {
                        d2dCtx->SetTarget(offscreenBitmap.Get());
                        d2dCtx->SetTransform(D2D1::Matrix3x2F::Scale(scaleX, scaleY));
                        d2dCtx->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
                        d2dCtx->DrawSvgDocument(svgDoc);

                        d2dCtx->SetTarget(bitmap.Get());
                        d2dCtx->SetTransform(D2D1::Matrix3x2F::Identity());

                        hr = d2dCtx->CreateEffect(CLSID_D2D1GaussianBlur, &blurEffect);
                        if (SUCCEEDED(hr)) {
                            scaledStdDev = nodeStdDev * scaleX * 4.0f;
                            blurEffect->SetInput(0, offscreenBitmap.Get());
                            blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, scaledStdDev);
                            blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_SOFT);
                            d2dCtx->DrawImage(blurEffect.Get());
                        }
                    }
                }
            } else {
                /* Unblurred Layer: Render crisp directly into main bitmap at current Z-index */
                d2dCtx->SetTarget(bitmap.Get());
                d2dCtx->SetTransform(D2D1::Matrix3x2F::Scale(scaleX, scaleY));
                d2dCtx->DrawSvgDocument(svgDoc);
            }

            RestoreSavedElementStates(savedStates);
        }
    }

    hr = d2dCtx->EndDraw();
    d2dCtx->SetTransform(D2D1::Matrix3x2F::Identity());
    d2dCtx->SetTarget(nullptr);

    if (FAILED(hr)) return false;

    /* Read back pixels via staging texture */
    memset(&stDesc, 0, sizeof(stDesc));
    stDesc.Width = renderW;
    stDesc.Height = renderH;
    stDesc.MipLevels = 1;
    stDesc.ArraySize = 1;
    stDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    stDesc.SampleDesc.Count = 1;
    stDesc.Usage = D3D11_USAGE_STAGING;
    stDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = d3dDevice->CreateTexture2D(&stDesc, nullptr, &stagingTex);
    if (FAILED(hr)) return false;

    d3dCtx->CopyResource(stagingTex.Get(), renderTex.Get());

    memset(&mapped, 0, sizeof(mapped));
    hr = d3dCtx->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return false;

    /* Write into the RwRaster mip level */
    pixels = RwRasterLock(raster, mipLevel, rwRASTERLOCKWRITE);
    if (pixels) {
        dstStride = raster->stride;
        for (row = 0; row < renderH; row++) {
            srcRow = static_cast<RwUInt8*>(mapped.pData) + row * mapped.RowPitch;
            dstRow = pixels + row * dstStride;
            memcpy(dstRow, srcRow, renderW * 4);
        }
        RwRasterUnlock(raster);
    }

    d3dCtx->Unmap(stagingTex.Get(), 0);
    return (pixels != nullptr);
}

static bool ReplaceStyleProp(std::string& t, const std::string& prop, const std::string& newVal) {
    size_t stylePos = t.find("style=\"");
    if (stylePos != std::string::npos) {
        size_t styleEnd = t.find("\"", stylePos + 7);
        if (styleEnd != std::string::npos) {
            std::string styleContent = t.substr(stylePos + 7, styleEnd - stylePos - 7);
            size_t propPos = styleContent.find(prop + ":");
            if (propPos != std::string::npos) {
                size_t propEnd = styleContent.find(";", propPos);
                if (propEnd == std::string::npos) propEnd = styleContent.size();
                styleContent.replace(propPos, propEnd - propPos, prop + ":" + newVal);
                t.replace(stylePos + 7, styleEnd - stylePos - 7, styleContent);
                return true;
            }
        }
    }
    return false;
}

static bool ReplaceAttr(std::string& t, const std::string& attr, const std::string& newVal) {
    size_t attrPos = t.find(" " + attr + "=\"");
    if (attrPos != std::string::npos) {
        size_t attrEnd = t.find("\"", attrPos + attr.size() + 3);
        if (attrEnd != std::string::npos) {
            t.replace(attrPos + attr.size() + 3, attrEnd - attrPos - attr.size() - 3, newVal);
            return true;
        }
    }
    return false;
}

static void PreprocessPaintOrder(std::string& xmlContent) {
    std::string result;
    size_t pos, nextTag, tagEnd, valStart, valEnd, strokeIdx, fillIdx, insertPos;
    bool isShape, hasPaintOrder, strokeTagModified, strokeAttrModified, fillTagModified;
    std::string tag, poVal, strokeTag, fillTag;
    std::vector<std::string> shapeTypes = { "<path", "<rect", "<circle", "<ellipse", "<line", "<polygon", "<polyline" };

    result.reserve(xmlContent.size() + 4096);
    pos = 0;
    while (pos < xmlContent.size()) {
        nextTag = xmlContent.find('<', pos);
        if (nextTag == std::string::npos) {
            result.append(xmlContent.substr(pos));
            break;
        }
        
        result.append(xmlContent.substr(pos, nextTag - pos));
        
        tagEnd = xmlContent.find('>', nextTag);
        if (tagEnd == std::string::npos) {
            result.append(xmlContent.substr(nextTag));
            break;
        }
        
        tag = xmlContent.substr(nextTag, tagEnd - nextTag + 1);
        pos = tagEnd + 1;
        
        isShape = false;
        for (const auto& type : shapeTypes) {
            if (tag.compare(0, type.size(), type) == 0 && (tag.size() > type.size())) {
                char nextChar = tag[type.size()];
                if (nextChar == ' ' || nextChar == '\n' || nextChar == '\r' || nextChar == '\t' || nextChar == '/' || nextChar == '>') {
                    isShape = true;
                    break;
                }
            }
        }
        
        if (!isShape) {
            result.append(tag);
            continue;
        }
        
        hasPaintOrder = false;
        size_t poPos = tag.find("paint-order");
        if (poPos != std::string::npos) {
            valStart = tag.find_first_of("=:", poPos);
            if (valStart != std::string::npos) {
                size_t searchStart = valStart + 1;
                while (searchStart < tag.size() && (tag[searchStart] == ' ' || tag[searchStart] == '"' || tag[searchStart] == '\'')) {
                    searchStart++;
                }
                valEnd = tag.find_first_of("\";>", searchStart);
                if (valEnd != std::string::npos) {
                    poVal = tag.substr(searchStart, valEnd - searchStart);
                    strokeIdx = poVal.find("stroke");
                    fillIdx = poVal.find("fill");
                    if (strokeIdx != std::string::npos && fillIdx != std::string::npos && strokeIdx < fillIdx) {
                        hasPaintOrder = true;
                    }
                }
            }
        }
        
        if (!hasPaintOrder) {
            result.append(tag);
            continue;
        }
        
        strokeTag = tag;
        fillTag = tag;
        
        strokeTagModified = ReplaceStyleProp(strokeTag, "fill", "none");
        if (!strokeTagModified) {
            strokeAttrModified = ReplaceAttr(strokeTag, "fill", "none");
            if (!strokeAttrModified) {
                insertPos = strokeTag.find_first_of(" />");
                if (insertPos != std::string::npos) {
                    strokeTag.insert(insertPos, " fill=\"none\"");
                }
            }
        }
        
        // Append "_stroke" to ID in strokeTag to avoid duplicate IDs in SVG document
        size_t idPos = strokeTag.find("id=\"");
        if (idPos != std::string::npos) {
            size_t idEnd = strokeTag.find("\"", idPos + 4);
            if (idEnd != std::string::npos) {
                std::string originalId = strokeTag.substr(idPos + 4, idEnd - idPos - 4);
                strokeTag.replace(idPos + 4, idEnd - idPos - 4, originalId + "_stroke");
            }
        }
        
        fillTagModified = ReplaceStyleProp(fillTag, "stroke", "none");
        if (!fillTagModified) {
            ReplaceAttr(fillTag, "stroke", "none");
        }
        
        result.append(strokeTag);
        result.append(fillTag);
    }
    xmlContent = result;
}

static float ParseSvgLength(const std::string& str) {
    size_t i;
    float val;
    std::string unit;

    i = 0;
    val = 0.0f;

    if (str.empty()) return 0.0f;

    while (i < str.size() && (str[i] == ' ' || str[i] == '\t' || str[i] == '\r' || str[i] == '\n')) {
        i++;
    }

    val = static_cast<float>(atof(str.c_str() + i));
    if (val <= 0.0f) return 0.0f;

    while (i < str.size() && ((str[i] >= '0' && str[i] <= '9') || str[i] == '.' || str[i] == '-' || str[i] == '+')) {
        i++;
    }
    while (i < str.size() && str[i] != ' ' && str[i] != ';' && str[i] != '"' && str[i] != '\'') {
        unit += str[i++];
    }

    if (unit == "mm") {
        return val * 3.779527559f;
    } else if (unit == "cm") {
        return val * 37.79527559f;
    } else if (unit == "in") {
        return val * 96.0f;
    } else if (unit == "pt") {
        return val * 1.333333333f;
    } else if (unit == "pc") {
        return val * 16.0f;
    }

    return val;
}

static void GetSvgDimensions(const std::string& xmlContent, uint32_t& outWidth, uint32_t& outHeight) {
    size_t svgTagPos, tagEnd, widthPos, heightPos, vbPos, quoteStart, quoteEnd, cIdx;
    std::string svgTag, widthStr, heightStr, vbStr;
    float parsedW, parsedH, vbMinX, vbMinY, vbW, vbH;
    int scanned;

    outWidth = 0;
    outHeight = 0;
    parsedW = 0.0f;
    parsedH = 0.0f;
    vbMinX = 0.0f;
    vbMinY = 0.0f;
    vbW = 0.0f;
    vbH = 0.0f;
    scanned = 0;

    svgTagPos = xmlContent.find("<svg");
    if (svgTagPos == std::string::npos) return;

    tagEnd = xmlContent.find('>', svgTagPos);
    if (tagEnd == std::string::npos) return;

    svgTag = xmlContent.substr(svgTagPos, tagEnd - svgTagPos + 1);

    widthPos = svgTag.find("width=\"");
    if (widthPos != std::string::npos) {
        quoteStart = widthPos + 7;
        quoteEnd = svgTag.find('"', quoteStart);
        if (quoteEnd != std::string::npos) {
            widthStr = svgTag.substr(quoteStart, quoteEnd - quoteStart);
            parsedW = ParseSvgLength(widthStr);
        }
    }

    heightPos = svgTag.find("height=\"");
    if (heightPos != std::string::npos) {
        quoteStart = heightPos + 8;
        quoteEnd = svgTag.find('"', quoteStart);
        if (quoteEnd != std::string::npos) {
            heightStr = svgTag.substr(quoteStart, quoteEnd - quoteStart);
            parsedH = ParseSvgLength(heightStr);
        }
    }

    vbPos = svgTag.find("viewBox=\"");
    if (vbPos != std::string::npos) {
        quoteStart = vbPos + 9;
        quoteEnd = svgTag.find('"', quoteStart);
        if (quoteEnd != std::string::npos) {
            vbStr = svgTag.substr(quoteStart, quoteEnd - quoteStart);
            for (cIdx = 0; cIdx < vbStr.size(); cIdx++) {
                if (vbStr[cIdx] == ',') vbStr[cIdx] = ' ';
            }
            scanned = sscanf_s(vbStr.c_str(), "%f %f %f %f", &vbMinX, &vbMinY, &vbW, &vbH);
            if (scanned == 4 && vbW > 0.0f && vbH > 0.0f) {
                if (parsedW <= 0.0f) parsedW = vbW;
                if (parsedH <= 0.0f) parsedH = vbH;
            }
        }
    }

    if (parsedW > 0.0f && parsedH <= 0.0f) {
        if (vbW > 0.0f && vbH > 0.0f) {
            parsedH = parsedW * (vbH / vbW);
        } else {
            parsedH = parsedW;
        }
    } else if (parsedH > 0.0f && parsedW <= 0.0f) {
        if (vbW > 0.0f && vbH > 0.0f) {
            parsedW = parsedH * (vbW / vbH);
        } else {
            parsedW = parsedH;
        }
    }

    if (parsedW <= 0.0f) parsedW = 512.0f;
    if (parsedH <= 0.0f) parsedH = 512.0f;

    outWidth = static_cast<uint32_t>(ceilf(parsedW));
    outHeight = static_cast<uint32_t>(ceilf(parsedH));
}

RwTexture* DirectImageRasterizer::LoadSVGToRwTexture(const char* filePath, uint32_t width, uint32_t height, bool generateMipmaps, uint32_t mipLevels) {
    HRESULT hr;
    D3D_FEATURE_LEVEL featureLevels[2];
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    Microsoft::WRL::ComPtr<ID2D1Factory3> d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device2> d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext2> d2dContext2;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext5> d2dContext;
    Microsoft::WRL::ComPtr<IStream> stream;
    Microsoft::WRL::ComPtr<ID2D1SvgDocument> svgDoc;
    Microsoft::WRL::ComPtr<ID2D1SvgElement> rootElem, elem;
    D2D1_SIZE_F svgSize;
    RwRaster* raster;
    RwTexture* texture;
    uint32_t targetW, targetH, autoW, autoH, level, numMipLevels, maxDim, maxLevels, mipW, mipH;
    float scaleX, scaleY;
    bool mipFailed;
    RwInt32 rasterFlags;
    std::string xmlContent;
    std::vector<FilteredNodeInfo> filterInfos;
    std::vector<FilteredNode> filteredNodes;
    size_t k;

    raster = nullptr;
    texture = nullptr;
    targetW = 0;
    targetH = 0;
    autoW = 0;
    autoH = 0;
    level = 0;
    numMipLevels = 1;
    maxDim = 0;
    maxLevels = 1;
    mipW = 0;
    mipH = 0;
    scaleX = 1.0f;
    scaleY = 1.0f;
    rasterFlags = 0;
    mipFailed = false;

    if (!filePath || filePath[0] == '\0') {
        return nullptr;
    }

    xmlContent = ReadFileToString(filePath);
    if (xmlContent.empty()) return nullptr;

    targetW = width;
    targetH = height;
    if (targetW == 0 || targetH == 0) {
        GetSvgDimensions(xmlContent, autoW, autoH);
        if (targetW == 0) targetW = autoW;
        if (targetH == 0) targetH = autoH;
    }

    if (targetW == 0 || targetH == 0) {
        return nullptr;
    }

    PreprocessPaintOrder(xmlContent);

    filterInfos = ProcessSvgFilters(xmlContent);

    /* --- D3D11 WARP device --- */
    featureLevels[0] = D3D_FEATURE_LEVEL_11_1;
    featureLevels[1] = D3D_FEATURE_LEVEL_11_0;

    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_WARP,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        featureLevels,
        2,
        D3D11_SDK_VERSION,
        &d3d11Device,
        nullptr,
        &d3d11Context
    );
    if (FAILED(hr)) return nullptr;

    /* --- D2D factory + device + context --- */
    hr = d3d11Device.As(&dxgiDevice);
    if (FAILED(hr)) return nullptr;

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3), nullptr, &d2dFactory);
    if (FAILED(hr)) return nullptr;

    hr = d2dFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice);
    if (FAILED(hr)) return nullptr;

    hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dContext2);
    if (FAILED(hr)) return nullptr;

    hr = d2dContext2.As(&d2dContext);
    if (FAILED(hr)) return nullptr;

    d2dContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    /* --- Create memory stream from processed XML string --- */
    stream.Attach(SHCreateMemStream((const BYTE*)xmlContent.data(), (UINT)xmlContent.size()));
    if (!stream) return nullptr;

    svgSize.width = static_cast<float>(targetW);
    svgSize.height = static_cast<float>(targetH);

    hr = d2dContext->CreateSvgDocument(stream.Get(), svgSize, &svgDoc);
    if (FAILED(hr)) return nullptr;

    svgDoc->GetRoot(&rootElem);

    /* Find all ID2D1SvgElement pointers for filtered nodes */
    for (k = 0; k < filterInfos.size(); k++) {
        elem.Reset();
        svgDoc->FindElementById(filterInfos[k].elementId.c_str(), &elem);
        if (elem) {
            FilteredNode node;
            node.element = elem;
            node.stdDev = filterInfos[k].stdDev;
            filteredNodes.push_back(node);
        }
    }

    /* --- Calculate mip level count --- */
    if (!generateMipmaps) {
        numMipLevels = 1;
        rasterFlags = rwRASTERTYPETEXTURE | rwRASTERFORMAT8888;
    } else {
        maxLevels = 1;
        maxDim = (targetW > targetH) ? targetW : targetH;
        while (maxDim > 1) {
            maxDim >>= 1;
            maxLevels++;
        }
        numMipLevels = (mipLevels == 0 || mipLevels > maxLevels) ? maxLevels : mipLevels;
        rasterFlags = (numMipLevels > 1) ? (rwRASTERTYPETEXTURE | rwRASTERFORMAT8888 | rwRASTERFORMATMIPMAP)
                                         : (rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    }

    /* --- Create RW raster --- */
    raster = RwRasterCreate(targetW, targetH, 32, rasterFlags);
    if (!raster) return nullptr;

    /* --- Re-rasterize SVG fresh at each mip level --- */
    for (level = 0; level < numMipLevels; level++) {
        mipW = targetW >> level;
        if (mipW < 1) mipW = 1;
        mipH = targetH >> level;
        if (mipH < 1) mipH = 1;

        scaleX = static_cast<float>(mipW) / static_cast<float>(targetW);
        scaleY = static_cast<float>(mipH) / static_cast<float>(targetH);

        if (!RenderSvgAtSize(d3d11Device.Get(), d3d11Context.Get(), d2dContext.Get(),
                             svgDoc.Get(), rootElem.Get(), filteredNodes, mipW, mipH, scaleX, scaleY,
                             raster, static_cast<RwUInt8>(level))) {
            mipFailed = true;
            break;
        }
    }

    texture = RwTextureCreate(raster);
    if (texture) {
        RwTextureSetFilterMode(texture, (mipFailed || numMipLevels <= 1) ? rwFILTERLINEAR : rwFILTERLINEARMIPLINEAR);
    }

    return texture;
}

static IWICImagingFactory* GetWICFactory() {
    static IWICImagingFactory* pFactory = nullptr;
    HRESULT hr;

    if (!pFactory) {
        CoInitialize(nullptr);
        hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&pFactory)
        );
        if (FAILED(hr)) {
            pFactory = nullptr;
        }
    }
    return pFactory;
}

RwTexture* DirectImageRasterizer::LoadPNGToRwTexture(const char* filePath, uint32_t width, uint32_t height, bool generateMipmaps, uint32_t mipLevels) {
    HRESULT hr;
    IWICImagingFactory* factory;
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
    IWICBitmapSource* currentSource;
    RwRaster* raster;
    RwTexture* texture;
    RwUInt8 *pixels, *dstRow;
    RwInt32 dstStride, rasterFlags;
    UINT origW, origH;
    uint32_t targetW, targetH, numMipLevels, maxDim, maxLevels, level, mipW, mipH, row;
    bool mipFailed;
    wchar_t wFilePath[MAX_PATH];
    WICRect rect;

    factory = nullptr;
    currentSource = nullptr;
    raster = nullptr;
    texture = nullptr;
    pixels = nullptr;
    dstRow = nullptr;
    origW = 0;
    origH = 0;
    targetW = 0;
    targetH = 0;
    numMipLevels = 1;
    maxDim = 0;
    maxLevels = 1;
    level = 0;
    mipW = 0;
    mipH = 0;
    row = 0;
    dstStride = 0;
    rasterFlags = 0;
    mipFailed = false;

    if (!filePath || filePath[0] == '\0') {
        return nullptr;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH) == 0) {
        if (MultiByteToWideChar(CP_ACP, 0, filePath, -1, wFilePath, MAX_PATH) == 0) {
            return nullptr;
        }
    }

    factory = GetWICFactory();
    if (!factory) return nullptr;

    hr = factory->CreateDecoderFromFilename(
        wFilePath,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder
    );
    if (FAILED(hr)) return nullptr;

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return nullptr;

    hr = frame->GetSize(&origW, &origH);
    if (FAILED(hr) || origW == 0 || origH == 0) return nullptr;

    targetW = (width > 0) ? width : origW;
    targetH = (height > 0) ? height : origH;

    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return nullptr;

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) return nullptr;

    /* Calculate mip level count */
    if (!generateMipmaps) {
        numMipLevels = 1;
        rasterFlags = rwRASTERTYPETEXTURE | rwRASTERFORMAT8888;
    } else {
        maxLevels = 1;
        maxDim = (targetW > targetH) ? targetW : targetH;
        while (maxDim > 1) {
            maxDim >>= 1;
            maxLevels++;
        }
        numMipLevels = (mipLevels == 0 || mipLevels > maxLevels) ? maxLevels : mipLevels;
        rasterFlags = (numMipLevels > 1) ? (rwRASTERTYPETEXTURE | rwRASTERFORMAT8888 | rwRASTERFORMATMIPMAP)
                                         : (rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    }

    raster = RwRasterCreate(targetW, targetH, 32, rasterFlags);
    if (!raster) return nullptr;

    for (level = 0; level < numMipLevels; level++) {
        mipW = targetW >> level;
        if (mipW < 1) mipW = 1;
        mipH = targetH >> level;
        if (mipH < 1) mipH = 1;

        if (level == 0 && targetW == origW && targetH == origH) {
            currentSource = converter.Get();
        } else {
            scaler.Reset();
            hr = factory->CreateBitmapScaler(&scaler);
            if (FAILED(hr)) {
                mipFailed = true;
                break;
            }

            hr = scaler->Initialize(converter.Get(), mipW, mipH, WICBitmapInterpolationModeFant);
            if (FAILED(hr)) {
                mipFailed = true;
                break;
            }
            currentSource = scaler.Get();
        }

        pixels = RwRasterLock(raster, static_cast<RwUInt8>(level), rwRASTERLOCKWRITE);
        if (!pixels) {
            mipFailed = true;
            break;
        }

        dstStride = raster->stride;
        if (dstStride == static_cast<RwInt32>(mipW * 4)) {
            rect.X = 0;
            rect.Y = 0;
            rect.Width = mipW;
            rect.Height = mipH;
            hr = currentSource->CopyPixels(&rect, dstStride, dstStride * mipH, pixels);
            if (FAILED(hr)) {
                mipFailed = true;
            }
        } else {
            for (row = 0; row < mipH; row++) {
                rect.X = 0;
                rect.Y = row;
                rect.Width = mipW;
                rect.Height = 1;
                dstRow = pixels + row * dstStride;
                hr = currentSource->CopyPixels(&rect, mipW * 4, mipW * 4, dstRow);
                if (FAILED(hr)) {
                    mipFailed = true;
                    break;
                }
            }
        }

        RwRasterUnlock(raster);
        if (mipFailed) break;
    }

    texture = RwTextureCreate(raster);
    if (texture) {
        RwTextureSetFilterMode(texture, (mipFailed || numMipLevels <= 1) ? rwFILTERLINEAR : rwFILTERLINEARMIPLINEAR);
    }

    return texture;
}

RwTexture* DirectImageRasterizer::FindSVGinFolderPath(const char* folderPath, const char* fileName, uint32_t width, uint32_t height, bool generateMipmaps, uint32_t mipLevels) {
    std::string fullPath;
    size_t len;

    if (!folderPath || !fileName || folderPath[0] == '\0' || fileName[0] == '\0') {
        return nullptr;
    }

    fullPath = folderPath;
    if (!fullPath.empty() && fullPath.back() != '\\' && fullPath.back() != '/') {
        fullPath += '\\';
    }
    fullPath += fileName;

    len = fullPath.size();
    if (len < 4 || (_stricmp(fullPath.c_str() + len - 4, ".svg") != 0)) {
        fullPath += ".svg";
    }

    return LoadSVGToRwTexture(fullPath.c_str(), width, height, generateMipmaps, mipLevels);
}

RwTexture* DirectImageRasterizer::FindPNGinFolderPath(const char* folderPath, const char* fileName, uint32_t width, uint32_t height, bool generateMipmaps, uint32_t mipLevels) {
    std::string fullPath;
    size_t len;

    if (!folderPath || !fileName || folderPath[0] == '\0' || fileName[0] == '\0') {
        return nullptr;
    }

    fullPath = folderPath;
    if (!fullPath.empty() && fullPath.back() != '\\' && fullPath.back() != '/') {
        fullPath += '\\';
    }
    fullPath += fileName;

    len = fullPath.size();
    if (len < 4 || (_stricmp(fullPath.c_str() + len - 4, ".png") != 0)) {
        fullPath += ".png";
    }

    return LoadPNGToRwTexture(fullPath.c_str(), width, height, generateMipmaps, mipLevels);
}



