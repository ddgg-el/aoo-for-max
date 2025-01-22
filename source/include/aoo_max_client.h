#pragma once

#include "ext.h"			// standard Max include
#include "ext_obex.h"		// required for new style Max object

#include "aoo_max_dejitter.h"
#include "aoo_max_node.h"

#include "aoo.h"
#include "common/net_utils.hpp"
#include "common/sync.hpp"

#define AOO_CLIENT_POLL_INTERVAL 2

struct t_peer_message
{
    t_peer_message(float del, AooId grp, AooId usr, const AooData& msg)
        : delay(del), group(grp), user(usr), type(msg.type),
          data(msg.data, msg.data + msg.size) {}
    float delay;
    AooId group;
    AooId user;
    AooDataType type;
    std::vector<AooByte> data;
};

struct t_group
{
    t_symbol *name;
    AooId id;
};

struct t_peer
{
    t_symbol *group_name;
    t_symbol *user_name;
    AooId group_id;
    AooId user_id;
    aoo::ip_address address;
};

////////////////////////// object struct
struct t_aoo_client
{
	t_aoo_client(int argc, t_atom *argv);
	t_dejitter *x_dejitter;
	t_object	ob;			// the object itself (must be first)
    t_node *x_node = nullptr;

    t_clock *x_clock = nullptr;
    t_clock *x_queue_clock = nullptr;
    t_outlet *x_stateout = nullptr;
    t_outlet *x_msgout = nullptr;

    bool x_connected = false;
    bool x_schedule = true;
    t_float x_delay = 0; // extra message delay
    bool x_discard = false;

    t_priority_queue<t_peer_message> x_queue;

     // replies
    using t_reply = std::function<void()>;
    std::vector<t_reply> replies_;
    aoo::sync::mutex reply_mutex_;

	const t_peer * find_peer(const aoo::ip_address& addr) const;

    const t_peer * find_peer(AooId group, AooId user) const;

    const t_peer * find_peer(t_symbol *group, t_symbol *user) const;

    void handle_message(AooId group, AooId user, AooNtpTime time, const AooData& msg);

    void dispatch_message(t_float delay, AooId group, AooId user, const AooData& msg) const;

    // peer list
	std::vector<t_peer> x_peers;
    // group list
    std::vector<t_group> x_groups;
};

void aoo_client_handle_event(t_aoo_client *x, const AooEvent *event, int32_t level);

static void aoo_client_queue_tick(t_aoo_client *x);
static void aoo_client_tick(t_aoo_client *x);
static int peer_to_atoms(const t_peer& peer, int argc, t_atom *argv);