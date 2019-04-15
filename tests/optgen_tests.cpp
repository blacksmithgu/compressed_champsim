#include "../replacement/size_aware_optgen.h"
#include <array>
#include <assert.h>
#include <iostream>

struct Interval {
    uint64_t start_interval;
    uint64_t end_interval;
    uint64_t address;

    constexpr Interval(uint64_t start, uint64_t end, uint64_t address) : start_interval(start), end_interval(end), address(address) { }
};

// example from https://www.cs.utexas.edu/~lin/papers/isca16.pdf
constexpr std::array<Interval, 6> reuse_access_stream {
    Interval(1, 2, 'B'),
    Interval(0, 6, 'A'),
    Interval(4, 8, 'D'),
    Interval(5, 9, 'E'),
    Interval(7, 10, 'F'),
    Interval(3, 11, 'C'),
};

int main() {
    std::cout << "OPTGEN TEST" << std::endl;
    OPTgen<1024> test_optgen(2);
    int hits = 0;
    for (int i = 0; i < reuse_access_stream.size(); i++) {
        const auto& current_interval = reuse_access_stream[i];
        if (test_optgen.try_cache(current_interval.start_interval, current_interval.end_interval, current_interval.address, 1)) {
            hits++;
        }
    }
    std::cout << "Hits are " << hits << std::endl;
    assert(hits == 4);
    std::cout << std::endl;

    std::cout << "YACCGEN TEST" << std::endl;
    YACCgen<1024> yaccgen(2);

    // overlapping superblock usage intervals
    assert(yaccgen.try_cache(0, 10, 0, 2));
    assert(yaccgen.try_cache(4, 14, 0, 2));

    // different superblock
    assert(yaccgen.try_cache(0, 20, 1, 1));

    // overlapping; cache is full, should reject
    assert(!yaccgen.try_cache(1, 21, 1, 1));
    assert(!yaccgen.try_cache(1, 22, 0, 2));

    // should replace first superblock 0.
    assert(yaccgen.try_cache(15, 20, 3, 1));

    // much later on, should replace both superblocks.
    assert(yaccgen.try_cache(50, 80, 3, 1));
    assert(yaccgen.try_cache(50, 81, 3, 1));

    // off-by-one-errors.
    assert(!yaccgen.try_cache(80, 81, 3, 1));
    std::cout << "all assertions passed" << std::endl;
}
