/* 
 * Copyright (c) Jared Higgins 2019.
 */

#include <iostream>

void adaptiveInpaint(const std::string& srcFile, const std::string& maskFile, 
             const std::string& outFile);

void exemplarInpaint(int patchRadius, const std::string& srcFile, 
            const std::string& maskFile, const std::string& outFile);

/**
 * Launches the program.
 */
int main(int argc, char** argv) {
    if (argc < 5 || argc > 6 || (argc == 6 && std::string(argv[1])[0] != 'e')) {
        std::cout << "Incorrect number of arguments: proper usage is "
                     "inpainter [type] [patch radius (if using exemplar method)] "
                     "[source image] [mask image] [output image]"
                  << std::endl;
    } else {
        auto type = std::string(argv[1]);
        if (type == "adaptive" || type == "a") {
            adaptiveInpaint(std::string(argv[2]), std::string(argv[3]), 
                std::string(argv[4]));
        } else if (type == "exemplar" || type == "e") {
            exemplarInpaint(std::stoi(std::string(argv[2])), std::string(argv[3]), 
                std::string(argv[4]), std::string(argv[5]));
        }
    }
    return 0;
}

