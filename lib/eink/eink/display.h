#ifndef __EINK_DISPLAY__
#define __EINK_DISPLAY__

#include "epd_driver.h"
#include "epd_highlevel.h"

#define EINK_DISPLAY_WIDTH EPD_WIDTH
#define EINK_DISPLAY_HEIGHT EPD_HEIGHT

namespace eink {

enum class FontSize {
  Size10,
  Size12,
  Size16,
  Size16b,
  Size24,
};

enum class DrawTextDirection {
  LTR,
  RTL,
};

class Display {
 public:
  Display();
  Display(const Display&) = delete;
  Display& operator=(const Display&) = delete;

  void Clear();
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void DrawRect(int y, int x, int h, int w, uint8_t color, bool fill);
  void DrawText(int y, int x, const char* text, uint8_t color, FontSize size);
  void DrawText(int y, int x, const char* text, uint8_t color, FontSize size,
                DrawTextDirection dir);
  void Update();
  int FontHeight(FontSize f);

 private:
  // High level display state.
  EpdiyHighlevelState hl_;
  // Framebuffer.
  uint8_t* fb_;
};

}  // namespace eink
#endif  // __EINK_DISPLAY__