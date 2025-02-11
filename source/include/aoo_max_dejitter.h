#pragma once

#include "ext.h"			// standard Max include
// #include "ext_obex.h"        // required for new style Max object
#include "z_dsp.h"			// required for MSP objects

// #include "aoo_max_common.hpp"
#include "common/time.hpp"

extern t_class *aoo_dejitter_class;

#define DEJITTER_TOLERANCE 0.1 // jitter tolerance in percent
#define DEJITTER_MAXDELTA 0.02 // max. expected jitter in seconds
#define DEJITTER_CHECK 1 // extra checks
#define DEJITTER_DEBUG 0 // debug dejitter algorithm

class t_dejitter {
public:
    t_object obj;
    static constexpr const char *bindsym = "aoo dejitter";


    t_dejitter();
    ~t_dejitter();
    
    aoo::time_tag osctime() const {
        return d_osctime_adjusted;
    }
    t_class* d_header;
    int d_refcount;        // Reference count
private:
    t_clock *d_clock;          // Pointer to a Max clock object
    aoo::time_tag d_last_osctime;  // Last recorded OSC time
    aoo::time_tag d_osctime_adjusted; // Adjusted OSC time
    double d_last_big_delta = 0; // Last big delta time
#if DEJITTER_CHECK
    double d_last_logical_time = -1;
#endif
    void update();
    
    static void tick(t_dejitter *x) {
        x->update();
        clock_delay(x->d_clock, sys_getblksize()); // once per DSP tick
    }
};

t_dejitter * dejitter_get(); 
uint64_t dejitter_osctime(t_dejitter *x);

void dejitter_release(t_dejitter *x);

void aoo_dejitter_setup(void);
void *aoo_dejitter_new(t_symbol *s, long argc, t_atom *argv);
void aoo_dejitter_free(t_dejitter *x);
