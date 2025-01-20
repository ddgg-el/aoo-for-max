#include "aoo_max_client.h"

static void aoo_client_queue_tick(t_aoo_client *x)
{
    auto& queue = x->x_queue;
    auto now = gettime();

    while (!queue.empty()){
        if (queue.top().time <= now) {
            auto& msg = queue.top().data;
            AooData data { msg.type, msg.data.data(), msg.data.size() };
            x->dispatch_message(msg.delay, msg.group, msg.user, data);
            queue.pop();
        } else {
            break;
        }
    }
    // reschedule
    if (!queue.empty()){
        // make sure update_jitter_offset() is called once per DSP tick!
        clock_set(x->x_queue_clock, queue.top().time);
    }
}

static void aoo_client_tick(t_aoo_client *x)
{
    x->x_node->client()->pollEvents();

    x->x_node->notify();

    // handle server replies
    aoo::sync::unique_lock<aoo::sync::mutex> lock(x->reply_mutex_,
                                                  aoo::sync::try_to_lock);
    if (lock.owns_lock()){
        for (auto& reply : x->replies_){
            reply();
        }
        x->replies_.clear();
    } else {
        debug_printf("aoo_client_tick: would block");
    }

    clock_delay(x->x_clock, AOO_CLIENT_POLL_INTERVAL);
}

t_aoo_client::t_aoo_client(int argc, t_atom *argv)
{
    x_dejitter = dejitter_get();
    x_clock = clock_new(this, (method)aoo_client_tick);
    x_queue_clock = clock_new(this, (method)aoo_client_queue_tick);
    x_stateout = outlet_new(&ob, 0);
    x_msgout = outlet_new(&ob, 0);

    int port = argc ? atom_getfloat(argv) : 0;

    x_node = port > 0 ? t_node::get((t_class *)this, port) : nullptr;

    if (x_node){
        object_post((t_object*)this, "%s: new client on port %d",
                object_classname(this), port);
        // start clock
        clock_delay(x_clock, AOO_CLIENT_POLL_INTERVAL);
    }
}

// handle incoming peer message
void t_aoo_client::dispatch_message(t_float delay, AooId group, AooId user,
                                    const AooData& msg) const
{
    auto peer = find_peer(group, user);
    if (!peer) {
        // should never happen because the peer should have been added
        error("BUG: dispatch_message");
        return;
    }

    // <offset> <group> <user> <type> <data...>
    auto size = 4 + (msg.size / datatype_element_size(msg.type));
    // TODO: verifica anche sotto
    //prevent possible stack overflow with huge messages 1000 in PD 500 in MAX
    auto vec = (t_atom *)((size > ASSIST_MAX_STRING_LEN) ?
        getbytes(size * sizeof(t_atom)) : alloca(size * sizeof(t_atom)));

    atom_setfloat(&vec[0], delay);
    atom_setsym(&vec[1], peer->group_name);
    atom_setsym(&vec[2], peer->user_name);
    data_to_atoms(msg, size - 3, vec + 3);

    outlet_anything(x_msgout, gensym("msg"), size, vec);
    // 1000 caratteri in PD
    if (size > ASSIST_MAX_STRING_LEN) {
        freebytes(vec, size * sizeof(t_atom));
    }
}

const t_peer * t_aoo_client::find_peer(const aoo::ip_address& addr) const {
    for (auto& peer : x_peers) {
        if (peer.address == addr) {
            return &peer;
        }
    }
    return nullptr;
}

const t_peer * t_aoo_client::find_peer(AooId group, AooId user) const {
    for (auto& peer : x_peers) {
        if (peer.group_id == group && peer.user_id == user) {
            return &peer;
        }
    }
    return nullptr;
}

const t_peer * t_aoo_client::find_peer(t_symbol *group, t_symbol *user) const {
    for (auto& peer : x_peers) {
        if (peer.group_name == group && peer.user_name == user) {
            return &peer;
        }
    }
    return nullptr;
}

// for t_node
bool aoo_client_find_peer(t_aoo_client *x, const aoo::ip_address& addr,t_symbol *& group, t_symbol *& user)
{
    if (auto peer = x->find_peer(addr)) {
        group = peer->group_name;
        user = peer->user_name;
        return true;
    } else {
        return false;
    }
}
// for t_node
bool aoo_client_find_peer(t_aoo_client *x, t_symbol *group, t_symbol *user, aoo::ip_address& addr)
{
    if (auto peer = x->find_peer(group, user)) {
        addr = peer->address;
        return true;
    } else {
        return false;
    }
}