#include "aoo_max_dejitter.h"

t_class *dejitter_class;

t_dejitter::t_dejitter()
    : d_refcount(1), d_last_osctime(0), d_osctime_adjusted(0), d_last_big_delta(0)
{
    d_clock = clock_new(this, (method)tick);
    // Bind the object to a symbol in Max/MSP
    object_attach_byptr_register(this, this, gensym(bindsym));
}

t_dejitter::~t_dejitter(){
    // pd_unbind(&d_header, gensym(bindsym));
    clock_free(d_clock);
}

// This is called exactly once per DSP tick and before any other clocks in AOO objects.
void t_dejitter::update()
{
#if DEJITTER_CHECK
    // check if this is really called once per DSP tick
    auto logical_time = gettime();
    if (logical_time == d_last_logical_time) {
        error("BUG: aoo dejitter");
    }
    d_last_logical_time = logical_time;
#endif
    aoo::time_tag osctime = aoo::time_tag::now();
    if (d_last_osctime.is_empty()) {
        d_osctime_adjusted = osctime;
    } else {
    #if DEJITTER_DEBUG
        auto last_adjusted = d_osctime_adjusted;
    #endif
        auto delta = aoo::time_tag::duration(d_last_osctime, osctime);
        auto period = (double)sys_getblksize() / (double)sys_getsr();
        // check if the current delta is lower than the nominal delta (with some tolerance).
        // If this is the case, we advance the adjusted OSC time by the nominal delta.
        if ((delta / period) < (1.0 - DEJITTER_TOLERANCE)) {
            // Don't advance if the previous big delta was larger than DEJITTER_MAXDELTA;
            // this makes sure that we catch up if the scheduler blocked.
            if (d_last_big_delta <= DEJITTER_MAXDELTA) {
                d_osctime_adjusted += aoo::time_tag::from_seconds(period);
            } else if (osctime > d_osctime_adjusted) {
                // set to actual time, but only if larger.
                d_osctime_adjusted = osctime;
            }
        } else {
            // set to actual time, but only if larger; never let time go backwards!
            if (osctime > d_osctime_adjusted) {
                d_osctime_adjusted = osctime;
            }
            d_last_big_delta = delta;
        }
    #if DEJITTER_DEBUG
        auto adjusted_delta = aoo::time_tag::duration(last_adjusted, d_osctime_adjusted);
        if (adjusted_delta == 0) {
            // get actual (negative) delta
            adjusted_delta = aoo::time_tag::duration(osctime, last_adjusted);
        }
        auto error = std::abs(adjusted_delta - period);
        LOG_DEBUG("dejitter: real delta: " << (delta * 1000.0)
                  << " ms, adjusted delta: " << (adjusted_delta * 1000.0)
                  << " ms, error: " << (error * 1000.0) << " ms");
        LOG_DEBUG("dejitter: real time: " << osctime
                  << ", adjusted time: " << d_osctime_adjusted);
    #endif
    }
    d_last_osctime = osctime;
}

t_dejitter * dejitter_get() {
    auto x = (t_dejitter *)class_findbyname(CLASS_NOBOX, (t_symbol*)(t_dejitter::bindsym));
    if (x) {
        x->d_refcount++;
    } else {
        x = new t_dejitter();
    }
    return x;
}

void dejitter_release(t_dejitter *x) {
    if (--x->d_refcount == 0) {
        // last instance
        delete x;
    } else if (x->d_refcount < 0) {
        error("BUG: dejitter_release: negative refcount!");
    }
}

uint64_t dejitter_osctime(t_dejitter *x) {
    return x->osctime();
}

// void ext_main(void *r){
//     t_class *c;
//     c = class_new("aoo dejitter", 0, 0,
//                                sizeof(t_dejitter), 0L, A_GIMME, 0);
//     class_register(CLASS_NOBOX, dejitter_class);
//     dejitter_class = c;
// }
