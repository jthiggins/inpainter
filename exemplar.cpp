#include <png++/png.hpp>
#include <glm/glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <math.h>

#include "windowex.h"

namespace {
    struct KeyFuncs {
        size_t operator()(const glm::vec2& k)const
        {
            return std::hash<int>()(k.x) ^ std::hash<int>()(k.y);
        }

        bool operator()(const glm::vec2& a, const glm::vec2& b)const
        {
                return a.x == b.x && a.y == b.y;
        }
    };
    
    // Convenience aliases
    using Pixel = png::rgba_pixel;
    using Image = png::image<Pixel>;
    using Boundary = std::vector<glm::vec2>;
    using PriorityMap = std::unordered_map<glm::vec2, double, KeyFuncs, KeyFuncs>;
    using ConfidenceMap = std::unordered_map<glm::vec2, double, KeyFuncs, KeyFuncs>;
    using VecPair = std::pair<glm::vec2, glm::vec2>;
    
    // Shared variables for exemplar algorithm
    Image src, mask;
    ConfidenceMap C;
    int patchRadius;
    
    // Constants
    const double ALPHA = 255;
    
    bool isDamagedPixel(int x, int y) {
        if (x < 0 || x >= src.get_width() || y < 0 || y >= src.get_height()) {
            return false;
        }
        return mask[y][x].alpha != 0;
    }
    
    void initializeVariables() {
        for (auto y = 0; y < src.get_height(); y++) {
            for (auto x = 0; x < src.get_width(); x++) {
                if (isDamagedPixel(x, y)) {
                    C[glm::vec2(x, y)] = 0;
                } else {
                    C[glm::vec2(x, y)] = 1;
                }
            }
        }
    }
    
    Boundary extractBoundary() {
        Boundary ret;
        for (auto y = 0; y < src.get_height(); y++) {
            for (auto x = 0; x < src.get_width(); x++) {
                if (isDamagedPixel(x, y)) {
                    if (!isDamagedPixel(x - 1, y) || !isDamagedPixel(x + 1, y) ||
                            !isDamagedPixel(x, y - 1) || !isDamagedPixel(x, y + 1)) {
                        ret.push_back(glm::vec2(x, y));
                    }
                }
            }
        }
        return ret;
    }
    
    std::vector<glm::vec2> getGradient(int x, int y) {
        Pixel x1, x2, y1, y2;
        int x1Offset = 0, x2Offset = 0, y1Offset = 0, y2Offset = 0;
        while (x - x1Offset >= 0 && isDamagedPixel(x - x1Offset, y)) {
            x1Offset++;
            if (x - x1Offset == -1) {
                x1Offset = -1;
                break;
            }
        }
        while (x + x2Offset < src.get_width() && isDamagedPixel(x + x2Offset, y)) {
            x2Offset++;
            if (x + x2Offset == src.get_width()) {
                x2Offset = -1;
                break;
            }
        }
        while (y - y1Offset >= 0 && isDamagedPixel(x, y - y1Offset)) {
            y1Offset++;
            if (y - y1Offset == -1) {
                y1Offset = -1;
                break;
            }
        }
        while (y + y2Offset < src.get_height() && isDamagedPixel(x, y + y2Offset)) {
            y2Offset++;
            if (y + y2Offset == src.get_height()) {
                y2Offset = -1;
                break;
            }
        }
        if (x1Offset == -1) {
            x1 = Pixel(0,0,0,0);
            x2 = src[y][x+x2Offset];
        } else if (x2Offset == -1) {
            x1 = src[y][x-x1Offset];
            x2 = Pixel(0,0,0,0);
        } else {
            x1 = src[y][x-x1Offset];
            x2 = src[y][x+x2Offset];
        }
        if (y1Offset == -1) {
            y1 = Pixel(0,0,0,0);
            y2 = src[y+y2Offset][x];
        } else if (y2Offset == -1) {
            y1 = src[y-y1Offset][x];
            y2 = Pixel(0,0,0,0);
        } else {
            y1 = src[y-y1Offset][x];
            y2 = src[y+y2Offset][x];
        }
        return {
            glm::vec2(x2.red - x1.red, y2.red - y1.red),
            glm::vec2(x2.green - x1.green, y2.green - y1.green),
            glm::vec2(x2.blue - x1.blue, y2.blue - y1.blue)
        };
    }
    
    glm::vec2 calcGradient(const WindowEx& w) {
        glm::vec2 gradient;
        for (auto x = w.x - w.left; x <= w.x + w.right; x++) {
            for (auto y = w.y - w.up; y <= w.y + w.down; y++) {
                if (isDamagedPixel(x, y)) {
                    continue;
                }
                auto gVec = getGradient(x, y);
                auto g = glm::vec2((gVec[0].x + gVec[1].x + gVec[2].x) / 3,
                        (gVec[0].y + gVec[1].y + gVec[2].y) / 3);
                gradient = glm::max(gradient, g);
            }
        }
        return gradient;
    }
    
    VecPair getNormalVectors(const glm::vec2& boundaryVec) {
        // If two vectors are orthogonal, their dot product is 0. So, given 
        // <a,b>, we want to find <c,d> such that <a,b> . <c,d> = ac + bd = 0.
        // There are two solutions: <c,d> = <-b, a> or <c,d> = <b, -a>.
        auto a = boundaryVec.x, b = boundaryVec.y;
        return VecPair(glm::vec2(-b, a), glm::vec2(b, -a));  
    }
    
    PriorityMap computePriorities(const Boundary& boundary) {
        PriorityMap ret;
        int index = boundary.size();
        for (auto pos : boundary) {
            double conf, data;
            double sum = 0;
            WindowEx w(src, pos.x, pos.y, patchRadius);
            for (auto x = w.x - w.left; x <= w.x + w.right; x++) {
                for (auto y = w.y - w.up; y <= w.y + w.down; y++) {
                    if (!isDamagedPixel(x, y)) {
                        sum += C[glm::vec2(x, y)];
                    }
                }
            }
            conf = sum / w.getArea();
            auto prev = boundary[(index - 1) % boundary.size()];
            auto next = boundary[(index + 1) % boundary.size()];
            glm::vec2 boundaryVec = next - prev;
            auto n = getNormalVectors(boundaryVec).first;
            auto isophote = getNormalVectors(calcGradient(w)).first;
            data = abs(glm::dot(isophote, n)) / ALPHA;
            ret[pos] = conf * data;
            index++;
        }
        return ret;
    }
    
    WindowEx findPatch(const PriorityMap& priorities) {
        glm::vec2 patchCenter;
        double minPriority = 1e300;
        for (const auto& entry : priorities) {
            if (entry.second < minPriority) {
                minPriority = entry.second;
                patchCenter = entry.first;
            }
        }
        return WindowEx(src, patchCenter.x, patchCenter.y, patchRadius);
    }
    
    double d(const WindowEx& a, const WindowEx& b) {
        double ret = 0;
        int left = (a.left < b.left) ? a.left : b.left;
        int right = (a.right < b.right) ? a.right : b.right;
        int up = (a.up < b.up) ? a.up : b.up;
        int down = (a.down < b.down) ? a.down : b.down;        
        for (auto xOffset = left; xOffset <= right; xOffset++) {
            for (auto yOffset = up; yOffset <= down; yOffset++) {
                if (isDamagedPixel(a.x + xOffset, a.y + yOffset) 
                        || isDamagedPixel(b.x + xOffset, b.y + yOffset)) {
                    continue;
                }
                auto aPx = src[a.y + yOffset][a.x + xOffset];
                auto bPx = src[b.y + yOffset][b.x + xOffset];
                ret += powf(aPx.red - bPx.red, 2);
                ret += powf(aPx.green - bPx.green, 2);
                ret += powf(aPx.blue - bPx.blue, 2);
            }
        }
        return ret;
    }
    
    WindowEx findExemplar(const WindowEx& patch) {
        WindowEx exemplar;
        double minD = 1e300;
        for (int y = patchRadius; y < src.get_height() - patchRadius; y++) {
            for (int x = patchRadius; x < src.get_width() - patchRadius; x++) {
                WindowEx test(src, x, y, patchRadius);
                bool valid = true;
                for (auto x = test.x - test.left; x <= test.x + test.right; x++) {
                    for (auto y = test.y - test.up; y <= test.y + test.down; y++) {
                        if (isDamagedPixel(x, y)) {
                            valid = false;
                            break;
                        }
                    }
                    if (!valid) {
                        break;
                    }
                }
                if (!valid) {
                    continue;
                }
                auto currentD = d(test, patch);
                if (currentD < minD) {
                    exemplar = test;
                    minD = currentD;
                }
            }
        }
        return exemplar;
    }
    
    void copyData(const WindowEx& exemplar, const WindowEx& patch) {
        for (auto xOffset = -patch.left; xOffset <= patch.right; xOffset++) {
            for (auto yOffset = -patch.up; yOffset <= patch.down; yOffset++) {
                if (!isDamagedPixel(patch.x + xOffset, patch.y + yOffset)) {
                    continue;
                }
                auto exPx = src[exemplar.y + yOffset][exemplar.x + xOffset];
                src[patch.y + yOffset][patch.x + xOffset] = exPx;
            }
        }
    }
    
    void updateC(const WindowEx& patch) {
        double conf = 0;
        for (auto x = patch.x - patch.left; x <= patch.x + patch.right; x++) {
            for (auto y = patch.y - patch.up; y <= patch.y + patch.down; y++) {
                if (!isDamagedPixel(x, y)) {
                    conf += C[glm::vec2(x, y)];
                }
            }
        }
        conf /= patch.getArea();
        for (auto x = patch.x - patch.left; x <= patch.x + patch.right; x++) {
            for (auto y = patch.y - patch.up; y <= patch.y + patch.down; y++) {
                if (isDamagedPixel(x, y)) {
                    C[glm::vec2(x, y)] = conf;
                    mask[y][x].alpha = 0;
                }
            }
        }
    }
    
}  // End namespace

void exemplarInpaint(int radius, const std::string& srcFile, 
        const std::string& maskFile, const std::string& outFile) {
    src = Image(srcFile); 
    mask = Image(maskFile);
    patchRadius = radius;
    if (src.get_width() != mask.get_width() 
            || src.get_height() != mask.get_height()) {
        std::cerr << "The source image and mask image must have the same "
                     "dimensions." << std::endl;
        return;
    }
    std::cout << "Initializing data..." << std::endl;
    initializeVariables();
    auto boundary = Boundary(1);
    int pass = 1;
    while (true) {
        boundary = extractBoundary();
        std::cout << "Pass " << pass++ << " - boundary size " << boundary.size() << std::endl;
        if (boundary.empty()) {
            break;
        }
        auto priorities = computePriorities(boundary);
        auto patch = findPatch(priorities);
        auto exemplar = findExemplar(patch);
        copyData(exemplar, patch);
        updateC(patch);
    }
    src.write(outFile);
}