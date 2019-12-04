#include <png++/png.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

namespace {
    // Convenience aliases
    using Pixel = png::rgba_pixel;
    using Image = png::image<Pixel>;
    
    // Shared variables for the adaptive inpainting algorithm
    Image src, mask;

    // Constants
    const std::vector<int> mu_t = {0, 7, 8, 9, 10, 12, 14, 16, 18, 20, 22};
    const std::vector<int> lambda_t = {0, 20, 22, 24, 26, 28, 30, 32, 34, 36, 
        38};
    const int MAX_DIS = 10;

    // Used to store the global mean pixel
    Pixel globalMeanPixel;
    bool globalMeanPixelSet = false;

    bool isDamagedPixel(const Pixel& pixel) {
        return pixel.alpha != 0;
    }

    Pixel getGlobalMeanPixel() {
        if (globalMeanPixelSet) {
            return globalMeanPixel;
        }

        long red = 0, green = 0, blue = 0, size = 0;
        for (png::uint_32 y = 0; y < src.get_height(); y++) {
            for (png::uint_32 x = 0; y < src.get_height(); y++) {
                if (isDamagedPixel(mask[y][x])) {
                    continue;
                }
                Pixel px = src[y][x];
                red += px.red;
                green += px.green;
                blue += px.blue;
                size++;
            }
        }
        globalMeanPixel = Pixel(red / size, green / size, blue / size);
        globalMeanPixelSet = true;
        return globalMeanPixel;
    }

    Pixel getMeanPixel(png::uint_32 y, png::uint_32 x, int DIS) {
        const auto startX = (x - DIS) < 0 ? 0 : (x - DIS);
        const auto endX = (x + DIS) >= mask.get_width() ? mask.get_width() - 1 
                          : (x + DIS);
        const auto startY = (y - DIS) < 0 ? 0 : (y - DIS);
        const auto endY = (y + DIS) >= mask.get_height() ? mask.get_height() - 1 
                          : (y + DIS);
        int size = 0;
        int red = 0, green = 0, blue = 0;
        for (auto currentY = startY; currentY <= endY; currentY++) {
            for (auto currentX = startX; currentX <= endX; currentX++) {
                if (isDamagedPixel(mask[currentY][currentX])) {
                    continue;
                }
                Pixel px = src[currentY][currentX];
                red += px.red;
                green += px.green;
                blue += px.blue;
                size++;
            }
        }
        return Pixel(red / size, green / size, blue / size);
    }

    int getMu(png::uint_32 y, png::uint_32 x, int DIS) {
        const auto startX = (x - DIS) < 0 ? 0 : (x - DIS);
        const auto endX = (x + DIS) >= mask.get_width() ? mask.get_width() - 1 
                          : (x + DIS);
        const auto startY = (y - DIS) < 0 ? 0 : (y - DIS);
        const auto endY = (y + DIS) >= mask.get_height() ? mask.get_height() - 1 
                          : (y + DIS);
        const auto size = (endX - startX + 1) * (endY - startY + 1);
        int usefulPixels = 0;
        for (auto currentY = startY; currentY <= endY; currentY++) {
            for (auto currentX = startX; currentX <= endX; currentX++) {
                if (!isDamagedPixel(mask[currentY][currentX])) {
                    usefulPixels++;
                }
            }
        }
        return ((double) usefulPixels / size) * 100;
    }

    int getLambda(png::uint_32 y, png::uint_32 x, int DIS) {
        const auto startX = (x - DIS) < 0 ? 0 : (x - DIS);
        const auto endX = (x + DIS) >= mask.get_width() ? mask.get_width() - 1 
                          : (x + DIS);
        const auto startY = (y - DIS) < 0 ? 0 : (y - DIS);
        const auto endY = (y + DIS) >= mask.get_height() ? mask.get_height() - 1 
                          : (y + DIS);
        const auto size = (endX - startX + 1) * (endY - startY + 1);
        int noisePixels = 0;
        for (auto currentY = startY; currentY <= endY; currentY++) {
            for (auto currentX = startX; currentX <= endX; currentX++) {
                if (isDamagedPixel(mask[currentY][currentX])) {
                    noisePixels++;
                }
            }
        }
        return ((double) noisePixels / size) * 100;
    }

    Pixel getInterpolatedPixel(png::uint_32 y, png::uint_32 x) {
        // Algorithm adapted from Shih, Chang, Lu, Ko, Wang: 
        // Adaptive digital image inpainting
        int DIS = 1;
        while (true) {
            if (DIS > MAX_DIS) {
                return getGlobalMeanPixel();
            }
            int mu = getMu(y, x, DIS);
            if (mu != 0) {
                if (mu > mu_t[DIS]) {
                    return getMeanPixel(y, x, DIS);
                } else {
                    DIS++;
                }
            } else {
                int lambda = getLambda(y, x, DIS);
                if (lambda > lambda_t[DIS]) {
                    DIS++;
                } else {
                    return getGlobalMeanPixel();
                }
            }
        }
    }

}  // End namespace

void adaptiveInpaint(const std::string& srcFile, const std::string& maskFile, 
             const std::string& outFile) {
    src = Image(srcFile);
    mask = Image(maskFile);
    if (src.get_width() != mask.get_width() 
            || src.get_height() != mask.get_height()) {
        std::cerr << "The source image and mask image must have the same "
                     "dimensions." << std::endl;
        return;
    }
    for (png::uint_32 y = 0; y < src.get_height(); y++) {
        for (png::uint_32 x = 0; x < src.get_width(); x++) {
            if (isDamagedPixel(mask[y][x])) {
                src[y][x] = getInterpolatedPixel(y, x);
            }
        }
    }
    src.write(outFile);
}