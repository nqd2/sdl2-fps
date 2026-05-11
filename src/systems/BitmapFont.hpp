#pragma once

#include <SDL.h>

namespace BitmapFont {

constexpr int kGlyphW = 5;
constexpr int kGlyphH = 7;

int textPixelWidth(const char* text, int scale);

void drawText(Uint32* pixels, int pixelPitch, int screenW, int screenH,
              const char* text, int posX, int posY, int scale, Uint32 color);

void drawTextCentered(Uint32* pixels, int pixelPitch, int screenW, int screenH,
                      const char* text, int cx, int cy, int scale, Uint32 color);

}  // namespace BitmapFont
