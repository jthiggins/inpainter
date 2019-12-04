#ifndef WINDOWEX_H
#define WINDOWEX_H

#include <png++/png.hpp>

using Pixel = png::rgba_pixel;
using Image = png::image<Pixel>;

class WindowEx {
private:
    
public:
    int left, right, up, down, x, y;
    WindowEx() {}
    WindowEx(const Image& src, png::uint_32 x, png::uint_32 y, int radius);
    int getArea() const;
    int getHeight() const;
    int getWidth() const;
};

#endif /* WINDOW_H */

