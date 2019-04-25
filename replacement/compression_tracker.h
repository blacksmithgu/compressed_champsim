#ifndef COMPRESSION_TRACKER_H
#define COMPRESSION_TRACKER_H

#include <array>
#include <iostream>

class CompressionTracker {
    // The count for compression factor "cf" is at index (cf - 1).
    std::array<uint64_t, MAX_COMPRESSIBILITY> counts;

public:
    CompressionTracker() : counts() {}

    /** Increment the count of the given compression factor. */
    void increment(uint32_t compression_factor) {
        assert(compression_factor >= 1 && compression_factor <= MAX_COMPRESSIBILITY);
        counts[compression_factor - 1]++;
    }

    /** Obtain the count of the given compression factor. */
    uint64_t count(uint32_t compression_factor) {
        return counts[compression_factor - 1];
    }

    /** Print out a summary of the compression factors. */
    void print() {
        // Total sum of all counts.
        uint64_t total_lines = 0;
        for (uint64_t i = MAX_COMPRESSIBILITY; i > 0; i /= 2) {
            total_lines += count(i);
        }
        double denominator = 0.0;
        for (uint64_t i = MAX_COMPRESSIBILITY; i > 0; i /= 2) {
            const double ratio = count(i) / double(total_lines);
            denominator += count(i) * (1.0 / double(i));
            printf("Compressible %ld: %ld (%.2f%%)\n", count(i), count(i), ratio * 100.0);
        }
        double bench_comp = total_lines / denominator;
        double line_comp = (4 * count(4) + 2 * count(2) + count(1)) / double(total_lines);

        printf("Benchmark Compression Ratio: %.2f\n", bench_comp);
        printf("Average Line Compressibility: %.2f\n", line_comp);
    }
};

#endif
