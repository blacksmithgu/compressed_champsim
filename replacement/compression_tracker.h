#ifndef COMPRESSION_TRACKER_H
#define COMPRESSION_TRACKER_H

#include <array>
#include <iostream>

class CompressionTracker {
    // The count for compression factor "cf" is at index (cf - 1).
    std::array<uint64_t, 4> counts;

public:
    CompressionTracker() : counts() {}

    /** Increment the count of the given compression factor. */
    void increment(uint32_t compression_factor) {
        assert(compression_factor >= 1 && compression_factor <= 4);
        counts[compression_factor - 1]++;
    }

    /** Obtain the count of the given compression factor. */
    uint64_t count(uint32_t compression_factor) {
        return counts[compression_factor - 1];
    }

    /** Print out a summary of the compression factors. */
    void print() {
        // Total sum of all counts.
        uint64_t total_lines = count(4) + count(2) + count(1);
        double ratio4 = count(4) / double(total_lines);
        double ratio2 = count(2) / double(total_lines);
        double ratio1 = count(1) / double(total_lines);
        double bench_comp = total_lines / (count(4) * 0.25 + count(2) * 0.5 + count(1));
        double line_comp = (4 * count(4) + 2 * count(2) + count(1)) / double(total_lines);

        printf("Compressible 4: %ld (%.2f%%)\n", count(4), ratio4 * 100.0);
        printf("Compressible 2: %ld (%.2f%%)\n", count(2), ratio2 * 100.0);
        printf("Compressible 1: %ld (%.2f%%)\n", count(1), ratio1 * 100.0);
        printf("Benchmark Compression Ratio: %.2f\n", bench_comp);
        printf("Average Line Compressibility: %.2f\n", line_comp);
    }
};

#endif
