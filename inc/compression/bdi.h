#ifndef __BDI_COMPRESSION_H__
#define __BDI_COMPRESSION_H__

#include <cstdlib>
#include <cstdio>

namespace bdi {

inline unsigned long long my_llabs(long long x) {
    unsigned long long t = x >> 63;
    return (x ^ t) - t;
}

inline unsigned my_abs(int x) {
    unsigned t = x >> 31;
    return (x ^ t) - t;
}

inline long long unsigned *convertBuffer2Array(char *buffer, unsigned size, unsigned step) {
    long long unsigned *values = (long long unsigned *)malloc(sizeof(long long unsigned) * size / step);

    //init
    unsigned int i, j;
    for (i = 0; i < size / step; i++)
        values[i] = 0; // Initialize all elements to zero.

    for (i = 0; i < size; i += step)
        for (j = 0; j < step; j++)
            values[i / step] += (long long unsigned)((unsigned char)buffer[i + j]) << (8 * j);

    return values;
}

///
/// Check if the cache line consists of only zero values
///
inline int isZeroPackable(long long unsigned *values, unsigned size) {
    int nonZero = 0;
    unsigned int i;
    for (i = 0; i < size; i++) {
        if (values[i] != 0) {
            nonZero = 1;
            break;
        }
    }
    return !nonZero;
}

///
/// Check if the cache line consists of only same values
///
inline int isSameValuePackable(long long unsigned *values, unsigned size) {
    int notSame = 0;
    unsigned int i;
    for (i = 0; i < size; i++) {
        if (values[0] != values[i]) {
            notSame = 1;
            break;
        }
    }
    return !notSame;
}

///
/// Check if the cache line values can be compressed with multiple base + 1,2,or 4-byte offset
/// Returns size after compression
///
inline unsigned doubleExponentCompression(long long unsigned *values, unsigned size, unsigned blimit, unsigned bsize) {
    unsigned long long limit = 0;
    //define the appropriate size for the mask
    switch (blimit) {
    case 1:
        limit = 56;
        break;
    case 2:
        limit = 48;
        break;
    default:
        // std::cout << "Wrong blimit value = " <<  blimit << std::endl;
        exit(1);
    }
    // finding bases: # BASES
    // find how many elements can be compressed with mbases
    unsigned compCount = 0;
    unsigned int i;
    for (i = 0; i < size; i++) {
        if ((values[0] >> limit) == (values[i] >> limit)) {
            compCount++;
        }
    }
    //return compressed size
    if (compCount != size)
        return size * bsize;
    return size * bsize - (compCount - 1) * blimit;
}

///
/// Check if the cache line values can be compressed with multiple base + 1,2,or 4-byte offset
/// Returns size after compression
///
inline unsigned multBaseCompression(long long unsigned *values, unsigned size, unsigned blimit, unsigned bsize) {
    unsigned long long limit = 0;
    unsigned BASES = 2;
    //define the appropriate size for the mask
    switch (blimit) {
    case 1:
        limit = 0xFF;
        break;
    case 2:
        limit = 0xFFFF;
        break;
    case 4:
        limit = 0xFFFFFFFF;
        break;
    default:
        //std::cout << "Wrong blimit value = " <<  blimit << std::endl;
        exit(1);
    }
    // finding bases: # BASES
    //std::vector<unsigned long long> mbases;
    //mbases.push_back(values[0]); //add the first base
    unsigned long long mbases[64];
    unsigned baseCount = 1;
    mbases[0] = 0;
    unsigned int i, j;
    for (i = 0; i < size; i++) {
        for (j = 0; j < baseCount; j++) {
            if (my_llabs((long long int)(mbases[j] - values[i])) > limit) {
                //mbases.push_back(values[i]); // add new base
                mbases[baseCount++] = values[i];
            }
        }
        if (baseCount >= BASES) //we don't have more bases
            break;
    }
    // find how many elements can be compressed with mbases
    unsigned compCount = 0;
    for (i = 0; i < size; i++) {
        //ol covered = 0;
        for (j = 0; j < baseCount; j++) {
            if (my_llabs((long long int)(mbases[j] - values[i])) <= limit) {
                compCount++;
                break;
            }
        }
    }
    //return compressed size
    unsigned mCompSize = blimit * compCount + bsize * (BASES - 1) + (size - compCount) * bsize;
    if (compCount < size)
        return size * bsize;
    return mCompSize;
}

inline unsigned BDICompress(char *buffer, unsigned _blockSize) {
    long long unsigned *values = convertBuffer2Array(buffer, _blockSize, 8);
    unsigned bestCSize = _blockSize;
    unsigned currCSize = _blockSize;
    if (isZeroPackable(values, _blockSize / 8))
        bestCSize = 1;
    if (isSameValuePackable(values, _blockSize / 8))
        currCSize = 8;
    bestCSize = bestCSize > currCSize ? currCSize : bestCSize;
    currCSize = multBaseCompression(values, _blockSize / 8, 1, 8);
    bestCSize = bestCSize > currCSize ? currCSize : bestCSize;
    currCSize = multBaseCompression(values, _blockSize / 8, 2, 8);
    bestCSize = bestCSize > currCSize ? currCSize : bestCSize;
    currCSize = multBaseCompression(values, _blockSize / 8, 4, 8);
    bestCSize = bestCSize > currCSize ? currCSize : bestCSize;
    free(values);
    values = convertBuffer2Array(buffer, _blockSize, 4);
    if (isSameValuePackable(values, _blockSize / 4))
        currCSize = 4;
    bestCSize = bestCSize > currCSize ? currCSize : bestCSize;
    currCSize = multBaseCompression(values, _blockSize / 4, 1, 4);
    bestCSize = bestCSize > currCSize ? currCSize : bestCSize;
    currCSize = multBaseCompression(values, _blockSize / 4, 2, 4);
    bestCSize = bestCSize > currCSize ? currCSize : bestCSize;
    free(values);
    values = convertBuffer2Array(buffer, _blockSize, 2);
    currCSize = multBaseCompression(values, _blockSize / 2, 1, 2);
    bestCSize = bestCSize > currCSize ? currCSize : bestCSize;
    free(values);

    buffer = NULL;
    values = NULL;
    return bestCSize;
}

inline unsigned FPCCompress(char *buffer, unsigned size) {
    long long unsigned *values = convertBuffer2Array(buffer, size * 4, 4);
    unsigned compressable = 0;
    unsigned int i;
    for (i = 0; i < size; i++) {

        // 000
        if (values[i] == 0) {
            compressable += 1; //SIM_printf("000\n ");
            continue;
        }
        // 001 010
        if (my_abs((int)(values[i])) <= 0xFF) {
            compressable += 1; //SIM_printf("001\n ");
            continue;
        }
        // 011
        if (my_abs((int)(values[i])) <= 0xFFFF) {
            compressable += 2; //SIM_printf("011\n ");
            continue;
        }
        //100
        if (((values[i]) & 0xFFFF) == 0) {
            compressable += 2; //SIM_printf("100\n ");
            continue;
        }
        //101
        if (my_abs((int)((values[i]) & 0xFFFF)) <= 0xFF && my_abs((int)((values[i] >> 16) & 0xFFFF)) <= 0xFF) {
            compressable += 2; //SIM_printf("101\n ");
            continue;
        }
        //110
        unsigned byte0 = (values[i]) & 0xFF;
        unsigned byte1 = (values[i] >> 8) & 0xFF;
        unsigned byte2 = (values[i] >> 16) & 0xFF;
        unsigned byte3 = (values[i] >> 24) & 0xFF;
        if (byte0 == byte1 && byte0 == byte2 && byte0 == byte3) {
            compressable += 1; //SIM_printf("110\n ");
            continue;
        }
        //111
        compressable += 4;
        //SIM_printf("111\n ");
    }
    free(values);
    //6 bytes for 3 bit per every 4-byte word in a 64 byte cache line
    unsigned compSize = compressable + size * 3 / 8;
    if (compSize < size * 4) return compSize;
    else return size * 4;
}

inline unsigned GeneralCompress(char *buffer, unsigned _blockSize, unsigned compress)
{
    switch (compress) {
    case 0:
        return _blockSize;
        break;
    case 1:
        return BDICompress(buffer, _blockSize);
        break;
    case 2:
        //std::cout << "block-size: " << _blockSize << "\n";
        return FPCCompress(buffer, _blockSize / 4);
        break;
    case 3: {
        unsigned BDISize = BDICompress(buffer, _blockSize);
        unsigned FPCSize = FPCCompress(buffer, _blockSize / 4);
        if (BDISize <= FPCSize)
            return BDISize;
        else
            return FPCSize;
        break;
    }
    default:
        //std::cout << "Unknown compression code: " << compress << "\n";
        exit(1);
    }
}

};

#endif
