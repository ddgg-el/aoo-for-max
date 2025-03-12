/**
    @file
    aoo_client - a max object shell
    jeremy bernstein - jeremy@bootsquad.com

    @ingroup	examples
*/
#include "aoo_max_common.hpp"

/*
t_aoo_client::t_aoo_client(int argc, t_atom *argv)
{
    x_dejitter = dejitter_get();
    x_clock = clock_new(this, (method)aoo_client_tick);
    x_queue_clock = clock_new(this, (method)aoo_client_queue_tick);
    x_stateout = outlet_new(&x_obj, 0);
    x_msgout = outlet_new(&x_obj, 0);

    int port = argc ? atom_getfloat(argv) : 0;

    x_node = port > 0 ? t_node::get((t_pd *)this, port) : nullptr;

    if (x_node){
        post(this, PD_DEBUG, "%s: new client on port %d",
                classname(this), port);
        // start clock
        clock_delay(x_clock, AOO_CLIENT_POLL_INTERVAL);
    }
}
*/
int peer_to_atoms(const t_peer &peer, int argc, t_atom *argv)
{
    if (argc >= 5)
    {
        atom_setsym(argv, peer.group_name);
        // don't send group ID because it might be too large for a float
        atom_setsym(argv + 1, peer.user_name);
        atom_setfloat(argv + 2, peer.user_id);
        address_to_atoms(peer.address, 2, argv + 3);
        return 5;
    }
    else
    {
        return 0;
    }
}

static void aoo_client_peer_list(t_aoo_client *x)
{
    object_post((t_object *)x, "peer_list");
    if(x->x_peers.empty()){
        t_atom msg[1];
        t_symbol* text = gensym("empty");
        atom_setsym(msg, text);
        outlet_anything(x->x_msgout, gensym("peer_list"), 1, msg);
        return;
    }

    for (auto &peer : x->x_peers)
    {
        t_atom msg[5];
        peer_to_atoms(peer, 5, msg);
        outlet_anything(x->x_msgout, gensym("peer_list"), 5, msg);
    }
}
static void aoo_client_broadcast(t_aoo_client *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!x->check("broadcast"))
        return;

    x->send_message(argc, argv, kAooIdInvalid, kAooIdInvalid);
}
static void aoo_client_send_group(t_aoo_client *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!x->check("send_group"))
        return;
    // only attempt to send if there are peers!
    if (!x->x_peers.empty())
    {
        if (argc > 1 && argv->a_type == A_SYM)
        {
            auto name = argv->a_w.w_sym;
            if (auto group = x->find_group(name))
            {
                x->send_message(argc - 1, argv + 1, group->id, kAooIdInvalid);
            }
            else
            {
                object_warn((t_object *)x, "could not find group '%s'", name->s_name);
            }
        }
        else
        {
            object_error((t_object *)x, "bad arguments to 'send_group': expecting <group> ...");
        }
    }
}
static void aoo_client_send_peer(t_aoo_client *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!x->check("send_peer"))
        return;

    if (argc > 2 && argv[0].a_type == A_SYM && argv[1].a_type == A_SYM)
    {
        auto group = argv[0].a_w.w_sym;
        auto user = argv[1].a_w.w_sym;
        if (auto peer = x->find_peer(group, user))
        {
            x->send_message(argc - 2, argv + 2, peer->group_id, peer->user_id);
        }
        else
        {
            object_error((t_object *)x, "could not find peer %s|%s",
                         group->s_name, user->s_name);
        }
    }
    else
    {
        object_error((t_object *)x, "bad arguments to 'send_peer': expecting <group> <user> ...");
    }
}
static void aoo_client_send(t_aoo_client *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!x->check("send"))
        return;

    if (x->x_target)
    {
        x->send_message(argc, argv, x->x_target_group, x->x_target_user);
    }
}
// TODO: check implementation
static void aoo_client_offset(t_aoo_client *x, double f)
{
    x->x_offset = f;
}
// TODO: check implementation
static void aoo_client_delay(t_aoo_client *x, double f)
{
    x->x_delay = f;
}
// TODO: check implementation
static void aoo_client_schedule(t_aoo_client *x, double f)
{
    x->x_schedule = (f != 0);
}
// TODO: check implementation
static void aoo_client_discard_late(t_aoo_client *x, double f)
{
    x->x_discard = (f != 0);
}
// TODO: check implementation
static void aoo_client_reliable(t_aoo_client *x, double f)
{
    x->x_reliable = (f != 0);
}
static void aoo_client_dejitter(t_aoo_client *x, double f)
{
    int dejitter = f != 0;
    if (dejitter && !x->x_dejitter)
    {
        x->x_dejitter = dejitter_get();
    }
    else if (!dejitter && x->x_dejitter)
    {
        dejitter_release(x->x_dejitter);
        x->x_dejitter = nullptr;
    }
}
static void aoo_client_packetsize(t_aoo_client *x, double f)
{
    if (!x->check("packetsize"))
        return;

    x->x_node->client()->setPacketSize(f);
}
static void aoo_client_binary(t_aoo_client *x, double f)
{
    if (!x->check("binary"))
        return;

    x->x_node->client()->setBinaryFormat(f != 0);
}
static void aoo_client_port(t_aoo_client *x, double f)
{
    int port = f;

    // 0 is allowed -> don't listen
    if (port < 0)
    {
        object_error((t_object *)x, "bad port %d", port);
        return;
    }

    if (x->x_node)
    {
        x->x_node->release((t_object *)x);
    }

    if (port)
    {
        x->x_node = t_node::get((t_object *)x, port);
    }
    else
    {
        x->x_node = 0;
    }
}
// TODO: reference
static void aoo_client_sim_packet_loss(t_aoo_client *x, double f)
{
    if (!x->check("sim_packet_loss"))
        return;

    float val = f;
    auto e = x->x_node->client()->control(kAooCtlSetSimulatePacketLoss, 0, AOO_ARG(val));
    if (e != kAooOk)
    {
        object_error((t_object *)x, "'sim_packet_loss' message failed (%s)",
                     aoo_strerror(e));
    }
}
// TODO: reference
static void aoo_client_sim_packet_reorder(t_aoo_client *x, double f)
{
    if (!x->check("sim_packet_reorder"))
        return;

    AooSeconds val = f * 0.001;
    auto e = x->x_node->client()->control(kAooCtlSetSimulatePacketReorder, 0, AOO_ARG(val));
    if (e != kAooOk)
    {
        object_error((t_object *)x, "'sim_packet_reorder' message failed (%s)",
                     aoo_strerror(e));
    }
}
// TODO: reference
static void aoo_client_sim_packet_jitter(t_aoo_client *x, double f)
{
    if (!x->check("sim_packet_jitter"))
        return;

    AooBool val = f;
    auto e = x->x_node->client()->control(kAooCtlSetSimulatePacketJitter, 0, AOO_ARG(val));
    if (e != kAooOk)
    {
        object_error((t_object *)x, "'sim_packet_jitter' message failed (%s)",
                     aoo_strerror(e));
    }
}
static void aoo_client_target(t_aoo_client *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!x->check("target"))
        return;

    if (argc > 1)
    {
        // <group> <user>
        t_symbol *groupname = atom_getsym(argv);
        t_symbol *username = atom_getsym(argv + 1);
        if (auto peer = x->find_peer(groupname, username))
        {
            x->x_target_group = peer->group_id;
            x->x_target_user = peer->user_id;
            x->x_target = true;
        }
        else
        {
            object_warn((t_object *)x, "could not find peer '%s|%s'",
                         groupname->s_name, username->s_name);
            x->x_target = false;
        }
    }
    else if (argc == 1)
    {
        // <group>
        auto name = atom_getsym(argv);
        if (auto group = x->find_group(name))
        {
            x->x_target_group = group->id;
            x->x_target_user = kAooIdInvalid; // broadcast to all users
            x->x_target = true;
        }
        else
        {
            object_warn((t_object *)x, "could not find group '%s'", name->s_name);
            x->x_target = false;
        }
    }
    else
    {
        x->x_target_group = kAooIdInvalid; // broadcast to all groups
        x->x_target_user = kAooIdInvalid;
        x->x_target = true;
    }
}

static void AOO_CALL connect_cb(t_aoo_client *x, const AooRequest *request,
                                AooError result, const AooResponse *response)
{
    if (result == kAooErrorNone)
    {
        x->push_reply([x](){
            // remove all peers and groups (to be sure)
            x->x_peers.clear();
            x->x_groups.clear();
            x->x_connected = true;

            outlet_float(x->x_stateout, 1); // connected
        });
    }
    else
    {
        auto &e = response->error;
        std::string errmsg = *e.errorMessage ? e.errorMessage : aoo_strerror(result);

        x->push_reply([x, errmsg = std::move(errmsg)](){
            object_error((t_object*)x, "can't connect to server: %s", errmsg.c_str());

            if (!x->x_connected){
                outlet_float(x->x_stateout, 0);
            } 
        });
    }
}
static void aoo_client_connect(t_aoo_client *x, t_symbol *s, int argc, t_atom *argv)
{
    if (x->x_connected)
    {
        object_warn((t_object *)x, "already connected");
        return;
    }
    if (argc < 2)
    {
        object_error((t_object *)x, "too few arguments for '%s' method", s->s_name);
        return;
    }
    if (x->x_node)
    {
        AooClientConnect args;
        args.hostName = atom_getsym(argv)->s_name;
        args.port = atom_getfloat(argv + 1);
        args.password = argc > 2 ? atom_getsym(argv + 2)->s_name : nullptr;

        x->x_node->client()->connect(args, (AooResponseHandler)connect_cb, x);
    }
}
// AooClient::disconnect() can't fail if we're connected, so we can
// essentially disconnect "synchronously". A subsequent call to
// AooClient::connect() will be scheduled after the disconnect request,
// so users can simply do [disconnect, connect ...(.
static void aoo_client_disconnect(t_aoo_client *x)
{
    if (!x->x_connected)
    {
        return; // don't complain
    }
    x->x_peers.clear();
    x->x_groups.clear();
    x->x_connected = false;
    if (x->x_node)
    {
        x->x_node->client()->disconnect(nullptr, nullptr);
    }
    outlet_float(x->x_stateout, 0);
}
static void AOO_CALL group_join_cb(t_aoo_client *x, const AooRequest *request,
                                   AooError result, const AooResponse *response)
{
    if (result == kAooErrorNone)
    {
        x->push_reply([x, group_name = std::string(request->groupJoin.groupName),
                       group_id = response->groupJoin.groupId,
                       user_id = response->groupJoin.userId]()
                      {
    // add group
    auto name = gensym(group_name.c_str());
    if (!x->find_group(group_id)) {
        x->x_groups.push_back({ name, group_id });
    } else {
        error("group_join_cb");
    }

    t_atom msg[3];
    atom_setsym(msg, name);
    atom_setfloat(msg + 1, 1); // 1: success
    atom_setfloat(msg + 2, user_id);
    outlet_anything(x->x_msgout, gensym("group_join"), 3, msg); });
    }
    else
    {
        auto &e = response->error;
        std::string group = std::string(request->groupJoin.groupName);
        std::string errmsg = *e.errorMessage ? e.errorMessage : aoo_strerror(result);

        x->push_reply([x, group = std::move(group), errmsg = std::move(errmsg)](){
            object_warn((t_object*)x, "can't join group '%s': %s",
            group.c_str(), errmsg.c_str());

            t_atom msg[2];
            atom_setsym(msg, gensym(group.c_str()));
            atom_setfloat(msg + 1, 0); // 0: fail
            outlet_anything(x->x_msgout, gensym("group_join"), 2, msg); 
        });
    }
}
static void aoo_client_group_join(t_aoo_client *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!x->x_connected)
    {
        object_error((t_object *)x, "not connected");
        return;
    }
    if (x->x_node)
    {
        if (argc < 3)
        {
            object_error((t_object *)x, "too few arguments for '%s' method", s->s_name);
            return;
        }
        AooClientJoinGroup args;
        args.groupName = atom_getsym(argv)->s_name;
        args.groupPassword = atom_getsym(argv + 1)->s_name;
        args.userName = atom_getsym(argv + 2)->s_name;
        args.userPassword = argc > 3 ? atom_getsym(argv + 3)->s_name : nullptr;

        x->x_node->client()->joinGroup(args, (AooResponseHandler)group_join_cb, x);
    }
}
static void AOO_CALL group_leave_cb(t_aoo_client *x, const AooRequest *request,
                                    AooError result, const AooResponse *response)
{
    if (result == kAooErrorNone){
        x->push_reply([x, id = request->groupLeave.group]() {
            // remove group
            t_symbol *name = nullptr;
            for (auto it = x->x_groups.begin(); it != x->x_groups.end(); ++it) {
                if (it->id == id) {
                    name = it->name;
                    x->x_groups.erase(it);
                    break;
                }
            }
            if (!name) {
                error("group_leave_cb");
                return;
            }
            // we have to remove the peers manually!
            for (auto it = x->x_peers.begin(); it != x->x_peers.end(); ) {
            // remove all peers matching group
                if (it->group_id == id) {
                    it = x->x_peers.erase(it);
                } else {
                    ++it;
                }
            }

            t_atom msg[2];
            atom_setsym(msg, name);
            atom_setfloat(msg + 1, 1); // 1: success
            outlet_anything(x->x_msgout, gensym("group_leave"), 2, msg); 
        });
    }
    else {
        auto &e = response->error;
        auto id = request->groupLeave.group;
        std::string errmsg = *e.errorMessage ? e.errorMessage : aoo_strerror(result);

        x->push_reply([x, id = id, errmsg = std::move(errmsg)]() {
            auto group = x->find_group(id);
            if (!group) {
                error("group_leave_cb");
                return;
            }
            object_warn((t_object*)x, "can't leave group '%s': %s",
            group->name->s_name, errmsg.c_str());

            t_atom msg[2];
            atom_setsym(msg, group->name);
            atom_setfloat(msg + 1, 0); // 0: fail
            outlet_anything(x->x_msgout, gensym("group_leave"), 2, msg); 
        });
    }
}
static void aoo_client_group_leave(t_aoo_client *x, t_symbol *group)
{
    if (!x->x_connected)
    {
        object_error((t_object *)x, "not connected");
        return;
    }
    if (x->x_node)
    {
        auto g = x->find_group(group);
        if (g)
        {
            x->x_node->client()->leaveGroup(g->id, (AooResponseHandler)group_leave_cb, x);
        }
        else
        {
            object_warn((t_object *)x, "can't leave group %s: not a group member",
                         group->s_name);
        }
    }
}

void ext_main(void *r)
{
    t_class *c;

    c = class_new("aoo.client", (method)aoo_client_new, (method)aoo_client_free, (long)sizeof(t_aoo_client), 0L, A_GIMME, 0);

    class_addmethod(c, (method)aoo_client_assist, "assist", A_CANT, 0);

    class_addmethod(c, (method)aoo_client_connect, "connect", A_GIMME, 0);
    class_addmethod(c, (method)aoo_client_disconnect, "disconnect", 0);
    class_addmethod(c, (method)aoo_client_group_join, "group_join", A_GIMME, 0);
    class_addmethod(c, (method)aoo_client_group_leave, "group_leave", A_SYM, 0);
    class_addmethod(c, (method)aoo_client_peer_list, "peer_list", 0);
    class_addmethod(c, (method)aoo_client_broadcast, "broadcast", A_GIMME, 0);
    class_addmethod(c, (method)aoo_client_send_peer, "send_peer", A_GIMME, 0);
    class_addmethod(c, (method)aoo_client_send_group, "send_group", A_GIMME, 0);
    class_addmethod(c, (method)aoo_client_target, "target", A_GIMME, 0);
    class_addmethod(c, (method)aoo_client_send, "send", A_GIMME, 0);
    class_addmethod(c, (method)aoo_client_offset, "offset", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_client_delay, "delay", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_client_schedule, "schedule", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_client_discard_late, "discard_late", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_client_reliable, "reliable", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_client_dejitter, "dejitter", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_client_port, "port", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_client_packetsize, "packetsize", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_client_binary, "binary", A_FLOAT, 0);
    // debug/simulate
    class_addmethod(c, (method)aoo_client_sim_packet_reorder, "sim_packet_reorder", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_client_sim_packet_loss, "sim_packet_loss", A_FLOAT, 0);
    class_addmethod(c, (method)aoo_client_sim_packet_jitter, "sim_packet_jitter", A_FLOAT, 0);

    class_register(CLASS_BOX, c);
    aoo_client_class = c;

    ///////////////////////////////////////////
    AooError err = aoo_initialize(NULL);
    // TODO: handle error
    // if(err != kAooErrorNone){
    //     error("aoo not initialized");
    //     return;
    // } else {
    //     post("aoo initialized!");
    // }

    if (auto [ok, msg] = aoo::check_ntp_server(); ok)
    {
        // post("NTP receive server");
    }
    else
    {
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
            (LPCSTR)&pd_typedmess, &module))
    {
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
    aoo_dejitter_setup();
}

void aoo_client_assist(t_aoo_client *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET)
    { // inlet
        snprintf_zero(s, 256, "(message) Messages");
    }
    else
    { // outlet
        if (a == 0)
        {
            snprintf_zero(s, 256, "(message) Events");
        }
        else
        {
            snprintf_zero(s, 256, "(int) Connection status 0/1");
        }
    }
}

void aoo_client_free(t_aoo_client *x)
{
    x->~t_aoo_client();
}

void *aoo_client_new(t_symbol *s, long argc, t_atom *argv)
{
    t_aoo_client *x = (t_aoo_client *)object_alloc(aoo_client_class);
    new (x) t_aoo_client(argc, argv);
    /*
    if (x) {
        object_post((t_object *)x, "a new %s object was instantiated: %p", s->s_name, x);
        object_post((t_object *)x, "it has %ld arguments", argc);

        new (x) t_aoo_client(argc, argv);

        for (i = 0; i < argc; i++) {
            if ((argv + i)->a_type == A_LONG) {
                object_post((t_object *)x, "arg %ld: long (%ld)", i, atom_getlong(argv+i));
            } else if ((argv + i)->a_type == A_FLOAT) {
                object_post((t_object *)x, "arg %ld: float (%f)", i, atom_getfloat(argv+i));
            } else if ((argv + i)->a_type == A_SYM) {
                object_post((t_object *)x, "arg %ld: symbol (%s)", i, atom_getsym(argv+i)->s_name);
            } else {
                object_error((t_object *)x, "forbidden argument");
            }
        }
    }
    */
    return (x);
}
