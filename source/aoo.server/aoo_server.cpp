/**
	@file
	aoo_server - a max object shell
	@ingroup	network
*/
#include "ext_critical.h"
#include "aoo_server.h"



void t_aoo_server::close() 
{
    if (x_port > 0) {
		unsigned int ret;
		unsigned int u_ret;

        x_server->stop();
        if (x_thread) {
            systhread_join(x_thread, &ret);
        }
        if (x_udp_thread) {
            systhread_join(x_udp_thread, &u_ret);
        }
    }
    clock_unset(x_clock);
    x_port = 0;
}

void t_aoo_server::run(t_aoo_server *x) {
    auto err = x->x_server->run(kAooInfinite);
    if (err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket::strerror(aoo::socket::get_last_error());
        } else {
            msg = aoo_strerror(err);
        }
        critical_enter(0);
        object_error((t_object*)x, "server error: %s", msg.c_str());
        // TODO: handle error
        critical_exit(0);
    }
}

void t_aoo_server::receive(t_aoo_server *x) {
    auto err = x->x_server->receive(kAooInfinite);
    if (err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket::strerror(aoo::socket::get_last_error());
        } else {
            msg = aoo_strerror(err);
        }
        critical_enter(0);
        object_error((t_object*)x, "UDP error: %s", msg.c_str());
        // TODO: handle error
        critical_exit(0);
    }
}


static void aoo_server_handle_event(t_aoo_server *x, const AooEvent *event, int32_t)
{
    switch (event->type) {
    case kAooEventClientLogin:
    {
        auto& e = event->clientLogin;

        // TODO: address + metadata
        t_atom msg;
        char id[64];
        snprintf(id, sizeof(id), "0x%X", e.id);
        atom_setsym(&msg, gensym(id));

        if (e.error == kAooOk) {
            outlet_anything(x->x_msgout, gensym("client_add"), 1, &msg);

            x->x_numclients++;

            outlet_float(x->x_stateout, x->x_numclients);
        } else {
            object_error((t_object*)x, "client %d failed to login: %s",
                    e.id, aoo_strerror(e.error));
        }

        break;
    }
    case kAooEventClientLogout:
    {
        auto& e = event->clientLogout;

        if (e.errorCode != kAooOk) {
            object_error((t_object*)x, "client error: %s", e.errorMessage);
        }

        // TODO: address
        t_atom msg;
        char id[64];
        snprintf(id, sizeof(id), "0x%X", e.id);
        atom_setsym(&msg, gensym(id));

        outlet_anything(x->x_msgout, gensym("client_remove"), 1, &msg);

        x->x_numclients--;

        if (x->x_numclients >= 0) {
            outlet_float(x->x_stateout, x->x_numclients);
        } else {
            error("BUG: kAooEventClientLogout");
            x->x_numclients = 0;
        }

        break;
    }
    case kAooEventGroupAdd:
    {
        // TODO add group
        t_atom msg;
        atom_setsym(&msg, gensym(event->groupAdd.name));
        // TODO metadata
        outlet_anything(x->x_msgout, gensym("group_add"), 1, &msg);

        break;
    }
    case kAooEventGroupRemove:
    {
        // TODO remove group
        t_atom msg;
        atom_setsym(&msg, gensym(event->groupRemove.name));
        outlet_anything(x->x_msgout, gensym("group_remove"), 1, &msg);

        break;
    }
    case kAooEventGroupJoin:
    {
        auto& e = event->groupJoin;

        t_atom msg[3];
        atom_setsym(msg, gensym(e.groupName));
        atom_setsym(msg + 1, gensym(e.userName));
        atom_setfloat(msg + 2, e.userId); // always small
        outlet_anything(x->x_msgout, gensym("group_join"), 3, msg);

        break;
    }
    case kAooEventGroupLeave:
    {
        auto& e = event->groupLeave;

        t_atom msg[3];
        atom_setsym(msg, gensym(e.groupName));
        atom_setsym(msg + 1, gensym(e.userName));
        atom_setfloat(msg + 2, e.userId); // always small
        outlet_anything(x->x_msgout, gensym("group_leave"), 3, msg);

        break;
    }
    default:
        object_post((t_object*)x, "unknown event type (%d)", event->type);
        break;
    }
}

static void aoo_server_tick(t_aoo_server *x)
{
    x->x_server->pollEvents();
    clock_delay(x->x_clock, AOO_SERVER_POLL_INTERVAL);
}

static void aoo_server_relay(t_aoo_server *x, bool f) {
    x->x_server->setUseInternalRelay(f != 0);
}

static void aoo_server_port(t_aoo_server *x, double f)
{
    int port = f;
    if (port == x->x_port) {
        return;
    }

    x->close();

    x->x_numclients = 0;
    outlet_float(x->x_stateout, 0);

    if (port > 0) {
        AooServerSettings settings;
        settings.portNumber = port;
        if (auto err = x->x_server->setup(settings); err != kAooOk) {
            std::string msg;
            if (err == kAooErrorSocket) {
                msg = aoo::socket::strerror(aoo::socket::get_last_error());
            } else {
                msg = aoo_strerror(err);
            }
            object_error((t_object*)x, "%s: setup failed: %s", msg.c_str());
            return;
        }

	    t_max_err err;

        // start server threads
    	err = systhread_create((method)x->run, x, 0, 0, 0, &x->x_thread);
    	if(err != MAX_ERR_NONE){
        	object_error((t_object*)x, "Could not create server thread");
    	}
        // start udp thread
		err = systhread_create((method)x->receive, x, 0, 0, 0, &x->x_udp_thread);
    	if(err != MAX_ERR_NONE){
        	object_error((t_object*)x, "Could not create udp thread");
    	}
      
        // start clock
        clock_delay(x->x_clock, AOO_SERVER_POLL_INTERVAL);

        x->x_port = port;
    }
}

t_aoo_server::t_aoo_server(int argc, t_atom *argv)
{
	x_clock = clock_new(this, (method)aoo_server_tick);
	x_stateout = outlet_new(&x_obj, 0);
	x_msgout = outlet_new(&x_obj, 0);

	x_server = AooServer::create(); // does not really fail...

	// first set event handler!
	x_server->setEventHandler((AooEventHandler)aoo_server_handle_event,
                              this, kAooEventModePoll);

    int port = atom_getfloatarg(0, argc, argv);
    aoo_server_port(this, port);
}

t_aoo_server::~t_aoo_server()
{
    close();
    clock_free(x_clock);
}

void ext_main(void *r)
{
	t_class *c;

	c = class_new("aoo.server", (method)aoo_server_new, (method)aoo_server_free, (long)sizeof(t_aoo_server),
				  0L, A_GIMME, 0);

	class_addmethod(c, (method)aoo_server_assist, "assist", A_CANT, 0);
	class_addmethod(aoo_server_class, (method)aoo_server_port, "port", A_FLOAT, 0);
	class_addmethod(aoo_server_class, (method)aoo_server_relay, "relay", A_FLOAT, 0);


	class_register(CLASS_BOX, c); /* CLASS_NOBOX */
	aoo_server_class = c;

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
}

void aoo_server_assist(t_aoo_server *x, void *b, long m, long a, char *s)
{
	;
}

void aoo_server_free(t_aoo_server *x)
{
	 x->~t_aoo_server();
}


void *aoo_server_new(t_symbol *s, long argc, t_atom *argv)
{
	t_aoo_server *x = (t_aoo_server *)object_alloc(aoo_server_class);
	if (x) {
		new (x) t_aoo_server(argc, argv);
	}
	return (x);
}
