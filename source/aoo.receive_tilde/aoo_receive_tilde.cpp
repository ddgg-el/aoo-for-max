/**
	@file
	aoo.receive~: a simple audio object for Max
	original by: jeremy bernstein, jeremy@bootsquad.com
	@ingroup examples
*/

#include "ext.h"			// standard Max include, always required (except in Jitter)
#include "ext_obex.h"		// required for "new" style objects
#include "z_dsp.h"			// required for MSP objects
#include <vector>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include "aoo_common.hpp"


// for hardware buffer sizes up to 1024 @ 44.1 kHz
#define DEFAULT_LATENCY 25

// "fake" stream message types
const int kAooDataStreamTime = -3; // AooEventStreamTime
const int kAooDataStreamState = -2; // AooEventStreamState
// NB: kAooDataUnspecified = -1


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
struct t_aoo_receive_tilde {
	t_pxobject		ob;			// the object itself (t_pxobject in MSP instead of t_object)
	t_outlet *x_msgout = nullptr;
	
	int32_t x_nchannels = 0;
	std::unique_ptr<t_sample *[]> x_vec;

	t_clock *x_clock = nullptr;
	t_clock *x_queue_clock = nullptr;
	t_priority_queue<t_stream_message> x_queue;
	
    bool x_multi = false;
	AooId x_id = 0;
	AooSink::Ptr x_sink;
	// node
    t_node *x_node = nullptr;
	
    t_aoo_receive_tilde(int argc, t_atom *argv);
	~t_aoo_receive_tilde();
	
    void dispatch_stream_message(const AooStreamMessage& msg, const aoo::ip_address& address, AooId id);
	
} ;

static void aoo_receive_tick(t_aoo_receive_tilde *x)
{
    x->x_sink->pollEvents();
}


static void aoo_receive_queue_tick(t_aoo_receive_tilde *x)
{
    auto& queue = x->x_queue;
    auto now = gettime();
	post("daje");
    while (!queue.empty()){
        if (queue.top().time <= now) {
            auto& m = queue.top().data;
            AooStreamMessage msg { 0, m.channel, m.type,
                                 (int32_t)m.data.size(), m.data.data() };
            x->dispatch_stream_message(msg, m.address, m.id);
            queue.pop();
        } else {
            break;
        }
    }
    // reschedule
    if (!queue.empty()){
        clock_set(x->x_queue_clock, queue.top().time);
    }
}


t_aoo_receive_tilde::t_aoo_receive_tilde(int argc, t_atom *argv)
{
    x_clock = clock_new(this, (method)aoo_receive_tick);
    x_queue_clock = clock_new(this, (method)aoo_receive_queue_tick);

    // // flags
    // while (argc && argv->a_type == A_SYM) {
    //     auto flag = argv->a_w.w_sym->s_name;
    //     if (*flag == '-') {
    //         if (!strcmp(flag, "-m")) {
    //             if (g_signal_setmultiout) {
    //                 x_multi = true;
    //             } else {
    //                 pd_error(this, "%s: no multi-channel support, ignoring '-m' flag", classname(this));
    //             }
    //         } else {
    //             pd_error(this, "%s: ignore unknown flag '%s",
    //                      classname(this), flag);
    //         }
    //         argc--; argv++;
    //     } else {
    //         break;
    //     }
    // }

    // arg #1: channels
    int noutlets;
    if (x_multi) {
        noutlets = 1;
        // NB: the channel count cannot be zero!
        x_nchannels = std::max<int>(atom_getfloatarg(0, argc, argv), 1);
    } else {
        // NB: users may explicitly specify 0 channels for pure message streams!
        noutlets = argc > 0 ? atom_getfloat(argv) : 1;
        if (noutlets < 0) {
            noutlets = 0;
        } else if (noutlets > AOO_MAX_NUM_CHANNELS) {
            // NB: in theory we can support any number of channels;
            // this rather meant to handle patches that accidentally
            // use the old argument order where the port would come first!
            object_error((t_object*)this, "%s: channel count (%d) out of range",
                     object_classname(this), noutlets);
            noutlets = 0;
        }
        x_nchannels = noutlets;
    }

    // arg #2 (optional): port number
    // NB: 0 means "don't listen"
    int port = atom_getfloatarg(1, argc, argv);

    // arg #3 (optional): ID
    AooId id = atom_getfloatarg(2, argc, argv);
    if (id < 0) {
        object_error((t_object*)this, "%s: bad id % d, setting to 0", object_classname(this), id);
        id = 0;
    }
    x_id = id;

    // arg #4 (optional): latency (ms)
    float latency = argc > 3 ? atom_getfloat(argv + 3) : DEFAULT_LATENCY;

    // event outlet
    x_msgout = outlet_new(this, 0);

    // make signal outlets
    for (int i = 0; i < noutlets; ++i){
        outlet_new(this, "signal");
    }
    // channel vector
    if (x_nchannels > 0) {
        x_vec = std::make_unique<t_sample *[]>(x_nchannels);
    }


    // create and initialize AooSink object
    x_sink = AooSink::create(x_id);

	//TODO 
    // // set event handler
    // x_sink->setEventHandler((AooEventHandler)aoo_receive_handle_event,
    //                          this, kAooEventModePoll);

    x_sink->setLatency(latency * 0.001);

	//TODO 
    // finally we're ready to receive messages
    // aoo_receive_port(this, port);
}

t_aoo_receive_tilde::~t_aoo_receive_tilde()
{
    if (x_node){
        x_node->release((t_class *)this, x_sink.get());
    }
    clock_free(x_clock);
    clock_free(x_queue_clock);
}


void t_aoo_receive_tilde::dispatch_stream_message(const AooStreamMessage& msg,
                                            const aoo::ip_address& address, AooId id) {
    // 5 extra atoms for endpoint (host, port, ID) + message (channel, type)
    // NB: in case of "fake" stream messages, we just over-allocate.
    auto size = 5 + (msg.size / datatype_element_size(msg.type));
	// un'array dove verrà copiato il messaggio da mandare all'outlet
    auto vec = (t_atom *)alloca(sizeof(t_atom) * size);
	// TODO  
    // if (!x_node->serialize_endpoint(address, id, 3, vec)) {
    //     error("bug: dispatch_stream_message: serialize_endpoint");
    //     return;
    // }
	// comunica attraverso l'outlet lo stato della connesione (credo)
    if (msg.type == kAooDataStreamState) {

        AooStreamState state;
		// blocca l'esecuzione del programma se i due dati sono di dimensioni diverse (misura di sicurezza)
        assert(msg.size == sizeof(state)); // see aoo_receive_handle_event()
		// copia dentro state msg.data, un'informazione che sarà di dimensione sizeof(state)
        memcpy(&state, msg.data, sizeof(state));
		// copia in vec il valore float/int che insica lo "stato" dell'oggetto
        atom_setfloat(vec + 3, state);
		// mandalo fuori dall'outlet (con la stringa "state")
        outlet_anything(x_msgout, gensym("state"), 4, vec);
    } else if (msg.type == kAooDataStreamTime) {
		//se in vece il messaggio è un'informazione legata al tempo
        AooNtpTime tt[2];
		// vedi sopra
        assert(msg.size == sizeof(tt)); // see aoo_receive_handle_event()
		// copia in tt msg.data che sarà di dimensione sizeof(tt)
        memcpy(tt, msg.data, sizeof(tt));
		// copia in vec+3 il tempo trascorso da boh
		// e in vecì4 stessa cosa
        atom_setfloat(vec + 3, get_elapsed_ms(tt[0]));
        atom_setfloat(vec + 4, get_elapsed_ms(tt[1]));
		// e manda in outlet questi due valori
        outlet_anything(x_msgout, gensym("time"), 5, vec);
    } else {
        // message - converti il messaggio in "atom" / quindi fallo diventare un messaggio leggibile in max
        stream_message_to_atoms(msg, size - 3, vec + 3);
		// mandalo in outlet
        outlet_anything(x_msgout, gensym("msg"), size, vec);
    }
}


// method prototypes
void *aoo_receive_tilde_new(t_symbol *s, long argc, t_atom *argv);
void aoo_receive_tilde_free(t_aoo_receive_tilde *x);
void aoo_receive_tilde_assist(t_aoo_receive_tilde *x, void *b, long m, long a, char *s);
void aoo_receive_tilde_dsp64(t_aoo_receive_tilde *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void aoo_receive_tilde_perform64(t_aoo_receive_tilde *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);


// global class pointer variable
static t_class *aoo_receive_tilde_class = NULL;


//***********************************************************************************************

void ext_main(void *r)
{
	// object initialization, note the use of dsp_free for the freemethod, which is required
	// unless you need to free allocated memory, in which case you should call dsp_free from
	// your custom free function.

	t_class *c = class_new("aoo.receive~", (method)aoo_receive_tilde_new, (method)aoo_receive_tilde_free, (long)sizeof(t_aoo_receive_tilde), 0L, A_GIMME, 0);


	class_addmethod(c, (method)aoo_receive_tilde_dsp64,		"dsp64",	A_CANT, 0);
	class_addmethod(c, (method)aoo_receive_tilde_assist,	"assist",	A_CANT, 0);

	class_dspinit(c);
	class_register(CLASS_BOX, c);
	aoo_receive_tilde_class = c;
}


void *aoo_receive_tilde_new(t_symbol *s, long argc, t_atom *argv)
{
	t_aoo_receive_tilde *x = (t_aoo_receive_tilde *)object_alloc(aoo_receive_tilde_class);

	if (x) {
		dsp_setup((t_pxobject *)x, 1);	// MSP inlets: arg is # of inlets and is REQUIRED!
		// use 0 if you don't need inlets
		new (x) t_aoo_receive_tilde(argc, argv);
	}
	return (x);
}


// NOT CALLED!, we use dsp_free for a generic free function
void aoo_receive_tilde_free(t_aoo_receive_tilde *x)
{
    x->~t_aoo_receive_tilde();
}


void aoo_receive_tilde_assist(t_aoo_receive_tilde *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) { //inlet
		sprintf(s, "I am inlet %ld", a);
	}
	else {	// outlet
		sprintf(s, "I am outlet %ld", a);
	}
}


// registers a function for the signal chain in Max
void aoo_receive_tilde_dsp64(t_aoo_receive_tilde *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	post("my sample rate is: %f", samplerate);
	// post(count);

	// instead of calling dsp_add(), we send the "dsp_add64" message to the object representing the dsp chain
	// the arguments passed are:
	// 1: the dsp64 object passed-in by the calling function
	// 2: the symbol of the "dsp_add64" message we are sending
	// 3: a pointer to your object
	// 4: a pointer to your 64-bit perform method
	// 5: flags to alter how the signal chain handles your object -- just pass 0
	// 6: a generic pointer that you can use to pass any additional data to your perform method

	object_method(dsp64, gensym("dsp_add64"), x, aoo_receive_tilde_perform64, 0, NULL);
}


// this is the 64-bit perform method audio vectors
void aoo_receive_tilde_perform64(t_aoo_receive_tilde *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	t_double *inL = ins[0];		// we get audio for each inlet of the object from the **ins argument
	t_double *outL = outs[0];	// we get audio for each outlet of the object from the **outs argument
	int n = sampleframes;

	// this perform method simply copies the input to the output, offsetting the value
	while (n--)
		*outL++ = *inL++;
}


