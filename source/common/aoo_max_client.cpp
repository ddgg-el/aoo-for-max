#include "aoo_max_common.hpp"

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

    x_node = port > 0 ? t_node::get((t_object*)this, port) : nullptr;

    if (x_node){
        object_post((t_object*)this, "new client on port %d", port);
        // start clock
        clock_delay(x_clock, AOO_CLIENT_POLL_INTERVAL);
    }
}

t_aoo_client::~t_aoo_client()
{
    if (x_node){
        if (x_connected){
            x_node->client()->disconnect(0, 0);
        }
        x_node->release((t_object*)this);
    }

    // ignore pending requests (doesn't leak)
    if (x_dejitter) {
        dejitter_release(x_dejitter);
    }
    clock_free(x_clock);
    clock_free(x_queue_clock);
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
/**
 * @brief find a peer in the list of peers
 * 
 * @param group 
 * @param user 
 * @return const t_peer* 
 */
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

int peer_to_atoms(const t_peer& peer, int argc, t_atom *argv) {
    if (argc >= 5) {
        atom_setsym(argv, peer.group_name);
        // don't send group ID because it might be too large for a float
        atom_setsym(argv + 1, peer.user_name);
        atom_setfloat(argv + 2, peer.user_id);
        address_to_atoms(peer.address, 2, argv + 3);
        return 5;
    } else {
        return 0;
    }
}

void t_aoo_client::handle_message(AooId group, AooId user, AooNtpTime time,
                                  const AooData& data)
{
    aoo::time_tag tt(time);
    if (!tt.is_empty() && !tt.is_immediate()){
        aoo::time_tag now = x_dejitter ? dejitter_osctime(x_dejitter) : get_osctime();
        auto delay = aoo::time_tag::duration(now, tt) * 1000.0;
        if (x_schedule) {
            // NB: only add extra delay to automatically scheduled messages!
            delay += x_delay;
            if (delay >= 0) {
                // put on queue and schedule on clock (using logical time)
                auto abstime = clock_getsystimeafter(delay);
                // reschedule if we are the next due element
                if (x_queue.empty() || abstime < x_queue.top().time) {
                    clock_set(x_queue_clock, abstime);
                }
                x_queue.emplace(t_peer_message(delay, group, user, data), abstime);
            } else if (!x_discard) {
                // output late message (negative delay!)
                dispatch_message(delay, group, user, data);
            }
        } else {
            // output immediately with delay (may be negative!)
            dispatch_message(delay, group, user, data);
        }
    } else {
        // send immediately
        dispatch_message(0, group, user, data);
    }
}

void aoo_client_handle_event(t_aoo_client *x, const AooEvent *event, int32_t level)
{
    switch (event->type){
    case kAooEventPeerMessage:
    {
        auto& e = event->peerMessage;
        x->handle_message(e.groupId, e.userId, e.timeStamp, e.data);
        break;
    }
    case kAooEventDisconnect:
    {
        object_error((t_object*)x, "disconnected from server");

        x->x_peers.clear();
        x->x_groups.clear();
        x->x_connected = false;

        outlet_float(x->x_stateout, 0); // disconnected

        break;
    }
    case kAooEventPeerHandshake:
    case kAooEventPeerTimeout:
    case kAooEventPeerJoin:
    case kAooEventPeerLeave:
    {
        auto& e = event->peer;
        auto group_name = gensym(e.groupName);
        auto user_name = gensym(e.userName);
        auto group_id = e.groupId;
        auto user_id = e.userId;
        aoo::ip_address addr((const sockaddr *)e.address.data, e.address.size);

        t_atom msg[5];

        switch (event->type) {
        case kAooEventPeerHandshake:
        {
            atom_setsym(msg, group_name);
            atom_setsym(msg + 1, user_name);
            atom_setfloat(msg + 2, user_id);
            outlet_anything(x->x_msgout, gensym("peer_handshake"), 3, msg);
            break;
        }
        case kAooEventPeerTimeout:
        {
            atom_setsym(msg, group_name);
            atom_setsym(msg + 1,user_name);
            atom_setfloat(msg + 2, user_id);
            outlet_anything(x->x_msgout, gensym("peer_timeout"), 3, msg);
            break;
        }
        case kAooEventPeerJoin:
        {
            if (x->find_peer(group_id, user_id)) {
                error("BUG: aoo_client: can't add peer %s|%s: already exists",
                    group_name->s_name, user_name->s_name);
                return;
            }

            // add peer
            x->x_peers.emplace_back(t_peer { group_name, user_name, group_id, user_id, addr });
            peer_to_atoms(x->x_peers.back(), 5, msg);

            outlet_anything(x->x_msgout, gensym("peer_join"), 5, msg);

            break;
        }
        case kAooEventPeerLeave:
        {
            for (auto it = x->x_peers.begin(); it != x->x_peers.end(); ++it) {
                if (it->group_id == group_id && it->user_id == user_id) {
                    peer_to_atoms(*it, 5, msg);

                    // remove *before* sending the message
                    x->x_peers.erase(it);

                    outlet_anything(x->x_msgout, gensym("peer_leave"), 5, msg);

                    return;
                }
            }
            error("BUG: aoo_client: can't remove peer %s|%s: does not exist",
                group_name->s_name, user_name->s_name);
            break;
        }
        default:
            break;
        }

        break; // !
    }
    case kAooEventPeerPing:
    {
        auto& e = event->peerPing;
        t_atom msg[9];

        auto peer = x->find_peer(e.group, e.user);
        if (!peer) {
            error("BUG: aoo_client: can't find peer %d|%d for ping event", e.group, e.user);
            return;
        }

        auto delta1 = aoo::time_tag::duration(e.t1, e.t2) * 1000;
        auto delta2 = aoo::time_tag::duration(e.t3, e.t4) * 1000;
        auto total_rtt = aoo::time_tag::duration(e.t1, e.t4) * 1000;
        auto network_rtt = total_rtt - aoo::time_tag::duration(e.t2, e.t3) * 1000;

        peer_to_atoms(*peer, 5, msg);
        atom_setfloat(msg + 5, delta1);
        atom_setfloat(msg + 6, delta2);
        atom_setfloat(msg + 7, network_rtt);
        atom_setfloat(msg + 8, total_rtt);

        outlet_anything(x->x_msgout, gensym("peer_ping"), 9, msg);

        break;
    }
    default:
        object_post((t_object*)x, "%s: unknown event type %d",
                object_classname(x), event->type);
        break;
    }
}
