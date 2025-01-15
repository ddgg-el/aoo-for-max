/**
	@file
	aoo_client - a max object shell
	jeremy bernstein - jeremy@bootsquad.com

	@ingroup	examples
*/

#include "ext.h"							// standard Max include, always required
#include "ext_obex.h"						// required for new style Max object

#include "aoo_common.hpp"


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
	t_object					ob;			// the object itself (must be first)

	// t_dejitter *x_dejitter;
	const t_peer * find_peer(const aoo::ip_address& addr) const;

    const t_peer * find_peer(AooId group, AooId user) const;

    const t_peer * find_peer(t_symbol *group, t_symbol *user) const;

	std::vector<t_peer> x_peers;
};
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
bool aoo_client_find_peer(t_aoo_client *x, const aoo::ip_address& addr,
                          t_symbol *& group, t_symbol *& user)
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
bool aoo_client_find_peer(t_aoo_client *x, t_symbol *group, t_symbol *user,
                          aoo::ip_address& addr)
{
    if (auto peer = x->find_peer(group, user)) {
        addr = peer->address;
        return true;
    } else {
        return false;
    }
}


///////////////////////// function prototypes
//// standard set
void *aoo_client_new(t_symbol *s, long argc, t_atom *argv);
void aoo_client_free(t_aoo_client *x);
void aoo_client_assist(t_aoo_client *x, void *b, long m, long a, char *s);

//////////////////////// global class pointer variable
static t_class *aoo_client_class = NULL;


void ext_main(void *r)
{
	t_class *c;

	c = class_new("aoo.client", (method)aoo_client_new, (method)aoo_client_free, (long)sizeof(t_aoo_client),
				  0L /* leave NULL!! */, A_GIMME, 0);

	/* you CAN'T call this from the patcher */
	class_addmethod(c, (method)aoo_client_assist,			"assist",		A_CANT, 0);

	class_register(CLASS_BOX, c); /* CLASS_NOBOX */
	aoo_client_class = c;

	post("I am the aoo_client object");
}

void aoo_client_assist(t_aoo_client *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) { // inlet
		sprintf(s, "I am inlet %ld", a);
	}
	else {	// outlet
		sprintf(s, "I am outlet %ld", a);
	}
}

void aoo_client_free(t_aoo_client *x)
{
	;
}


void *aoo_client_new(t_symbol *s, long argc, t_atom *argv)
{
	t_aoo_client *x = (t_aoo_client *)object_alloc(aoo_client_class);
	long i;

	if (x) {
		object_post((t_object *)x, "a new %s object was instantiated: %p", s->s_name, x);
		object_post((t_object *)x, "it has %ld arguments", argc);

		// new (x) t_aoo_client(argc, argv);

		// for (i = 0; i < argc; i++) {
		// 	if ((argv + i)->a_type == A_LONG) {
		// 		object_post((t_object *)x, "arg %ld: long (%ld)", i, atom_getlong(argv+i));
		// 	} else if ((argv + i)->a_type == A_FLOAT) {
		// 		object_post((t_object *)x, "arg %ld: float (%f)", i, atom_getfloat(argv+i));
		// 	} else if ((argv + i)->a_type == A_SYM) {
		// 		object_post((t_object *)x, "arg %ld: symbol (%s)", i, atom_getsym(argv+i)->s_name);
		// 	} else {
		// 		object_error((t_object *)x, "forbidden argument");
		// 	}
		// }
	}
	return (x);
}
