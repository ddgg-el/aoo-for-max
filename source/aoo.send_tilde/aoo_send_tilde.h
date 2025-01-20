#pragma once

#include "ext.h"			// standard Max include
#include "ext_obex.h"		// required for new style Max object
#include "z_dsp.h"

#include "aoo.h"
#include "aoo_source.hpp"

#include "aoo_max_common.hpp"
#include "aoo_max_node.h"

#define DEFBUFSIZE 25

// struct to represent the object's state
// not a typedef struct anymore
struct t_aoo_send {
	t_aoo_send(int argc, t_atom *argv);

	t_pxobject		obj;			// the object itself (t_pxobject in MSP instead of t_object)
	double			offset; 	// the value of a property of our object

    int32_t			x_nchannels = 0;
    void 			*x_msgout = nullptr;
    std::unique_ptr<t_double *[]> x_vec;


    AooSource::Ptr 	x_source;
    AooId 			x_id = 0;
    int32_t         x_port = 0;
    t_node          *x_node = nullptr;

    t_symbol 		*x_codec = nullptr;

    t_clock 		*x_clock = nullptr;
    bool			x_multi = false;

};

// MAX MSP method prototypes
void *aoo_send_new(t_symbol *s, long argc, t_atom *argv);
void aoo_send_free(t_aoo_send *x);
void aoo_send_assist(t_aoo_send *x, void *b, long m, long a, char *s);
void aoo_send_float(t_aoo_send *x, double f);
void aoo_send_dsp64(t_aoo_send *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void aoo_send_perform64(t_aoo_send *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
