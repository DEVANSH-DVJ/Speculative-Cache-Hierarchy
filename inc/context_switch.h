#ifndef CONTEXT_H
#define CONTEXT_H

#include "cache.h"

#define NUM_PROCESSES 2
    
class S_BIT_DATA {
    public:
        bool s_bits_L1[L1D_SET][L1D_WAY], 
            s_bits_L2[L2C_SET][L2C_WAY], 
            s_bits_LLC[LLC_SET][LLC_WAY];
        
        uint32_t ts_L1[L1D_SET][L1D_WAY], 
            ts_L2[L2C_SET][L2C_WAY], 
            ts_LLC[LLC_SET][LLC_WAY];

        S_BIT_DATA()
        {
            for (uint32_t set = 0; set < L1D_SET; set++)
            {
                for (uint32_t way = 0; way < L1D_WAY; way++)
                {    
                    s_bits_L1[set][way] = false;
                    ts_L1[set][way] = 0;
                }
            }
            for (uint32_t set = 0; set < L2C_SET; set++)
            {
                for (uint32_t way = 0; way < L2C_WAY; way++)
                {    
                    s_bits_L2[set][way] = false;
                    ts_L2[set][way] = 0;
                }
            }
            for (uint32_t set = 0; set < LLC_SET; set++)
            {
                for (uint32_t way = 0; way < LLC_WAY; way++)
                {    
                    s_bits_LLC[set][way] = false;
                    ts_LLC[set][way] = 0;
                }
            }
        }
};

class PROCESS {
    public:
        uint8_t pid;
        S_BIT_DATA s_data; 

    PROCESS()
    {
        s_data = S_BIT_DATA();
    }

    void save_s_data (CACHE *L1, CACHE *L2, CACHE *L3)
    {
        for (uint32_t set = 0; set < L1D_SET; set++)
        {
            for (uint32_t way = 0; way < L1D_WAY; way++)
            {    
                s_data.s_bits_L1[set][way] = L1->block[set][way].s_bit;
                s_data.ts_L1[set][way] = L1->block[set][way].timestamp;
            }
        }
        for (uint32_t set = 0; set < L2C_SET; set++)
        {
            for (uint32_t way = 0; way < L2C_WAY; way++)
            {    
                s_data.s_bits_L2[set][way] = L2->block[set][way].s_bit;
                s_data.ts_L2[set][way] = L2->block[set][way].timestamp;
            }
        }
        for (uint32_t set = 0; set < LLC_SET; set++)
        {
            for (uint32_t way = 0; way < LLC_WAY; way++)
            {    
                s_data.s_bits_LLC[set][way] = L3->block[set][way].s_bit;
                s_data.ts_LLC[set][way] = L3->block[set][way].timestamp;
            }
        }
    }
    void load_s_data (CACHE *L1, CACHE *L2, CACHE *L3)
    {
        for (uint32_t set = 0; set < L1D_SET; set++)
        {
            for (uint32_t way = 0; way < L1D_WAY; way++)
            {    
                bool s_bit = s_data.s_bits_L1[set][way];
                uint32_t Ts = s_data.ts_L1[set][way];
                uint32_t Tc = L1->block[set][way].timestamp;
                L1->block[set][way].s_bit = (s_bit && (Ts >= Tc));
                L1->block[set][way].timestamp = Ts;
            }
        }
        for (uint32_t set = 0; set < L2C_SET; set++)
        {
            for (uint32_t way = 0; way < L2C_WAY; way++)
            {    
                bool s_bit = s_data.s_bits_L2[set][way];
                uint32_t Ts = s_data.ts_L2[set][way];
                uint32_t Tc = L2->block[set][way].timestamp;
                L2->block[set][way].s_bit = (s_bit && (Ts >= Tc));
                L2->block[set][way].timestamp = Ts;
            }
        }
        for (uint32_t set = 0; set < LLC_SET; set++)
        {
            for (uint32_t way = 0; way < LLC_WAY; way++)
            {   
                bool s_bit = s_data.s_bits_LLC[set][way];
                uint32_t Ts = s_data.ts_LLC[set][way];
                uint32_t Tc = L3->block[set][way].timestamp;
                L3->block[set][way].s_bit = (s_bit && (Ts >= Tc));
                L3->block[set][way].timestamp = Ts;
            }
        }
    }
};

#endif