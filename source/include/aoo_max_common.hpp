#pragma once

#include "ext.h"
#include "z_dsp.h"

#include "aoo.h"
#include "aoo_types.h"
#include "common/time.hpp"
#include "common/utils.hpp"
#include "common/bit_utils.hpp"
#include "common/priority_queue.hpp"

#include "codec/aoo_pcm.h"
#include "codec/aoo_null.h"
#if AOO_USE_OPUS
#include "codec/aoo_opus.h"
#endif

static AooNtpTime g_start_time;
double get_elapsed_ms(AooNtpTime tt);

void format_makedefault(AooFormatStorage &f, int nchannels);

int format_to_atoms(const AooFormat &f, int argc, t_atom *argv);

int datatype_element_size(AooDataType type);

int data_to_atoms(const AooData& data, int argc, t_atom *argv);

int stream_message_to_atoms(const AooStreamMessage& msg, int argc, t_atom *argv);

double clock_getsystimeafter(double deltime);
///////////////////////////// priority queue ////////////////////////////////

template<typename T>
struct t_queue_item {
    template<typename U>
    t_queue_item(U&& _data, double _time)
        : data(std::forward<U>(_data)), time(_time) {}
    T data;
    double time;
};

template<typename T>
bool operator> (const t_queue_item<T>& a, const t_queue_item<T>& b) {
    return a.time > b.time;
}

template<typename T>
using t_priority_queue = aoo::priority_queue<t_queue_item<T>, std::greater<t_queue_item<T>>>;

