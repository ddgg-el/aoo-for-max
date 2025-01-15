#include "ext.h"			// standard Max include
#include "z_dsp.h"
#include "aoo_types.h"

#include "aoo.h"
#include "aoo_client.hpp"
#include "aoo_sink.hpp"
#include "common/log.hpp"
#include "common/time.hpp"
#include "common/net_utils.hpp"
#include "common/priority_queue.hpp"


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

	virtual bool find_peer(const aoo::ip_address& addr,
                           t_symbol *& group, t_symbol *& user) const = 0;

    virtual bool find_peer(t_symbol * group, t_symbol * user,
                           aoo::ip_address& addr) const = 0;

	virtual int serialize_endpoint(const aoo::ip_address& addr, AooId id,
                                   int argc, t_atom *argv) const = 0;

	// virtual AooClient * client() = 0;

	// virtual int port() const = 0;

	// virtual void notify() = 0;
};

void format_makedefault(AooFormatStorage &f, int nchannels);

int datatype_element_size(AooDataType type);

double get_elapsed_ms(AooNtpTime tt);

int format_to_atoms(const AooFormat &f, int argc, t_atom *argv);

int data_to_atoms(const AooData& data, int argc, t_atom *argv);

int stream_message_to_atoms(const AooStreamMessage& data, int argc, t_atom *argv);

// TODO: speriamo bene
double clock_getsystimeafter(double deltime);

/*//////////////////////////// priority queue ////////////////////////////////*/


template<typename T>
struct t_queue_item {
    template<typename U>
    t_queue_item(U&& _data, double _time)
        : data(std::forward<U>(_data)), time(_time) {}
    T data;
    double time;
};

template<typename T>
bool operator> (const t_queue_item<T>& a, const t_queue_item<T>& b) {
    return a.time > b.time;
}

template<typename T>
using t_priority_queue = aoo::priority_queue<t_queue_item<T>, std::greater<t_queue_item<T>>>;