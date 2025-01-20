/**
	@file
	aoo_client - a max object shell
	jeremy bernstein - jeremy@bootsquad.com

	@ingroup	examples
*/
#include "aoo_max_client.h"

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