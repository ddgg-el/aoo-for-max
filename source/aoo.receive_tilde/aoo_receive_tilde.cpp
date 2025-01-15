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
struct t_aoo_receive_tilde {
	t_pxobject		ob;			// the object itself (t_pxobject in MSP instead of t_object)
	t_outlet *x_msgout = nullptr;
	
	int32_t x_nchannels = 0;
    int32_t x_samplerate = 0;
	std::unique_ptr<t_sample *[]> x_vec;

	t_clock *x_clock = nullptr;
	t_clock *x_queue_clock = nullptr;
	t_priority_queue<t_stream_message> x_queue;
	
    bool x_multi = false;
	AooId x_id = 0;
	AooSink::Ptr x_sink;
	// node
    t_node *x_node = nullptr;
    // sources
    std::vector<t_source> x_sources;
	
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

static void aoo_receive_handle_stream_message(t_aoo_receive_tilde *x, const AooStreamMessage *msg, const AooEndpoint *ep)
{
    auto delay = (double)msg->sampleOffset / (double)x->x_samplerate * 1000.0;
    if (delay > 0) {
        // put on queue and schedule on clock (using logical time)
        auto abstime = clock_getsystimeafter(delay);
        // reschedule if we are the next due element
        if (x->x_queue.empty() || abstime < x->x_queue.top().time) {
            clock_set(x->x_queue_clock, abstime);
        }
        x->x_queue.emplace(t_stream_message(*msg, *ep), abstime);
    } else {
        // dispatch immediately
        aoo::ip_address addr((const sockaddr *)ep->address, ep->addrlen);
        x->dispatch_stream_message(*msg, addr, ep->id);
    }

}

static void aoo_receive_handle_event(t_aoo_receive_tilde *x, const AooEvent *event, int32_t)
{
    switch (event->type){
    case kAooEventSourceAdd:
    case kAooEventSourceRemove:
    case kAooEventInviteDecline:
    case kAooEventInviteTimeout:
    case kAooEventUninviteTimeout:
    case kAooEventBufferOverrun:
    case kAooEventBufferUnderrun:
    case kAooEventFormatChange:
    case kAooEventStreamStart:
    case kAooEventStreamStop:
    case kAooEventStreamState:
    case kAooEventStreamLatency:
    case kAooEventStreamTime:
    case kAooEventBlockDrop:
    case kAooEventBlockResend:
    case kAooEventBlockXRun:
    case kAooEventSourcePing:
    {
        // common endpoint header
        auto& ep = event->endpoint.endpoint;
        // crea un riferimento ad un nodo per creare un indirizzo ip
        aoo::ip_address addr((const sockaddr *)ep.address, ep.addrlen);
        const int maxsize = 32;
        // crea un msg
        t_atom msg[maxsize];
        if (!x->x_node->serialize_endpoint(addr, ep.id, 3, msg)) {
            error("bug: aoo_receive_handle_event: serialize_endpoint");
            return;
        }
        // dipendendo dal tipo di evento:
        // event data
        switch (event->type){
        // quando aoo.send~ aggiunge (add) questo receive come AOO sink
        case kAooEventSourceAdd:
        {
            // first add to source list; try to find peer name!
            t_symbol *group = nullptr;
            t_symbol *user = nullptr;
            // cerca una connecssione
            x->x_node->find_peer(addr, group, user);
            // aggiunge la connessione all'array delle sorgenti
            x->x_sources.push_back({ addr, ep.id, group, user });
            // stampa il messaggio "add"
            outlet_anything(x->x_msgout, gensym("add"), 3, msg);
            break;
        }
        // quando aoo.send~ rimuove (remove) questo receive come AOO sink
        case kAooEventSourceRemove:
        {
            // loop nell'array delle sorgenti
            // e cancella una sorgente se presente nel loop
            // first remove from source list
            auto& sources = x->x_sources;
            for (auto it = sources.begin(); it != sources.end(); ++it){
                if ((it->s_address == addr) && (it->s_id == ep.id)){
                    x->x_sources.erase(it);
                    break;
                }
            }
            // informa in Max
            outlet_anything(x->x_msgout, gensym("remove"), 3, msg);
            break;
        }
        case kAooEventInviteDecline:
        {
            outlet_anything(x->x_msgout, gensym("invite_decline"), 3, msg);
            break;
        }
        case kAooEventInviteTimeout:
        {
            outlet_anything(x->x_msgout, gensym("invite_timeout"), 3, msg);
            break;
        }
        case kAooEventUninviteTimeout:
        {
            outlet_anything(x->x_msgout, gensym("uninvite_timeout"), 3, msg);
            break;
        }
        case kAooEventSourcePing:
        {
            auto& e = event->sourcePing;

            double delta1 = aoo_ntpTimeDuration(e.t1, e.t2) * 1000.0;
            double delta2 = aoo_ntpTimeDuration(e.t3, e.t4) * 1000.0;
            double total_rtt = aoo_ntpTimeDuration(e.t1, e.t4) * 1000.0;
            double network_rtt = total_rtt - aoo_ntpTimeDuration(e.t2, e.t3) * 1000;

            atom_setfloat(msg + 3, delta1);
            atom_setfloat(msg + 4, delta2);
            atom_setfloat(msg + 5, network_rtt);
            atom_setfloat(msg + 6, total_rtt);

            outlet_anything(x->x_msgout, gensym("ping"), 7, msg);

            break;
        }
        case kAooEventBufferOverrun:
        {
            outlet_anything(x->x_msgout, gensym("overrun"), 3, msg);
            break;
        }
        case kAooEventBufferUnderrun:
        {
            outlet_anything(x->x_msgout, gensym("underrun"), 3, msg);
            break;
        }
        case kAooEventFormatChange:
        {
            // skip first 3 atoms
            int n = format_to_atoms(*event->formatChange.format, maxsize - 3, msg + 3);
            outlet_anything(x->x_msgout, gensym("format"), n + 3, msg);
            break;
        }
        case kAooEventStreamStart:
        {
            auto& e = event->streamStart;
            if (e.metadata){
                // <ip> <port> <id> <type> <data...>
                auto count = 4 + (e.metadata->size / datatype_element_size(e.metadata->type));
                auto vec = (t_atom *)alloca(count * sizeof(t_atom));
                // copy endpoint
                memcpy(vec, msg, 3 * sizeof(t_atom));
                // copy data
                data_to_atoms(*e.metadata, count - 3, vec + 3);

                outlet_anything(x->x_msgout, gensym("start"), count, vec);
            } else {
                outlet_anything(x->x_msgout, gensym("start"), 3, msg);
            }
            break;
        }
        case kAooEventStreamStop:
        {
            outlet_anything(x->x_msgout, gensym("stop"), 3, msg);
            break;
        }
        case kAooEventStreamState:
        {
            auto state = event->streamState.state;
            auto offset = event->streamState.sampleOffset;
            if (offset > 0) {
                // HACK: schedule as fake stream message
                AooStreamMessage msg;
                msg.type = kAooDataStreamState;
                msg.sampleOffset = offset;
                msg.size = sizeof(state);
                msg.data = (AooByte *)&state;
                aoo_receive_handle_stream_message(x, &msg, &ep);
            } else {
                atom_setfloat(msg + 3, state);
                outlet_anything(x->x_msgout, gensym("state"), 4, msg);
            }
            break;
        }
        case kAooEventStreamLatency:
        {
            atom_setfloat(msg + 3, event->streamLatency.sourceLatency * 1000);
            atom_setfloat(msg + 4, event->streamLatency.sinkLatency * 1000);
            atom_setfloat(msg + 5, event->streamLatency.bufferLatency * 1000);
            outlet_anything(x->x_msgout, gensym("latency"), 6, msg);
            break;
        }
        case kAooEventStreamTime:
        {
            AooNtpTime tt[2];
            tt[0] = event->streamTime.sourceTime;
            tt[1] = event->streamTime.sinkTime;
            auto offset = event->streamTime.sampleOffset;
            if (offset > 0) {
                // HACK: schedule as fake stream message
                AooStreamMessage msg;
                msg.type = kAooDataStreamTime;
                msg.sampleOffset = offset;
                msg.size = sizeof(tt);
                msg.data = (AooByte *)tt;
                aoo_receive_handle_stream_message(x, &msg, &ep);
            } else {
                atom_setfloat(msg + 3, get_elapsed_ms(tt[0]));
                atom_setfloat(msg + 4, get_elapsed_ms(tt[1]));
                outlet_anything(x->x_msgout, gensym("time"), 5, msg);
            }
            break;
        }
        case kAooEventBlockDrop:
        {
            atom_setfloat(msg + 3, event->blockDrop.count);
            outlet_anything(x->x_msgout, gensym("block_dropped"), 4, msg);
            break;
        }
        case kAooEventBlockResend:
        {
            atom_setfloat(msg + 3, event->blockResend.count);
            outlet_anything(x->x_msgout, gensym("block_resent"), 4, msg);
            break;
        }
        case kAooEventBlockXRun:
        {
            atom_setfloat(msg + 3, event->blockXRun.count);
            outlet_anything(x->x_msgout, gensym("block_xrun"), 4, msg);
            break;
        }
        default:
            error("BUG: aoo_receive_handle_event: bad case label!");
            break;
        }

        break; // !
    }
    default:
        object_post((t_object*)x, "%s: unknown event type (%d)",
                object_classname(x), event->type);
        break;
    }
}


// member functions
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
    x_sink->setEventHandler((AooEventHandler)aoo_receive_handle_event,
                             this, kAooEventModePoll);

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


