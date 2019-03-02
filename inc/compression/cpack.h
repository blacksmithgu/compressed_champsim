#ifndef __CPACK_COMPRESS_H__
#define __CPACK_COMPRESS_H__

#include <cstdint>

namespace cpack {
    int compress(const uint8_t* input, uint8_t* output);

    void set_bit(uint8_t*, int&, bool);
    void set_bit(uint8_t*, int&, bool, bool);
    void set_bit(uint8_t*, int&, bool, bool, bool, bool);

    void set_byte(uint8_t*, int&, uint8_t);
    void set_idx(uint8_t*, int&, int);

    int decompress(const uint8_t input[68], uint8_t output[64], int in_size);

    uint8_t read_bit(const uint8_t* input, int& i);
    uint8_t read_byte(const uint8_t* input, int& i);
    int read_idx(const uint8_t* input, int& i);
};

#endif
