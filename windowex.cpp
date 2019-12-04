#include "windowex.h"

WindowEx::WindowEx(const Image& src, png::uint_32 x, png::uint_32 y, int radius) 
        : x(x), y(y) {
    left = (x >= radius) ? radius : x;
    right = (x + radius < src.get_width()) ? radius : src.get_width() - x - 1;
    up = (y >= radius) ? radius : y;
    down = (y + radius < src.get_height()) ? radius : src.get_height() - y - 1;
}

int WindowEx::getArea() const {
    return getWidth() * getHeight();
}

int WindowEx::getWidth() const {
    return (left + right + 1);
}

int WindowEx::getHeight() const {
    return (up + down + 1);
}