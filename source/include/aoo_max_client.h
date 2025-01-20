#pragma once

#include "ext.h"			// standard Max include
#include "ext_obex.h"		// required for new style Max object

#include "aoo_max_globals.h"
#include "aoo_max_dejitter.h"

#include "aoo.h"
#include "common/net_utils.hpp"

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
	// t_object	ob;			// the object itself (must be first)

	// t_dejitter *x_dejitter;
	const t_peer * find_peer(const aoo::ip_address& addr) const;

    const t_peer * find_peer(AooId group, AooId user) const;

    const t_peer * find_peer(t_symbol *group, t_symbol *user) const;

	std::vector<t_peer> x_peers;
};


