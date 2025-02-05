#include "ext.h"							// standard Max include, always required
#include "ext_obex.h"						// required for new style Max object

#include "aoo_max_common.hpp"
#include "aoo_server.hpp"

#define AOO_SERVER_POLL_INTERVAL 2

t_class *aoo_server_class;

////////////////////////// object struct
struct t_aoo_server
{


	t_object	x_obj;
    t_clock *x_clock = nullptr;
	AooServer::Ptr x_server;

	t_systhread x_thread;
	t_systhread x_udp_thread;

	int x_port = 0;
    int x_numclients = 0;

	t_outlet *x_stateout = nullptr;
    t_outlet *x_msgout = nullptr;

	t_aoo_server(int argc, t_atom *argv);
	~t_aoo_server();

	void close();
	static void run(t_aoo_server *x);
	static void receive(t_aoo_server *x);

};

static void aoo_server_handle_event(t_aoo_server *x, const AooEvent *event, int32_t);
static void aoo_server_tick(t_aoo_server *x);
static void aoo_server_port(t_aoo_server *x, double f);

///////////////////////// function prototypes
void *aoo_server_new(t_symbol *s, long argc, t_atom *argv);
void aoo_server_free(t_aoo_server *x);
void aoo_server_assist(t_aoo_server *x, void *b, long m, long a, char *s);

//////////////////////// global class pointer variable
