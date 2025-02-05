/**
	@file
	aoo_client - a max object shell
	jeremy bernstein - jeremy@bootsquad.com

	@ingroup	examples
*/
#include "aoo_client.h"

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



void ext_main(void *r)
{
	t_class *c;

	c = class_new("aoo.client", (method)aoo_client_new, (method)aoo_client_free, (long)sizeof(t_aoo_client), 0L, A_GIMME, 0);

	class_addmethod(c, (method)aoo_client_assist,			"assist",		A_CANT, 0);

	class_register(CLASS_BOX, c); /* CLASS_NOBOX */
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

    if (auto [ok, msg] = aoo::check_ntp_server(); ok){
        // post("NTP receive server");
    } else {
        error("%s", msg.c_str());
    }

    g_start_time = aoo::time_tag::now();
	aoo_dejitter_setup();

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
}

void aoo_client_assist(t_aoo_client *x, void *b, long m, long a, char *s)
{
	// if (m == ASSIST_INLET) { // inlet
	// 	sprintf(s, "I am inlet %ld", a);
	// }
	// else {	// outlet
	// 	sprintf(s, "I am outlet %ld", a);
	// }
    ;
}

void aoo_client_free(t_aoo_client *x)
{
	;
}


void *aoo_client_new(t_symbol *s, long argc, t_atom *argv)
{
	t_aoo_client *x = (t_aoo_client *)object_alloc(aoo_client_class);
	new (x) t_aoo_client(argc,argv);
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