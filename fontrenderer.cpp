#include "fontrenderer.h"

/*
Render a single line of text
free floating - no background
*/
void FontRenderer::addMeshForLabel(FontMesh& result, const char* label, const glm::vec2& origin) {
    int numChars = strlen(label);
    size_t curIndex = result.vert.size();
    glm::vec2 curOrigin = origin;
    for (int i = 0; i < numChars; i++) {
        if (label[i] == '\n') {
            curOrigin.x = origin.x;
            curOrigin.y += glyphHeightScreen;
            continue;
        }
        glm::vec2 curEndpoint(curOrigin.x + glyphWidthScreen, curOrigin.y + glyphHeightScreen);

        curIndex = result.vert.size();

        glm::vec2 curCharTexCoord = texCoordForGlyph(label[i]);

        result.vert.push_back({curOrigin, curCharTexCoord});
        result.vert.push_back({{curEndpoint.x, curOrigin.y}, {curCharTexCoord.x + GLYPH_WIDTH_TEXCOORDS, curCharTexCoord.y}});
        result.vert.push_back({curEndpoint, {curCharTexCoord.x + GLYPH_WIDTH_TEXCOORDS, curCharTexCoord.y + GLYPH_WIDTH_TEXCOORDS}});
        result.vert.push_back({{curOrigin.x, curEndpoint.y}, {curCharTexCoord.x, curCharTexCoord.y + GLYPH_WIDTH_TEXCOORDS}});

        result.ind.push_back(curIndex);
        result.ind.push_back(curIndex + 1);
        result.ind.push_back(curIndex + 2);
        result.ind.push_back(curIndex + 2);
        result.ind.push_back(curIndex + 3);
        result.ind.push_back(curIndex);
        curOrigin.x += glyphWidthScreen;
    }
}

/*
Render a solid-color quad
*/
void FontRenderer::addMeshForBG(FontMesh& result, const glm::vec2& uv, const glm::vec2& minExtent, const glm::vec2& maxExtent) {
    size_t curIndex = result.vert.size();

    result.vert.push_back({minExtent, uv});
    result.vert.push_back({{maxExtent.x, minExtent.y}, uv});
    result.vert.push_back({maxExtent, uv});
    result.vert.push_back({{minExtent.x, maxExtent.y}, uv});

    result.ind.push_back(curIndex);
    result.ind.push_back(curIndex + 1);
    result.ind.push_back(curIndex + 2);
    result.ind.push_back(curIndex + 2);
    result.ind.push_back(curIndex + 3);
    result.ind.push_back(curIndex);
}
