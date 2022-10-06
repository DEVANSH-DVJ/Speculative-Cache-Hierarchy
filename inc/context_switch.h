#ifndef CONTEXT_H
#define CONTEXT_H

#include "cache.h"

#define NUM_PROCESSES 2
    
uint8_t last_proc = 0;

class S_BIT_DATA {
    public:
        bool s_bits[LLC_SET*LLC_WAY];
        uint32_t timestamp[LLC_SET*LLC_WAY];

        S_BIT_DATA()
        {
            for (uint32_t counter = 0; counter < LLC_SET*LLC_WAY; counter++)
            {
                s_bits[i] = false;
                timestamp[i] = 0;
            }
        }
};

class PROCESS {
    public:
        uint8_t pid;
        S_BIT_DATA s_data; 

    PROCESS(uint8_t proc_id)
    {
        pid = proc_id;

    }
};


#endif