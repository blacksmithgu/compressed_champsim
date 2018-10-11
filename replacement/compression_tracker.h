#ifndef COMPRESSION_TRACKER_H
#define COMPRESSION_TRACKER_H

#include <array>
#include <iostream>

class CompressionTracker {
    // The count for compression factor "cf" is at index (cf - 1).
    std::array<uint32_t, 4> counts = {0};

public:
    /** Increment the count of the given compression factor. */
    void increment(uint32_t compression_factor) {
        counts[compression_factor - 1]++;
    }

    /** Obtain the count of the given compression factor. */
    uint32_t count(uint32_t compression_factor) {
        return counts[compresion_factor - 1];
    }

    /** Print out a summary of the compression factors. */
    void print() {
        std::cout << "Compressible 4: " << count(4) << std::endl;
        std::cout << "Compressible 2: " << count(2) << std::endl;
        std::cout << "Compressible 1: " << count(1) << std::endl;
    }
};

#endif
