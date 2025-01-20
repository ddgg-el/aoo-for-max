#pragma once

#include "ext.h"			// standard Max include
#include "z_dsp.h"			// required for MSP objects

// #include "aoo_max_common.hpp"
#include "common/time.hpp"

#define DEJITTER_TOLERANCE 0.1 // jitter tolerance in percent
#define DEJITTER_MAXDELTA 0.02 // max. expected jitter in seconds
#define DEJITTER_CHECK 1 // extra checks
#define DEJITTER_DEBUG 0 // debug dejitter algorithm

t_class *dejitter_class;

struct t_dejitter {
    static constexpr const char *bindsym = "aoo dejitter";

    t_dejitter();
    ~t_dejitter();

    double osctime() const {
        return d_osctime_adjusted;
    }
    t_object ob;            // The object itself (t_object in Max/MSP)

    void update();
    // static void tick(t_dejitter *x);
    static void tick(t_dejitter *x) {
        x->update();
        clock_delay(x->d_clock, sys_getblksize()); // once per DSP tick
    }

    int d_refcount;        // Reference count
private:
    void *d_clock;          // Pointer to a Max clock object
    aoo::time_tag d_last_osctime;  // Last recorded OSC time
    aoo::time_tag d_osctime_adjusted; // Adjusted OSC time
    double d_last_big_delta = 0; // Last big delta time
#if DEJITTER_CHECK
    double d_last_logical_time = -1;
#endif
};
