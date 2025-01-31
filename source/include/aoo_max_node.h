#pragma once

#include "ext.h"			// standard Max include
#include "aoo_max_common.hpp"

#include "aoo_client.hpp"
#include "common/net_utils.hpp"

#include <thread>

extern t_class *aoo_client_class;
extern t_class *aoo_send_class;
extern t_class *aoo_receive_class;
extern t_class *aoo_node_class;

///////////////////////////// t_node ////////////////////////////////
class t_node {
public: 
	static t_node * get(t_object *obj, int port, void *x = nullptr, AooId id = 0);

	virtual ~t_node() {}

	virtual void release(t_object *obj, void *x = nullptr) = 0;

	virtual bool find_peer(const aoo::ip_address& addr,
                           t_symbol *& group, t_symbol *& user) const = 0;

    virtual bool find_peer(t_symbol * group, t_symbol * user,
                           aoo::ip_address& addr) const = 0;

	virtual int serialize_endpoint(const aoo::ip_address& addr, AooId id,
                                   int argc, t_atom *argv) const = 0;

	virtual AooClient * client() = 0;

	virtual int port() const = 0;

	virtual void notify() = 0;

    virtual bool resolve(t_symbol *host, int port, aoo::ip_address& addr) const = 0;
};

class t_node_imp final : public t_node
{

private:
    t_object * x_clientobj = nullptr;
    t_symbol *x_bindsym;

    AooClient::Ptr x_client;
    int32_t x_refcount = 0;
    int x_port = 0;

    aoo::ip_address::ip_type x_type = aoo::ip_address::Unspec;
    bool x_ipv4mapped = true;

    // threading
    t_systhread x_clientthread;
#if NETWORK_THREAD_POLL
    std::thread x_iothread;
    std::atomic<bool> x_quit{false};
#else
    t_systhread x_sendthread;
    t_systhread x_recvthread;
#endif
    static void run_client(t_node_imp *x);
public:
    // public methods
    t_node_imp(t_symbol *s, int port);

    ~t_node_imp();

    bool find_peer(const aoo::ip_address& addr,
                   t_symbol *& group, t_symbol *& user) const override;

    bool find_peer(t_symbol * group, t_symbol * user,
                   aoo::ip_address& addr) const override;

    int serialize_endpoint(const aoo::ip_address &addr, AooId id,
                           int argc, t_atom *argv) const override;

    void release(t_object *obj, void *x) override;

    bool add_object(t_object *obj, void *x, AooId id);

    int port() const override { return x_port; }

    AooClient * client() override { return x_client.get(); }

     void notify() override { x_client->notify(); }

     bool resolve(t_symbol *host, int port, aoo::ip_address& addr) const override;


#if NETWORK_THREAD_POLL
    void perform_io();
#else
    static void send(t_node_imp*);

    static void receive(t_node_imp*);
#endif

};

struct t_aoo_client;

void aoo_client_handle_event(t_aoo_client *x, const AooEvent *event, int32_t level);

bool aoo_client_find_peer(t_aoo_client *x, const aoo::ip_address& addr,
                          t_symbol *& group, t_symbol *& user);

bool aoo_client_find_peer(t_aoo_client *x, t_symbol * group, t_symbol * user,
                          aoo::ip_address& addr);

void aoo_node_setup(void);
