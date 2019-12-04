#ifndef WINDOWPROP_H
#define WINDOWPROP_H

#include <vector>

#include <png++/png.hpp>

using Pixel = png::rgba_pixel;
using Image = png::image<Pixel>;

class WindowProp {
private:
    double variance;
    double validPixelRate;
    
    void calculateVariables(const Image& src, const Image& mask);
    
public:
    png::uint_32 left, right, up, down, x, y;
    WindowProp() {}
    WindowProp(const Image& src, const Image& mask, png::uint_32 x, png::uint_32 y,
           int radius);
    
    double getVariance() const;
    double getValidPixelRate() const;
    int getArea() const;
    int getHeight() const;
    int getWidth() const;
};

#endif /* WINDOW_H */

