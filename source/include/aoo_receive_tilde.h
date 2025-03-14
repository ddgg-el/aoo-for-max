#pragma once

#include "ext.h"			// standard Max include, always required (except in Jitter)
#include "ext_obex.h"		// required for "new" style objects
#include "z_dsp.h"			// required for MSP objects

#include "aoo_sink.hpp"
#include "common/net_utils.hpp"

#include "aoo_max_common.hpp"

// t_class *aoo_receive_class;

struct t_source
{
    aoo::ip_address s_address;
    AooId s_id;
    t_symbol *s_group;
    t_symbol *s_user;
};

struct t_stream_message
{
    t_stream_message(const AooStreamMessage& msg, const AooEndpoint& ep)
        : address((const sockaddr *)ep.address, ep.addrlen), id(ep.id),
          channel(msg.channel), type(msg.type), data(msg.data, msg.data + msg.size) {}
    aoo::ip_address address;
    AooId id;
    int32_t channel;
    AooDataType type;
    std::vector<AooByte> data;
};

// struct to represent the object's state
struct t_aoo_receive {
	t_pxobject		ob;			// the object itself (t_pxobject in MSP instead of t_object)
	t_outlet *x_msgout = nullptr;
	
	int32_t x_nchannels = 0;
    long x_samplerate = 0;
    long x_blocksize = 0; 
	std::unique_ptr<t_sample *[]> x_vec;

	t_clock *x_clock = nullptr;
	t_clock *x_queue_clock = nullptr;
	t_priority_queue<t_stream_message> x_queue;
	
    bool x_multi = false;
	AooId x_id = 0;
	AooSink::Ptr x_sink;
    int32_t x_port = 0;
	// node
    t_node *x_node = nullptr;
    // sources
    std::vector<t_source> x_sources;

    bool x_valid = true; // if the object is not properly constructed 
	
    t_aoo_receive(int argc, t_atom *argv);
	~t_aoo_receive();
	
    void dispatch_stream_message(const AooStreamMessage& msg, const aoo::ip_address& address, AooId id);
    bool check(const char *name) const;
	bool check(int argc, t_atom *argv, int minargs, const char *name) const;
    bool get_source_arg(int argc, t_atom *argv, aoo::ip_address& addr, AooId& id, bool check) const;
} ;

// MAX MSP method prototypes
void *aoo_receive_new(t_symbol *s, long argc, t_atom *argv);
void aoo_receive_free(t_aoo_receive *x);
void aoo_receive_assist(t_aoo_receive *x, void *b, long m, long a, char *s);
void aoo_receive_dsp64(t_aoo_receive *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void aoo_receive_perform64(t_aoo_receive *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);

long aoo_receive_multichanneloutputs(t_aoo_receive *x, int index);