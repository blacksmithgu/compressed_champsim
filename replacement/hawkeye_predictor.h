//Hawkeye Cache Replacement Tool v2.0
//UT AUSTIN RESEARCH LICENSE (SOURCE CODE)
//The University of Texas at Austin has developed certain software and documentation that it desires to
//make available without charge to anyone for academic, research, experimental or personal use.
//This license is designed to guarantee freedom to use the software for these purposes. If you wish to
//distribute or make other use of the software, you may purchase a license to do so from the University of
//Texas.
///////////////////////////////////////////////
//                                            //
//     Hawkeye [Jain and Lin, ISCA' 16]       //
//     Akanksha Jain, akanksha@cs.utexas.edu  //
//                                            //
///////////////////////////////////////////////

#ifndef PREDICTOR_H
#define PREDICTOR_H

using namespace std;

#include <iostream>

#include <math.h>
#include <set>
#include <vector>
#include <map>

uint64_t CRC( uint64_t _blockAddress )
{
    static const unsigned long long crcPolynomial = 3988292384ULL;
    unsigned long long _returnVal = _blockAddress;
    for( unsigned int i = 0; i < 32; i++ )
        _returnVal = ( ( _returnVal & 1 ) == 1 ) ? ( ( _returnVal >> 1 ) ^ crcPolynomial ) : ( _returnVal >> 1 );
    return _returnVal;
}


class HAWKEYE_PC_PREDICTOR
{
    map<uint64_t, short unsigned int > SHCT;

       public:

    void increment (uint64_t pc)
    {
        uint64_t signature = CRC(pc) % SHCT_SIZE;
        if(SHCT.find(signature) == SHCT.end())
            SHCT[signature] = (1+MAX_SHCT)/2;

        SHCT[signature] = (SHCT[signature] < MAX_SHCT) ? (SHCT[signature]+1) : MAX_SHCT;

    }

    void saturate (uint64_t pc)
    {
        uint64_t signature = CRC(pc) % SHCT_SIZE;
        assert(SHCT.find(signature) != SHCT.end());

        SHCT[signature] = MAX_SHCT;

    }


    void decrement (uint64_t pc)
    {
        uint64_t signature = CRC(pc) % SHCT_SIZE;
        if(SHCT.find(signature) == SHCT.end())
            SHCT[signature] = (1+MAX_SHCT)/2;
        if(SHCT[signature] != 0)
            SHCT[signature] = SHCT[signature]-1;
    }

    bool get_prediction (uint64_t pc)
    {
        uint64_t signature = CRC(pc) % SHCT_SIZE;
        if(SHCT.find(signature) != SHCT.end() && SHCT[signature] < ((MAX_SHCT+1)/2))
            return false;
        return true;
    }
};

class HAWKEYE_IDEALPC_PREDICTOR
{
    map<uint64_t, uint32_t> detraining_count;
    map<uint64_t, uint32_t> training_count;
    map<uint64_t, uint32_t> prediction_count;
    map<uint64_t, uint32_t> positive_count;
    map<uint64_t, uint32_t> negative_count;

    public:
    void increment (uint64_t pc)
    {
        if(training_count.find(pc) == training_count.end())
        {
            detraining_count[pc] = 0;
            training_count[pc] = 0;
            positive_count[pc] = 0;
            negative_count[pc] = 0;
        }

        training_count[pc]++;
        positive_count[pc]++;
    }

    void decrement (uint64_t pc)
    {
        if(training_count.find(pc) == training_count.end())
        {
            detraining_count[pc] = 0;
            training_count[pc] = 0;
            prediction_count[pc] = 0;
            positive_count[pc] = 0;
            negative_count[pc] = 0;
        }

        training_count[pc]++;
        negative_count[pc]++;
    }

    void detrain (uint64_t pc)
    {
        if(training_count.find(pc) == training_count.end())
        {
            detraining_count[pc] = 0;
            training_count[pc] = 0;
            prediction_count[pc] = 0;
            positive_count[pc] = 0;
            negative_count[pc] = 0;
        }

        detraining_count[pc]++;
    }


    bool get_prediction (uint64_t pc)
    {
        if(training_count.find(pc) == training_count.end())
            return true;

        if(training_count[pc] == 0)
            return true;
        //cout << "Prediction: "<< hex << PC << " " << training_count[PC] << " " << (positive_count[PC] >= negative_count[PC]) << endl;
        //if(positive_count[pc] >= negative_count[pc])
        if(positive_count[pc] >= (detraining_count[pc] + negative_count[pc]))
            return true;
        return false;
    }

    double get_probability (uint64_t pc)
    {
        if(training_count.find(pc) == training_count.end())
            return 2; 

        if(training_count[pc] == 0)
            return 2;
        //cout << "Prediction: "<< hex << PC << " " << training_count[PC] << " " << (positive_count[PC] >= negative_count[PC]) << endl;
        return ((double)positive_count[pc]/(double)(training_count[pc]));

    }

    double get_detrain_probability (uint64_t pc)
    {
        if(training_count.find(pc) == training_count.end())
            return 2; 

        if(training_count[pc] == 0)
            return 2;
        //cout << "Prediction: "<< hex << PC << " " << training_count[PC] << " " << (positive_count[PC] >= negative_count[PC]) << endl;
        return ((double)positive_count[pc]/(double)(training_count[pc] + detraining_count[pc]));

    }

};


#endif
