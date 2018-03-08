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


////////////////////////////////////////////////
// This file is shared by all versions of hawkeye
// please pay attention to the optgen file
// choose
// optgen_simple.h
// or
// optgen.h and OPTGEN_VECTOR_SIZE 128
// please be consistent with the parent hawkeye_ml file
////////////////////////////////////////////////
#ifndef PREDICTOR_H
#define PREDICTOR_H

using namespace std;

#include <iostream>

#include <math.h>
#include <set>
#include <vector>
#include <map>
#include <set>
#include <limits>
#include <algorithm>
#include "cache.h"

#define CONF_RATIO 0.2 //CONF_RATIO is the second parameters concerning confidence
double step_size = 1.776;
double lambda = 512;
double threshold_coeff = 100;


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

    short unsigned int get_probability (uint64_t pc)
    {
        uint64_t signature = CRC(pc) % SHCT_SIZE;
        if(SHCT.find(signature) != SHCT.end())
            return SHCT[signature];
        return -1;
    }
};

class BASE_PERCEPTRON
{
public:
    map<uint64_t, map<uint64_t, double>> weights;
    map<uint64_t, double> bias;

    double training_threshold; // training_coeff: initial threshold
    int positive_count = 0; // count for accuracy, reset every interval (1024 accesses)

    double dot_product(uint64_t X, set<uint64_t>& s)
    {
        double d_product = bias[X];
        for(auto iter = s.begin(); iter != s.end(); iter++)
        {
            uint64_t A = *iter;
            if(weights[X].find(A) == weights[X].end())
            {
                weights[X][A] = 0;
            }
            d_product += weights[X][A];
        }
        return d_product;
    }

    // update weights, increment or decrement
    void update(uint64_t X, int32_t coeff, set<uint64_t>& s)
    {
        bias[X] += coeff;
        for(auto iter = s.begin(); iter != s.end(); iter++)
        {
            uint64_t A = *iter;
            weights[X][A] = weights[X][A] + coeff;
        }
    }

    void copy(BASE_PERCEPTRON* target){
        weights = target->weights;
        bias = target->bias;
        training_threshold = target->training_threshold;
        positive_count = target->positive_count;
    }
};

class MULTIPLE_PERCEPTRON_PREDICTOR
{
public:

    MULTIPLE_PERCEPTRON_PREDICTOR(int num_agent){
        this->num_agent = num_agent;
        agent = new BASE_PERCEPTRON[num_agent];
        agent[0].training_threshold = 30;
        for (int i = 1; i < num_agent; i++){
            agent[i].training_threshold = agent[i-1].training_threshold * step;
        }
	    current = num_agent / 2;
        for(int i = 0; i < 51; i++)
        {
            confidence_histogram_up[i] = 0;
            confidence_histogram_down[i] = 0;
        }
        cout << "Dynamic Perceptron" << endl;
        cout << "interval size: " << interval_size << endl;
        cout << "step size: " << step << endl;
        cout << "initial threshold: " << threshold_coeff << "(useless)" << endl;
    }

    HAWKEYE_PC_PREDICTOR* baseline_hawkeye_predictor = new HAWKEYE_PC_PREDICTOR();

    const double step = step_size;
    map<uint64_t, bool> paddr_this_interval; //store all data addresses this interval
    const int interval_size = lambda;  //interval length
    int training_count = 0;  //current progress into this interval
    double past_thresholds[100000]; // store past thresholds
    int num_intervals = 0;
    int num_agent;
    int current;
    bool debug = false;
    bool debug_hawkeye = false;

    double confidence_histogram_up[51];
    double confidence_histogram_down[51];

    BASE_PERCEPTRON* agent;

    void update_threshold()
    {
        // store threshold of last interval
        past_thresholds[num_intervals] = agent[current].training_threshold;
        num_intervals++;

        cout << "Accuracy this interval: " << endl;
        for (int i = 0; i < num_agent; i++){
            cout << agent[i].training_threshold << " " << agent[i].positive_count << " " << training_count << endl;
        }
        cout << endl;

        // clear data addresses and training count for this interval
        paddr_this_interval.clear();
        training_count = 0;
        
        int max_id = current;
        for (int i = 0; i < num_agent; i++){
            if (agent[i].positive_count > agent[max_id].positive_count){
                max_id = i;
            }
        }
        assert(max_id != -1);
	    double new_threshold = agent[max_id].training_threshold;
	    int offset = max_id - current;
    	if (offset > 0){
    		// going up
    		for (int i = num_agent-1; i >= offset; i--){
    			agent[i] = agent[i-offset];
    		}
    		//agent[2] = agent[1];
    		//agent[1] = agent[0];
    		//agent[0] = agent[0];
    	} else {
    		for (int i = 0; i < num_agent+offset; i++){
    			agent[i] = agent[i-offset]; // offset < 0 here
    		}
    		//agent[0] = agent[1];
    		//agent[1] = agent[2];
    		//agent[2] = agent[2];
    	}
    	agent[current].training_threshold = new_threshold;
    	for (int i = current+1; i < num_agent; i++){
    		agent[i].training_threshold = agent[i-1].training_threshold * step;
    	}
    	for (int i = current-1; i >= 0; i--){
    		agent[i].training_threshold = agent[i+1].training_threshold / step;
    		if (agent[i].training_threshold < 1.0){
    			agent[i].training_threshold = 1.0;
    		}
    	}
    	for (int i = 0; i < num_agent; i++){
    		agent[i].positive_count = 0;
    	}
    }

    void increment (uint64_t pc, 
        vector<uint64_t> pc_history, 
        uint64_t paddr, 
        Result& prev_result, 
		bool detrained)
    {
        if (debug) {
            cout << "increment" << endl;
        }
        baseline_hawkeye_predictor->increment(pc);

        if(debug_hawkeye)
            return;

        if(debug_hawkeye)
            assert(0);

        if(pc_history.size() == 0)
            return;

        bool inside_interval = (paddr_this_interval.find(paddr) != paddr_this_interval.end());

    	if (training_count == interval_size){
            update_threshold();
    	}

    	if(inside_interval){
    		training_count++;
    		if (detrained){
    			training_count--;
    		}
    	}

        uint64_t X = pc;
        bool ground_truth = true;
        int32_t coeff = ground_truth ? 1 : -1;
        double d_product;
        bool prediction;

        if(agent[current].weights.find(X) == agent[current].weights.end())
        {
            for (int i = 0; i < num_agent; i++){
                agent[i].weights[X].clear();
                agent[i].bias[X] = 0;
            }
        }

        set<uint64_t> s(pc_history.begin(), pc_history.end());

    	for (int i = 0; i < num_agent; i++){
    		d_product = agent[i].dot_product(X, s);
    		prediction = d_product > 0;
    		if(prediction == ground_truth && abs(d_product) >= agent[i].training_threshold) 
    			;
    		else
    			agent[i].update(X, coeff, s);
    		// update accuracy counter for base predictor based on its last prediction
    		if (inside_interval){
    			if (detrained){
    				if (prev_result.prediction[i] == 0){
    					agent[i].positive_count--;
    				}
    			}
    			if (prev_result.prediction[i] > 0){
    				agent[i].positive_count++;
    			}
    		}
    	}
        if (debug) {
            cout << "increment done" << endl;
        }

        if(prev_result.confidence > 0)
        {
            int level = 0;
            if(prev_result.confidence < prev_result.threshold)
            {
                level = (int)(20 * prev_result.confidence / prev_result.threshold);
            }
            else
            {
                level = 10 + (int)(10 * prev_result.confidence / prev_result.threshold);
            }
            level = level > 50 ? 50 : level;
            confidence_histogram_up[level]++;
            confidence_histogram_down[level]++;
            if(detrained)
            {
                confidence_histogram_down[level]--;
            }
        }
    }

    void decrement (uint64_t pc, 
        vector<uint64_t> pc_history, 
        uint64_t paddr, 
        Result& prev_result,
	    bool detrained)
    {
        if (debug){
            cout << "decrement" << endl;
        }
        baseline_hawkeye_predictor->decrement(pc);

        if(debug_hawkeye)
            return;

        if(debug_hawkeye)
            assert(0);

        if(pc_history.size() == 0) 
            return;

        bool inside_interval = (paddr_this_interval.find(paddr) != paddr_this_interval.end());

    	if (training_count == interval_size)
                update_threshold();
		
        uint64_t X = pc;
        bool ground_truth = false;
        int32_t coeff = ground_truth ? 1 : -1;
        double d_product;
        bool prediction;
        if(inside_interval){
            training_count++;
	    	if (detrained){
				training_count--;
			}
		}

        if(agent[current].weights.find(X) == agent[current].weights.end())
        {
            for (int i = 0; i < num_agent; i++){
                agent[i].weights[X].clear();
                agent[i].bias[X] = 0;
            }
        }

        set<uint64_t> s(pc_history.begin(), pc_history.end());
        
        //train predictors
        for (int i = 0; i < num_agent; i++){
            d_product = agent[i].dot_product(X, s);
            prediction = d_product > 0;
            if(prediction == ground_truth && abs(d_product) >= agent[i].training_threshold) 
                ;
            else
                agent[i].update(X, coeff, s);
            // update accuracy counter for base predictor based on its last prediction
			if (inside_interval){
				if (detrained){
					if (prev_result.prediction[i] == 0){
						agent[i].positive_count--;
					}
				}
				if (prev_result.prediction[i] == 0){
					agent[i].positive_count++;
				}
			}
        }
        if (debug){
            cout << "decrement done" << endl;
        }

        if(prev_result.confidence > 0)
        {
            int level = 0;
            if(prev_result.confidence < prev_result.threshold)
            {
                level = (int)(20 * prev_result.confidence / prev_result.threshold);
            }
            else
            {
                level = 10 + (int)(10 * prev_result.confidence / prev_result.threshold);
            }
            level = level > 50 ? 50 : level;
            //confidence_histogram_up[level]++;
            confidence_histogram_down[level]++;
            if(detrained)
            {
                confidence_histogram_down[level]--;
            }
        }
    }

    Result get_prediction (uint64_t pc, vector<uint64_t> pc_history, uint64_t paddr)
    {
        if (debug){
            cout << "get prediction2" << endl;
        }
        bool baseline_prediction = baseline_hawkeye_predictor->get_prediction(pc);
        if(debug_hawkeye || pc_history.size() == 0 || agent[current].weights.find(pc) == agent[current].weights.end())
        {
            Result result(current, num_agent);
            for (int i = 0; i < num_agent; i++){
                result.prediction[i] = baseline_prediction;
            }
            if (debug){
                cout << "get prediction2 done2" << endl;
            }
            return result;
        }
        
        if(debug_hawkeye)
            assert(0);

        if(paddr_this_interval.find(paddr) == paddr_this_interval.end())
            paddr_this_interval[paddr] = true;

        uint64_t X = pc;
        set<uint64_t> s(pc_history.begin(), pc_history.end());
        
        Result result(current, num_agent);
        // for histogram
        result.confidence = agent[current].dot_product(X, s);
        result.threshold = agent[current].training_threshold;

        for (int i = 0; i < num_agent; i++){
            double confidence = agent[i].dot_product(X, s);
            int prediction;
            // now we make predictions based on confidence vs. threshold
            if(confidence <= 0) // if below 0, it's a cache-averse line
            {    
                prediction = 0;
            }
            // if above 0, below threshold * 0.2 (the ratio, which is a hyperparameter), it's a line in the middle
            else if(confidence < agent[i].training_threshold * CONF_RATIO)
            {
                prediction = 1;
            }
            else // confidence >= agent[i].training_threshold, it's a cache-friendly line
            {
                prediction = 2;
            }
            result.prediction[i] = prediction;
        }
        if (debug){
            cout << "get prediction2 done1" << endl;
        }
        return result;
    }
};

class MULTI_CORE_MULTIPLE_PERCEPTRON_PREDICTOR
{
public:
    MULTI_CORE_MULTIPLE_PERCEPTRON_PREDICTOR(int num_cpus, int num_agent){
        for(int i = 0; i < num_cpus; i++)
            single_core_predictors[i] = new MULTIPLE_PERCEPTRON_PREDICTOR(num_agent);
    }
    MULTIPLE_PERCEPTRON_PREDICTOR* single_core_predictors[4];
};

#endif
