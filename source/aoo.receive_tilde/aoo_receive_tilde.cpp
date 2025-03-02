/**
	@file
	aoo.receive~: a simple audio object for Max
	original by: Christof Ressi, ported to Max by Francesca Seggioli
	@ingroup network
*/

// #include "aoo.h"
#include "aoo_receive_tilde.h"

// for hardware buffer sizes up to 1024 @ 44.1 kHz
#define DEFAULT_LATENCY 25

// "fake" stream message types
const int kAooDataStreamTime = -3; // AooEventStreamTime
const int kAooDataStreamState = -2; // AooEventStreamState
// NB: kAooDataUnspecified = -1

//********************** static functions **********************
static void aoo_receive_tick(t_aoo_receive *x)
{
    x->x_sink->pollEvents();
}

static void aoo_receive_queue_tick(t_aoo_receive *x)
{
    auto& queue = x->x_queue;
    auto now = gettime();

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

static void aoo_receive_handle_stream_message(t_aoo_receive *x, const AooStreamMessage *msg, const AooEndpoint *ep)
{
    // FIXME: casting
    auto delay = (double)msg->sampleOffset / (double)x->x_samplerate * 1000.0;
    // object_post((t_object*)x, "delay: %d", delay);
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

static void aoo_receive_handle_event(t_aoo_receive *x, const AooEvent *event, int32_t)
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

static void aoo_receive_set(t_aoo_receive *x, int f1, int f2)
{
    int port = f1;
    AooId id = f2;

    if (id == x->x_id && port == x->x_port) {
        return;
    }

    if (id < 0) {
        object_error((t_object*)x, "bad id %d", id);
        return;
    }

    if (port < 0) {
        // NB: 0 is allowed (= don't listen)!
        object_error((t_object*)x, "bad port %d", id);
        return;
    }

    // always release node!
    if (x->x_node) {
        x->x_node->release((t_object*)x, x->x_sink.get());
    }

    if (id != x->x_id) {
        x->x_sink->setId(id);
        x->x_id = id;
    }

    if (port) {
        x->x_node = t_node::get((t_object *)x, port, x->x_sink.get(), id);
    } else {
        x->x_node = nullptr;
    }
    x->x_port = port;
}
// Vedi sopra
static void aoo_receive_port(t_aoo_receive *x, double f)
{
    aoo_receive_set(x, f, x->x_id);
}

//********************** member functions **********************
t_aoo_receive::t_aoo_receive(int argc, t_atom *argv)
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
    //                 object_error(this, "%s: no multi-channel support, ignoring '-m' flag", classname(this));
    //             }
    //         } else {
    //             object_error(this, "%s: ignore unknown flag '%s",
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
            object_error((t_object*)this, "channel count (%d) out of range", noutlets);
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
        object_error((t_object*)this, "bad id % d, setting to 0", id);
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

    // // set event handler per il Sink
    x_sink->setEventHandler((AooEventHandler)aoo_receive_handle_event,
                             this, kAooEventModePoll);

    // imposta la latenza per il sink
    x_sink->setLatency(latency * 0.001);

    // finally we're ready to receive messages
    // imposto la porta e l'id del sink
    aoo_receive_port(this, port);
}

t_aoo_receive::~t_aoo_receive()
{
    if (x_node){
        x_node->release((t_object*)this, x_sink.get());
    }
    clock_free(x_clock);
    clock_free(x_queue_clock);
}

void t_aoo_receive::dispatch_stream_message(const AooStreamMessage& msg, const aoo::ip_address& address, AooId id) 
{
    // 5 extra atoms for endpoint (host, port, ID) + message (channel, type)
    // NB: in case of "fake" stream messages, we just over-allocate.
    auto size = 5 + (msg.size / datatype_element_size(msg.type));
	// un'array dove verrà copiato il messaggio da mandare all'outlet
    auto vec = (t_atom *)alloca(sizeof(t_atom) * size);

    if (!x_node->serialize_endpoint(address, id, 3, vec)) {
        error("bug: dispatch_stream_message: serialize_endpoint");
        return;
    }
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

bool t_aoo_receive::check(const char *name) const
{
    if (x_node){
        return true;
    } else {
        object_error((t_object*)this, "'%s' failed: no socket!", name);
        return false;
    }
}

bool t_aoo_receive::check(int argc, t_atom *argv, int minargs, const char *name) const
{
    if (!check(name)) return false;

    if (argc < minargs){
        object_error((t_object*)this, "too few arguments for '%s' message", name);
        return false;
    }

    return true;
}

bool t_aoo_receive::get_source_arg(int argc, t_atom *argv,
                                   aoo::ip_address& addr, AooId& id, bool check) const
{
    if (!x_node) {
        object_error((t_object*)this, "no socket!");
        return false;
    }
    if (argc < 3){
        object_error((t_object*)this, "too few arguments for source");
        return false;
    }
    // first try peer (group|user)
    if (argv[1].a_type == A_SYM) {
        t_symbol *group = atom_getsym(argv);
        t_symbol *user = atom_getsym(argv + 1);
        id = atom_getfloat(argv + 2);
        // first search source list, in case the client has been disconnected
        for (auto& s : x_sources) {
            if (s.s_group == group && s.s_user == user && s.s_id == id) {
                addr = s.s_address;
                return true;
            }
        }
        if (!check) {
            // not yet in list -> try to get from client
            if (x_node->find_peer(group, user, addr)) {
                return true;
            }
        }
        object_error((t_object*)this, "couldn't find source %s|%s %d", group->s_name, user->s_name, id);
        return false;
    } else {
        // otherwise try host|port
        t_symbol *host = atom_getsym(argv);
        int port = atom_getfloat(argv + 1);
        id = atom_getfloat(argv + 2);
        if (x_node->resolve(host, port, addr)) {
            if (check) {
                // try to find in list
                for (auto& s : x_sources) {
                    if (s.s_address == addr && s.s_id == id) {
                        return true;
                    }
                }
                object_error((t_object*)this, "couldn't find source %s %d %d", host->s_name, port, id);
                return false;
            } else {
                return true;
            }
        } else {
            object_error((t_object*)this, "couldn't resolve source hostname '%s'", host->s_name);
            return false;
        }
    }
}

//********************** object methods **********************
static void aoo_receive_ping(t_aoo_receive *x, double f)
{
    x->x_sink->setPingInterval(f * 0.001);
}
static void aoo_receive_invite(t_aoo_receive *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!x->check(argc, argv, 3, "invite")) return;

    aoo::ip_address addr;
    AooId id = 0;
    if (x->get_source_arg(argc, argv, addr, id, false)) {
        AooEndpoint ep { addr.address(), (AooAddrSize)addr.length(), id };

        if (argc > 3) {
            // with metadata
            AooData metadata;
            if (!atom_to_datatype(argv[3], metadata.type, x)) {
                return;
            }
            argc -= 4; argv += 4;
            if (!argc) {
                object_error((t_object*)x, "metadata must not be empty");
                return;
            }
            auto size = argc * datatype_element_size(metadata.type);
            auto data = (AooByte *)alloca(size);
            atoms_to_data(metadata.type, argc, argv, data, size);
            metadata.size = size;
            metadata.data = data;

            x->x_sink->inviteSource(ep, &metadata);
        } else {
            post("inviting");
            x->x_sink->inviteSource(ep, nullptr);
        }
        // notify send thread
        x->x_node->notify();
    }
}
static void aoo_receive_uninvite(t_aoo_receive *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!x->check("uninvite")) return;

    if (!argc){
        x->x_sink->uninviteAll();
        return;
    }

    if (argc < 3){
        object_error((t_object*)x, "too few arguments for 'uninvite' message");
        return;
    }

    aoo::ip_address addr;
    AooId id = 0;
    if (x->get_source_arg(argc, argv, addr, id, true)){
        AooEndpoint ep { addr.address(), (AooAddrSize)addr.length(), id };
        x->x_sink->uninviteSource(ep);
        // notify send thread
        x->x_node->notify();
    }
}
static void aoo_receive_source_list(t_aoo_receive *x)
{
    if (!x->check("source_list")) return;

     if(x->x_sources.empty()){
        t_atom msg[1];
        t_symbol* text = gensym("empty");
        atom_setsym(msg, text);
        outlet_anything(x->x_msgout, gensym("source_list"), 1, msg);
        object_post((t_object*)x, "no sources registered");
        return;
    }

    for (auto& src : x->x_sources)
    {
        t_atom msg[3];
        if (x->x_node->serialize_endpoint(src.s_address, src.s_id, 3, msg)) {
            outlet_anything(x->x_msgout, gensym("source"), 3, msg);
        } else {
            error("BUG: aoo_receive_source_list: serialize_endpoint");
        }
    }
}
static void aoo_receive_bang(t_aoo_receive *x);
static void aoo_receive_latency(t_aoo_receive *x, double f)
{
        // post("Valore di f: %f", f);
        x->x_sink->setLatency(f * 0.001);
}
static void aoo_receive_fill_ratio(t_aoo_receive *x, t_symbol *s, int argc, t_atom *argv){
    aoo::ip_address addr;
    AooId id = 0;
    if (argc > 0) {
        if (!x->get_source_arg(argc, argv, addr, id, true)) {
            return;
        }
    } else {
        // just get the first source (if not empty)
        if (!x->x_sources.empty()) {
            auto& src = x->x_sources.front();
            addr = src.s_address;
            id = src.s_id;
        } else {
            object_error((t_object*)x, "%s: no sources");
            
        }
    }

    double ratio = 0;
    AooEndpoint ep { addr.address(), (AooAddrSize)addr.length(), id };
    x->x_sink->getBufferFillRatio(ep, ratio);

    t_atom msg[4];
    if (x->x_node->serialize_endpoint(addr, id, 3, msg)){
        atom_setfloat(msg + 3, ratio);
        outlet_anything(x->x_msgout, gensym("fill_ratio"), 4, msg);
    }
}
static void aoo_receive_packetsize(t_aoo_receive *x, double f)
{
    // post("Valore di f: %f", f);
    x->x_sink->setPacketSize(f);
}
static void aoo_receive_buffersize(t_aoo_receive *x, double f)
{
    // post("Valore di f: %f", f);
    x->x_sink->setBufferSize(f * 0.001);
}
static void aoo_receive_reset(t_aoo_receive *x, t_symbol *s, int argc, t_atom *argv)
{
    if (argc){
        // reset specific source
        aoo::ip_address addr;
        AooId id = 0;
        if (x->get_source_arg(argc, argv, addr, id, true)) {
            AooEndpoint ep { addr.address(), (AooAddrSize)addr.length(), id };
            x->x_sink->resetSource(ep);
        }
    } else {
        // reset all sources
        x->x_sink->reset();
    }
}
static void aoo_receive_resend(t_aoo_receive *x, double f)
{
    x->x_sink->setResendData(f != 0);
}
static void aoo_receive_resend_limit(t_aoo_receive *x, double f)
{
    x->x_sink->setResendLimit(f);
}
static void aoo_receive_resend_interval(t_aoo_receive *x, double f)
{
    x->x_sink->setResendInterval(f * 0.001);
}
static void aoo_receive_resample_method(t_aoo_receive *x, t_symbol *s)
{
    AooResampleMethod method;
    std::string_view name = s->s_name;
    if (name == "hold") {
        method = kAooResampleHold;
    } else if (name == "linear") {
        method = kAooResampleLinear;
    } else if (name == "cubic") {
        method = kAooResampleCubic;
    } else {
        object_error((t_object*)x, "%s: bad resample method '%s'",
                  name.data());
        return;
    }
    if (x->x_sink->setResampleMethod(method) != kAooOk) {
        object_error((t_object*)x, "%s: resample method '%s' not supported",
                  name.data());
    }
}
static void aoo_receive_dynamic_resampling(t_aoo_receive *x, double f)
{
    x->x_sink->setDynamicResampling(f);
}
static void aoo_receive_dll_bandwidth(t_aoo_receive *x, double f)
{
    x->x_sink->setDllBandwidth(f);
}
// <ip> <port> <id> <codec> <option> [<value>]
static void aoo_receive_codec_set(t_aoo_receive *x, t_symbol *s, int argc, t_atom *argv){
    if (!x->check(argc, argv, 6, "codec_set")) return;
#if 0
    aoo::ip_address addr;
    AooId id = 0;
    if (!x->get_source_arg(argc, argv, addr, id, true)) {
        return;
    }
    AooEndpoint ep { addr.address(), (AooAddrSize)addr.length(), id };
#endif
    auto codec = atom_getsym(argv + 3);
    auto opt = atom_getsym(argv + 4);
    // no codec options yet
    object_error((t_object*)x,"%s: unknown parameter '%s' for codec '%s'",
             opt->s_name, codec->s_name);
}
// <ip> <port> <id> <codec> <option>
// replies with <ip> <port> <id> <codec> <option> <value>
static void aoo_receive_codec_get(t_aoo_receive *x, t_symbol *s, int argc, t_atom *argv) {
    if (!x->check(argc, argv, 5, "codec_get")) return;
#if 0
    aoo::ip_address addr;
    AooId id = 0;
    if (!x->get_source_arg(argc, argv, addr, id, true)) {
        return;
    }
    AooEndpoint ep { addr.address(), (AooAddrSize)addr.length(), id };
#endif
    auto codec = atom_getsym(argv + 3);
    auto opt = atom_getsym(argv + 4);
#if 0
    t_atom msg[6];
    std::copy(argv, argv + 5, msg);
#endif
    // no codec options yet
    object_error((t_object*)x, "%s: unknown parameter '%s' for codec '%s'",
             opt->s_name, codec->s_name);
    return;
}
static void aoo_receive_id(t_aoo_receive *x, double f)
{
    aoo_receive_set(x, x->x_port, f);
}
// there is no method for 'real_samplerate' in pd exthernal?
static void aoo_receive_real_samplerate(t_aoo_receive *x)
{
    AooSampleRate sr;
    x->x_sink->getRealSampleRate(sr);
    t_atom msg;
    atom_setfloat(&msg, sr);
    outlet_anything(x->x_msgout, gensym("real_samplerate"), 1, &msg);
}
//***********************************************************************************************

extern "C" void ext_main(void *r)
{
	// object initialization, note the use of dsp_free for the freemethod, which is required
	// unless you need to free allocated memory, in which case you should call dsp_free from
	// your custom free function.

	t_class *c = class_new("aoo.receive~", (method)aoo_receive_new, (method)aoo_receive_free, (long)sizeof(t_aoo_receive), 0L, A_GIMME, 0);

	class_addmethod(c, (method)aoo_receive_dsp64,	"dsp64",	A_CANT, 0);
	class_addmethod(c, (method)aoo_receive_assist,	"assist",	A_CANT, 0);

    class_addmethod(c, (method)aoo_receive_ping,"ping", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_receive_invite, "invite", A_GIMME, 0);
    class_addmethod(c, (method)aoo_receive_uninvite,"uninvite", A_GIMME, 0);
    class_addmethod(c, (method)aoo_receive_source_list, "source_list", 0);
    class_addmethod(c, (method)aoo_receive_latency,"latency", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_receive_port,"port", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_receive_id,"id", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_receive_fill_ratio,"fill_ratio", A_GIMME, 0);
    class_addmethod(c, (method)aoo_receive_packetsize,"packetsize", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_receive_buffersize,"buffersize", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_receive_bang, "bang", 0);
    class_addmethod(c, (method)aoo_receive_reset,"reset", A_GIMME, 0);
    class_addmethod(c, (method)aoo_receive_resend,"resend", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_receive_resend_limit,"resend_limit", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_receive_resend_interval,"resend_interval", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_receive_resample_method,"resample_method", A_SYM, 0);
    class_addmethod(c, (method)aoo_receive_dynamic_resampling,"dynamic_resampling", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_receive_dll_bandwidth,"dll_bandwidth", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_receive_codec_set,"codec_set", A_GIMME, 0);
    class_addmethod(c, (method)aoo_receive_codec_get,"codec_get", A_GIMME, 0);
    class_addmethod(c, (method)aoo_receive_real_samplerate, "real_samplerate", 0);

	class_dspinit(c);
	class_register(CLASS_BOX, c);
	aoo_receive_class = c;
///////////////////////////////////////////
    AooError err = aoo_initialize(NULL);
    // TODO: handle error
    // if(err != kAooErrorNone){
    //     error("aoo not initialized");
    //     return;
    // } else {
    //     post("aoo initialized!");
    // }
    

    if (auto [ok, msg] = aoo::check_ntp_server(); ok){
        // post("NTP receive server");
    } else {
        error("%s", msg.c_str());
    }

    g_start_time = aoo::time_tag::now();

#ifdef PD_HAVE_MULTICHANNEL
    // runtime check for multichannel support:
#ifdef _WIN32
    // get a handle to the module containing the Pd API functions.
    // NB: GetModuleHandle("pd.dll") does not cover all cases.
    HMODULE module;
    if (GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&pd_typedmess, &module)) {
        g_signal_setmultiout = (t_signal_setmultiout)(void *)GetProcAddress(
            module, "signal_setmultiout");
    }
#else
    // search recursively, starting from the main program
    g_signal_setmultiout = (t_signal_setmultiout)dlsym(
        dlopen(nullptr, RTLD_NOW), "signal_setmultiout");
#endif
#endif // PD_HAVE_MULTICHANNEL
///////////////////////////////////////////

    aoo_node_setup();

}


void *aoo_receive_new(t_symbol *s, long argc, t_atom *argv)
{
	t_aoo_receive *x = (t_aoo_receive *)object_alloc(aoo_receive_class);

	if (x) {
		dsp_setup((t_pxobject *)x, 1);	// MSP inlets: arg is # of inlets and is REQUIRED!
		// use 0 if you don't need inlets
		new (x) t_aoo_receive(argc, argv);
	}
	return (x);
}

void aoo_receive_free(t_aoo_receive *x)
{
    x->~t_aoo_receive();
}

static void aoo_receive_bang(t_aoo_receive *x)
{
    
    ;
}

void aoo_receive_assist(t_aoo_receive *x, void *b, long m, long a, char *s)
{
    int32_t n_chans = x->x_nchannels;
	if (m == ASSIST_INLET) { //inlet
		sprintf(s, "(message) Message");
	}
	else {	// outlet
        if(a == n_chans){
            sprintf(s, "(message) Event output");
        } else {
            sprintf(s, "(signal) Stream Ch %ld", a+1);
        }
	}
}

// registers a function for the signal chain in Max
void aoo_receive_dsp64(t_aoo_receive *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    int32_t nchannels = x->x_nchannels;
    
    if (maxvectorsize != x->x_blocksize || samplerate != x->x_samplerate) {
        object_post((t_object*)x, "blocksize %ld -> %ld, samplerate %f -> %f", x->x_blocksize, maxvectorsize, x->x_samplerate, samplerate);
        x->x_sink->setup(nchannels, samplerate, maxvectorsize, kAooFixedBlockSize);
        x->x_blocksize = maxvectorsize;
        x->x_samplerate = samplerate;
    }
    

	// instead of calling dsp_add(), we send the "dsp_add64" message to the object representing the dsp chain
	// the arguments passed are:
	// 1: the dsp64 object passed-in by the calling function
	// 2: the symbol of the "dsp_add64" message we are sending
	// 3: a pointer to your object
	// 4: a pointer to your 64-bit perform method
	// 5: flags to alter how the signal chain handles your object -- just pass 0
	// 6: a generic pointer that you can use to pass any additional data to your perform method

	object_method(dsp64, gensym("dsp_add64"), x, aoo_receive_perform64, 0, NULL);
}

// this is the 64-bit perform method audio vectors
void aoo_receive_perform64(t_aoo_receive *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	int n = sampleframes;

    if(x->x_node) {
        AooNtpTime time = get_osctime();
        auto err = x->x_sink->process((AooSample**)outs, (int32_t)sampleframes, time, (AooStreamMessageHandler)aoo_receive_handle_stream_message, x); 
        if(err != kAooErrorIdle){
            x->x_node->notify();
        }

        if(x->x_sink->eventsAvailable()){
            clock_delay(x->x_clock, 0);
        }
    }
    else {
        for (int i = 0; i < numouts; ++i)
        {
            for (int j = 0; j < sampleframes; ++j)
            {
                outs[i][j] = 0.5;
            }
        }
    }
}


