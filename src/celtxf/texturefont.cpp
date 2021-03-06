// texturefont.cpp
//
// Copyright (C) 2001, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include <cassert>
#include <cstring>
#include <fstream>
#include <config.h>
#include <celutil/debug.h>
#include <celutil/bytes.h>
#include <celutil/utf8.h>
#include <celutil/util.h>
#include <celengine/glsupport.h>
#include <celengine/render.h>
#include "texturefont.h"

using namespace std;

TextureFont::TextureFont(const Renderer *r) :
    renderer(r)
{
}

TextureFont::~TextureFont()
{
    if (texName != 0)
        glDeleteTextures(1, (const GLuint*) &texName);

    delete[] fontImage;
    delete[] glyphLookup;
}


struct FontVertex
{
    FontVertex(float _x, float _y, float _u, float _v) :
        x(_x), y(_y), u(_u), v(_v)
    {}
    float x, y;
    float u, v;
};

/** Render a single character of the font. The modelview transform is
 *  automatically updated to advance to the next character.
 */
void TextureFont::render(wchar_t ch) const
{
    const Glyph* glyph = getGlyph(ch);
    if (glyph == nullptr) glyph = getGlyph((wchar_t)'?');
    if (glyph != nullptr)
    {
        const float x1 = glyph->xoff;
        const float y1 = glyph->yoff;
        const float x2 = glyph->xoff + glyph->width;
        const float y2 = glyph->yoff + glyph->height;
        FontVertex vertices[4] = {
            {x1, y1, glyph->texCoords[0].u, glyph->texCoords[0].v},
            {x2, y1, glyph->texCoords[1].u, glyph->texCoords[1].v},
            {x2, y2, glyph->texCoords[2].u, glyph->texCoords[2].v},
            {x1, y2, glyph->texCoords[3].u, glyph->texCoords[3].v}
        };
        glEnableVertexAttribArray(CelestiaGLProgram::VertexCoordAttributeIndex);
        glEnableVertexAttribArray(CelestiaGLProgram::TextureCoord0AttributeIndex);
        glVertexAttribPointer(CelestiaGLProgram::VertexCoordAttributeIndex,
                              2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), &vertices[0].x);
        glVertexAttribPointer(CelestiaGLProgram::TextureCoord0AttributeIndex,
                              2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), &vertices[0].u);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDisableVertexAttribArray(CelestiaGLProgram::VertexCoordAttributeIndex);
        glDisableVertexAttribArray(CelestiaGLProgram::TextureCoord0AttributeIndex);
        glTranslatef(glyph->advance, 0.0f, 0.0f);
    }
}


/** Render a single character of the font, adding the specified offset
 *  to the location.
 */
void TextureFont::render(wchar_t ch, float xoffset, float yoffset) const
{
    const Glyph* glyph = getGlyph(ch);
    if (glyph == nullptr) glyph = getGlyph((wchar_t)'?');
    if (glyph != nullptr)
    {
        const float x1 = glyph->xoff + xoffset;
        const float y1 = glyph->yoff + yoffset;
        const float x2 = glyph->xoff + glyph->width + xoffset;
        const float y2 = glyph->yoff + glyph->height + yoffset;
        FontVertex vertices[4] = {
            {x1, y1, glyph->texCoords[0].u, glyph->texCoords[0].v},
            {x2, y1, glyph->texCoords[1].u, glyph->texCoords[1].v},
            {x2, y2, glyph->texCoords[2].u, glyph->texCoords[2].v},
            {x1, y2, glyph->texCoords[3].u, glyph->texCoords[3].v}
        };
        glEnableVertexAttribArray(CelestiaGLProgram::VertexCoordAttributeIndex);
        glEnableVertexAttribArray(CelestiaGLProgram::TextureCoord0AttributeIndex);
        glVertexAttribPointer(CelestiaGLProgram::VertexCoordAttributeIndex,
                              2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), &vertices[0].x);
        glVertexAttribPointer(CelestiaGLProgram::TextureCoord0AttributeIndex,
                              2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), &vertices[0].u);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDisableVertexAttribArray(CelestiaGLProgram::VertexCoordAttributeIndex);
        glDisableVertexAttribArray(CelestiaGLProgram::TextureCoord0AttributeIndex);
    }
}


/** Render a string and automatically update the modelview transform for the
  * string width.
  */
void TextureFont::render(const string& s) const
{
    int len = s.length();
    bool validChar = true;
    int i = 0;

    float xoffset = 0.0f;

    while (i < len && validChar) {
        wchar_t ch = 0;
        validChar = UTF8Decode(s, i, ch);
        i += UTF8EncodedSize(ch);

        render(ch, xoffset, 0.0f);

        const Glyph* glyph = getGlyph(ch);
        if (glyph == nullptr)
            glyph = getGlyph((wchar_t)'?');
        xoffset += glyph->advance;
    }

    glTranslatef(xoffset, 0.0f, 0.0f);
}


/** Render a string with the specified offset. Do *not* automatically update
 *  the modelview transform.
 */
void TextureFont::render(const string& s, float xoffset, float yoffset) const
{
    int len = s.length();
    bool validChar = true;
    int i = 0;

    while (i < len && validChar) {
        wchar_t ch = 0;
        validChar = UTF8Decode(s, i, ch);
        i += UTF8EncodedSize(ch);

        render(ch, xoffset, yoffset);

        const Glyph* glyph = getGlyph(ch);
        if (glyph == nullptr)
            glyph = getGlyph((wchar_t)'?');
        xoffset += glyph->advance;
    }
}


int TextureFont::getWidth(const string& s) const
{
    int width = 0;
    int len = s.length();
    bool validChar = true;
    int i = 0;

    while (i < len && validChar)
    {
        wchar_t ch = 0;
        validChar = UTF8Decode(s, i, ch);
        i += UTF8EncodedSize(ch);

        const Glyph* g = getGlyph(ch);
        if (g != nullptr)
            width += g->advance;
    }

    return width;
}


int TextureFont::getHeight() const
{
    return maxAscent + maxDescent;
}

int TextureFont::getMaxWidth() const
{
    return maxWidth;
}

int TextureFont::getMaxAscent() const
{
    return maxAscent;
}

void TextureFont::setMaxAscent(int _maxAscent)
{
    maxAscent = _maxAscent;
}

int TextureFont::getMaxDescent() const
{
    return maxDescent;
}

void TextureFont::setMaxDescent(int _maxDescent)
{
    maxDescent = _maxDescent;
}


int TextureFont::getTextureName() const
{
    return texName;
}


void TextureFont::bind()
{
    auto *prog = renderer->getShaderManager().getShader("text");
    if (prog == nullptr)
        return;

    if (texName != 0)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texName);
        prog->use();
        prog->samplerParam("atlasTex") = 0;
    }
}

void TextureFont::unbind()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void TextureFont::addGlyph(const TextureFont::Glyph& g)
{
    glyphs.push_back(g);
    if (g.width > maxWidth)
        maxWidth = g.width;
}


const TextureFont::Glyph* TextureFont::getGlyph(wchar_t ch) const
{
    if (ch >= (wchar_t)glyphLookupTableSize)
        return nullptr;

    return glyphLookup[ch];
}


bool TextureFont::buildTexture()
{
    assert(fontImage != nullptr);

    if (texName != 0)
        glDeleteTextures(1, (const GLuint*) &texName);
    glGenTextures(1, (GLuint*) &texName);
    if (texName == 0)
    {
        DPRINTF(LOG_LEVEL_ERROR, "Failed to allocate texture object for font.\n");
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, texName);

    // Don't build mipmaps . . . should really make them an option.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA,
                 texWidth, texHeight,
                 0,
                 GL_ALPHA, GL_UNSIGNED_BYTE,
                 fontImage);

    return true;
}


void TextureFont::rebuildGlyphLookupTable()
{
    if (glyphs.size() == 0)
        return;

    // Find the largest glyph id
    int maxID = glyphs[0].__id;
    vector<Glyph>::const_iterator iter;
    for (iter = glyphs.begin(); iter != glyphs.end(); iter++)
    {
        if (iter->__id > maxID)
            maxID = iter->__id;
    }

    // If there was already a lookup table, delete it.
    delete[] glyphLookup;

    DPRINTF(LOG_LEVEL_INFO, "texturefont: allocating glyph lookup table with %d entries.\n",
            maxID + 1);
    glyphLookup = new const Glyph*[maxID + 1];
    for (int i = 0; i <= maxID; i++)
        glyphLookup[i] = nullptr;

    // Fill the table with glyph pointers
    for (iter = glyphs.begin(); iter != glyphs.end(); iter++)
        glyphLookup[iter->__id] = &(*iter);
    glyphLookupTableSize = (unsigned int) maxID + 1;
}


static uint32_t readUint32(istream& in, bool swap)
{
    uint32_t x;
    in.read(reinterpret_cast<char*>(&x), sizeof x);
    return swap ? bswap_32(x) : x;
}

static uint16_t readUint16(istream& in, bool swap)
{
    uint16_t x;
    in.read(reinterpret_cast<char*>(&x), sizeof x);
    return swap ? bswap_16(x) : x;
}

static uint8_t readUint8(istream& in)
{
    uint8_t x;
    in.read(reinterpret_cast<char*>(&x), sizeof x);
    return x;
}

/* Not currently used
static int32_t readInt32(istream& in, bool swap)
{
    int32_t x;
    in.read(reinterpret_cast<char*>(&x), sizeof x);
    return swap ? static_cast<int32_t>(bswap_32(static_cast<uint32_t>(x))) : x;
}*/

static int16_t readInt16(istream& in, bool swap)
{
    int16_t x;
    in.read(reinterpret_cast<char*>(&x), sizeof x);
    return swap ? static_cast<int16_t>(bswap_16(static_cast<uint16_t>(x))) : x;
}

static int8_t readInt8(istream& in)
{
    int8_t x;
    in.read(reinterpret_cast<char*>(&x), sizeof x);
    return x;
}


TextureFont* TextureFont::load(const Renderer *r, istream& in)
{
    char header[4];

    in.read(header, sizeof header);
    if (!in.good() || strncmp(header, "\377txf", 4) != 0)
    {
        DPRINTF(LOG_LEVEL_ERROR, "Stream is not a texture font!.\n");
        return nullptr;
    }

    uint32_t endiannessTest = 0;
    in.read(reinterpret_cast<char*>(&endiannessTest), sizeof endiannessTest);
    if (!in.good())
    {
        DPRINTF(LOG_LEVEL_ERROR, "Error reading endianness bytes in txf header.\n");
        return nullptr;
    }

    bool byteSwap;
    if (endiannessTest == 0x78563412)
        byteSwap = true;
    else if (endiannessTest == 0x12345678)
        byteSwap = false;
    else
    {
        DPRINTF(LOG_LEVEL_ERROR, "Stream is not a texture font!.\n");
        return nullptr;
    }

    int format = readUint32(in, byteSwap);
    unsigned int texWidth = readUint32(in, byteSwap);
    unsigned int texHeight = readUint32(in, byteSwap);
    unsigned int maxAscent = readUint32(in, byteSwap);
    unsigned int maxDescent = readUint32(in, byteSwap);
    unsigned int nGlyphs = readUint32(in, byteSwap);

    if (!in)
    {
        DPRINTF(LOG_LEVEL_ERROR, "Texture font stream is incomplete");
        return nullptr;
    }

    DPRINTF(LOG_LEVEL_INFO, "Font contains %d glyphs.\n", nGlyphs);

    auto* font = new TextureFont(r);
    assert(font != nullptr);

    font->setMaxAscent(maxAscent);
    font->setMaxDescent(maxDescent);

    float dx = 0.5f / texWidth;
    float dy = 0.5f / texHeight;

    for (unsigned int i = 0; i < nGlyphs; i++)
    {
        uint16_t __id = readUint16(in, byteSwap);
        TextureFont::Glyph glyph(__id);

        glyph.width = readUint8(in);
        glyph.height = readUint8(in);
        glyph.xoff = readInt8(in);
        glyph.yoff = readInt8(in);
        glyph.advance = readInt8(in);
        readInt8(in);
        glyph.x = readInt16(in, byteSwap);
        glyph.y = readInt16(in, byteSwap);

        if (!in)
        {
            DPRINTF(LOG_LEVEL_ERROR, "Error reading glyph %ud from texture font stream.\n", i);
            delete font;
            return nullptr;
        }

        float fWidth = texWidth;
        float fHeight = texHeight;
        glyph.texCoords[0].u = glyph.x / fWidth + dx;
        glyph.texCoords[0].v = glyph.y / fHeight + dy;
        glyph.texCoords[1].u = (glyph.x + glyph.width) / fWidth + dx;
        glyph.texCoords[1].v = glyph.y / fHeight + dy;
        glyph.texCoords[2].u = (glyph.x + glyph.width) / fWidth + dx;
        glyph.texCoords[2].v = (glyph.y + glyph.height) / fHeight + dy;
        glyph.texCoords[3].u = glyph.x / fWidth + dx;
        glyph.texCoords[3].v = (glyph.y + glyph.height) / fHeight + dy;

        font->addGlyph(glyph);
    }

    font->texWidth = texWidth;
    font->texHeight = texHeight;
    if (format == TxfByte)
    {
        auto* fontImage = new unsigned char[texWidth * texHeight];

        DPRINTF(LOG_LEVEL_INFO, "Reading %d x %d 8-bit font image.\n", texWidth, texHeight);

        in.read(reinterpret_cast<char*>(fontImage), texWidth * texHeight);
        if (in.gcount() != (signed)(texWidth * texHeight))
        {
            DPRINTF(LOG_LEVEL_ERROR, "Missing bitmap data in font stream.\n");
            delete font;
            delete[] fontImage;
            return nullptr;
        }

        font->fontImage = fontImage;
    }
    else
    {
        int rowBytes = (texWidth + 7) >> 3;
        auto* fontBits = new unsigned char[rowBytes * texHeight];
        auto* fontImage = new unsigned char[texWidth * texHeight];

        DPRINTF(LOG_LEVEL_INFO, "Reading %d x %d 1-bit font image.\n", texWidth, texHeight);

        in.read(reinterpret_cast<char*>(fontBits), rowBytes * texHeight);
        if (in.gcount() != (signed)(rowBytes * texHeight))
        {
            DPRINTF(LOG_LEVEL_ERROR, "Missing bitmap data in font stream.\n");
            delete font;
            delete[] fontImage;
            delete[] fontBits;
            return nullptr;
        }

        for (unsigned int y = 0; y < texHeight; y++)
        {
            for (unsigned int x = 0; x < texWidth; x++)
            {
                if ((fontBits[y * rowBytes + (x >> 3)] & (1 << (x & 0x7))) != 0)
                    fontImage[y * texWidth + x] = 0xff;
                else
                    fontImage[y * texWidth + x] = 0x0;
            }
        }

        font->fontImage = fontImage;
        delete[] fontBits;
    }

    font->rebuildGlyphLookupTable();

    return font;
}


TextureFont* LoadTextureFont(const Renderer *r, const fs::path& filename)
{
    fs::path localeFilename = LocaleFilename(filename);
    ifstream in(localeFilename.string(), ios::in | ios::binary);
    if (!in.good())
    {
        DPRINTF(LOG_LEVEL_ERROR, "Could not open font file %s\n", filename);
        return nullptr;
    }

    return TextureFont::load(r, in);
}
