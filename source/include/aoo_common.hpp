#include "ext.h"			// standard Max include
#include "z_dsp.h"
#include "aoo_types.h"

#include "aoo.h"
#include "aoo_client.hpp"
#include "common/log.hpp"
#include "common/time.hpp"

#define AOO_MAX_NUM_CHANNELS 256

#define DEJITTER_TOLERANCE 0.1 // jitter tolerance in percent
#define DEJITTER_MAXDELTA 0.02 // max. expected jitter in seconds
#define DEJITTER_CHECK 1 // extra checks


struct t_dejitter;
t_dejitter *dejitter_get();

class t_node {
public: 
	static t_node * get(t_class *obj, int port, void *x = nullptr, AooId id = 0);

	// virtual ~t_node() {}

	virtual void release(t_class *obj, void *x = nullptr) = 0;

	// virtual AooClient * client() = 0;

	// virtual int port() const = 0;

	// virtual void notify() = 0;
};

void format_makedefault(AooFormatStorage &f, int nchannels);
