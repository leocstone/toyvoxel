#ifndef FONTRENDERER_H
#define FONTRENDERER_H
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "lib/stb_image.h"
#include <vector>
#include <cstring>

static bool isPrintableAscii(char c) {
    return (c >= 32 && c <= 126);
}

struct Vertex {
    glm::vec2 pos;
    glm::vec2 texCoord;
};

struct FontMesh {
    std::vector<Vertex> vert = {};
    std::vector<uint16_t> ind = {};
};

struct GlyphBounds {
    int minX;
    int maxX;
};

struct RGBAPixel {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

/** Constants **/
constexpr int FONT_WIDTH_GLYPHS = 16;
constexpr int glyphWidthPixels = 8;
constexpr int glyphHeightPixels = 9;
constexpr int rowSizePixels = FONT_WIDTH_GLYPHS * glyphWidthPixels;
constexpr double GLYPH_WIDTH_TEXCOORDS = 1.0 / double(FONT_WIDTH_GLYPHS);
constexpr double CONSOLE_MARGIN = 0.005;

/* Information for the console background colors - stored in font */
constexpr int FONT_BG_INDEX = 129;
constexpr double FONT_BG_X = double(FONT_BG_INDEX % FONT_WIDTH_GLYPHS) * GLYPH_WIDTH_TEXCOORDS + 0.5f * GLYPH_WIDTH_TEXCOORDS;
constexpr double FONT_BG_Y = double(FONT_BG_INDEX / FONT_WIDTH_GLYPHS) * GLYPH_WIDTH_TEXCOORDS + 0.5f * GLYPH_WIDTH_TEXCOORDS;
constexpr glm::vec2 FONT_BG_UV(FONT_BG_X, FONT_BG_Y);

constexpr int FONT_BG2_INDEX = 141;
constexpr double FONT_BG2_X = double(FONT_BG2_INDEX % FONT_WIDTH_GLYPHS) * GLYPH_WIDTH_TEXCOORDS + 0.5f * GLYPH_WIDTH_TEXCOORDS;
constexpr double FONT_BG2_Y = double(FONT_BG2_INDEX / FONT_WIDTH_GLYPHS) * GLYPH_WIDTH_TEXCOORDS + 0.5f * GLYPH_WIDTH_TEXCOORDS;
constexpr glm::vec2 FONT_BG2_UV(FONT_BG2_X, FONT_BG2_Y);

/**
Simple bitmap font rendering
Monospace font
**/
class FontRenderer
{
public:
    FontRenderer() {}
    ~FontRenderer() {}

    void updateResolution(int x, int y) {
        screenWidth = x;
        screenHeight = y;
        curAspect = double(screenHeight) / double(screenWidth);
        glyphHeightScreen = ((double(glyphHeightPixels) / double(screenHeight)) * glyphHeightPixels);
        glyphWidthScreen = (((double(glyphWidthPixels) / double(screenWidth)) * glyphWidthPixels) * curAspect);
    }

    void addMeshForLabel(FontMesh& result, const char* label, const glm::vec2& origin);
    void addMeshForBG(FontMesh& result, const glm::vec2& uv, const glm::vec2& minExtent, const glm::vec2& maxExtent);

    double getGlyphWidthScreen() { return glyphWidthScreen; }
    double getGlyphHeightScreen() { return glyphHeightScreen; }

private:
    int screenWidth;
    int screenHeight;
    double curAspect;
    double glyphHeightScreen;
    double glyphWidthScreen;

    int fontWidthPixels;
    int fontHeightPixels;

    /* Returns UV of the top left of the given glyph */
    glm::vec2 texCoordForGlyph(char glyph) {
        int curCharX = glyph % FONT_WIDTH_GLYPHS;
        int curCharY = glyph / FONT_WIDTH_GLYPHS;
        return glm::vec2(double(curCharX) * GLYPH_WIDTH_TEXCOORDS,
                         double(curCharY) * GLYPH_WIDTH_TEXCOORDS);
    }
};

#endif // FONTRENDERER_H
