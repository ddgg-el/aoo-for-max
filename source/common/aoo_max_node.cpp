#include "ext_critical.h"

#include "aoo_max_node.h"

#include "common/log.hpp"
#include "common/sync.hpp"



// TODO:documenta
t_node * t_node::get(t_class *obj, int port, void *x, AooId id)
{
    t_node_imp *node = nullptr;
    // make bind symbol for port number
    char buf[64];
    snprintf(buf, sizeof(buf), "aoo_node %d", port);
    t_symbol *s = gensym(buf);
    // find or create node
    // TODO:
    // auto y = (t_node_proxy *)pd_findbyclass(s, node_proxy_class);
    // if (y) {
    //     node = y->x_node;
    // } else {
    try {
        // finally create aoo node instance
        node = new t_node_imp(s, port);
    } catch (const std::exception& e) {
        object_error((t_object*)obj, "%s: %s", object_classname(obj), e.what());
        return nullptr;
    }
    // }

    if (!node->add_object(obj, x, id)){
        // never fails for new t_node_imp!
        return nullptr;
    }

    return node;
}

// TODO: documenta
t_node_imp::t_node_imp(t_symbol *s, int port)
    : x_port(port) //, x_bindsym(s),
{
    LOG_DEBUG("create AooClient on port " << port);
    auto client = AooClient::create();

    // client->setEventHandler([](void *user, const AooEvent *event, AooThreadLevel level) {
    //     auto x = static_cast<t_node_imp *>(user);
    //     if (x->x_clientobj) {
    //         aoo_client_handle_event((t_aoo_client *)x->x_clientobj, event, level);
    //     }
    // }, this, kAooEventModePoll);

    AooClientSettings settings;
    settings.portNumber = port;
    if (auto err = client->setup(settings); err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket::strerror(aoo::socket::get_last_error());
        } else {
            msg = aoo_strerror(err);
        }
        throw std::runtime_error("could not create node: " + msg);
    }
    // get socket type
    if (settings.socketType & kAooSocketIPv6) {
        x_type = aoo::ip_address::IPv6;
    } else {
        x_type = aoo::ip_address::IPv4;
    }
    x_ipv4mapped = settings.socketType & kAooSocketIPv4Mapped;

    // success
    x_client = std::move(client);

    // pd_bind(&x_proxy.x_pd, x_bindsym);
/*
#if NETWORK_THREAD_POLL
    // start network I/O thread
    LOG_DEBUG("start network thread");
    x_iothread = std::thread([this, pd=pd_this]() {
    #ifdef PDINSTANCE
        pd_setinstance(pd);
    #endif
        aoo::sync::lower_thread_priority();
        perform_io();
    });
#else
    // start send thread
    LOG_DEBUG("start network send thread");
    x_sendthread = std::thread([this, pd=pd_this]() {
    #ifdef PDINSTANCE
        pd_setinstance(pd);
    #endif
        aoo::sync::lower_thread_priority();
        send();
    });
    // start receive thread
    LOG_DEBUG("start network receive thread");
    x_recvthread = std::thread([this, pd=pd_this]() {
    #ifdef PDINSTANCE
        pd_setinstance(pd);
    #endif
        aoo::sync::lower_thread_priority();
        receive();
    });
#endif
*/

    object_post(nullptr, "aoo: new node on port %d", port);
}

// TODO: documenta
t_node_imp::~t_node_imp()
{
    // pd_unbind(&x_proxy.x_pd, x_bindsym);

    // stop the client and join threads
    x_client->stop();
    if (x_clientthread.joinable()){
        x_clientthread.join();
    }
#if NETWORK_THREAD_POLL
    LOG_DEBUG("join network thread");
    // notify and join I/O thread
    x_quit.store(true);
    if (x_iothread.joinable()) {
        x_iothread.join();
    }
#else
    LOG_DEBUG("join network threads");
    // join both threads
    if (x_sendthread.joinable()) {
        x_sendthread.join();
    }
    if (x_recvthread.joinable()) {
        x_recvthread.join();
    }
#endif

    object_post(nullptr, "aoo: released node on port %d", x_port);
}
// TODO: documenta
bool t_node_imp::find_peer(const aoo::ip_address& addr,
                           t_symbol *& group, t_symbol *& user) const {
    if (x_clientobj &&
            aoo_client_find_peer(
               (t_aoo_client *)x_clientobj, addr, group, user)) {
        return true;
    } else {
        return false;
    }
}

// TODO: documenta
bool t_node_imp::find_peer(t_symbol * group, t_symbol * user,
                           aoo::ip_address& addr) const {
    if (x_clientobj &&
            aoo_client_find_peer(       
                (t_aoo_client *)x_clientobj, group, user, addr)) {
        return true;
    } else {
        return false;
    }
}

//TODO: documenta
int t_node_imp::serialize_endpoint(const aoo::ip_address &addr, AooId id,
                                   int argc, t_atom *argv) const {
    if (argc < 3 || !addr.valid()) {
        LOG_DEBUG("serialize_endpoint: invalid address");
        return 0;
    }
    t_symbol *group, *user;
    if (find_peer(addr, group, user)) {
        // group name, user name, id
        atom_setsym(argv, group);
        atom_setsym(argv + 1, user);
        atom_setfloat(argv + 2, id);
    } else {
        // ip string, port number, id
        atom_setsym(argv, gensym(addr.name()));
        atom_setfloat(argv + 1, addr.port());
        atom_setfloat(argv + 2, id);
    }
    return 3;
}
// TODO: documenta
void t_node_imp::run_client() {
    auto err = x_client->run(kAooInfinite);
    if (err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket::strerror(aoo::socket::get_last_error());
        } else {
            msg = aoo_strerror(err);
        }
        critical_enter(0);
        object_error((t_object*)x_clientobj, "%s: TCP error: %s",
                 (x_clientobj ? "aoo_client" : "aoo"), msg.c_str());
        // TODO: handle error
        critical_exit(0);
    }
}

// TODO: documenta
bool t_node_imp::add_object(t_class *obj, void *x, AooId id)
{
    if (obj == aoo_client_class){
        // aoo_client
        if (!x_clientobj){
            // TODO: verifica!
            x_clientobj = (t_object*)obj;
            // start thread lazily
            if (!x_clientthread.joinable()){
                x_clientthread = std::thread([this](){
                    aoo::sync::lower_thread_priority();
                    run_client();
                });
            }
        } else {
            object_error((t_object*)obj, "%s on port %d already exists!",
                     object_classname(obj), port());
            return false;
        }
    } else  if (obj == aoo_send_class){
        if (x_client->addSource((AooSource *)x) != kAooOk){
            object_error((t_object*)obj, "%s with ID %d on port %d already exists!",
                     object_classname(obj), id, port());
        }
    } else if (obj == aoo_receive_class){
        if (x_client->addSink((AooSink *)x) != kAooOk){
            object_error((t_object*)obj, "%s with ID %d on port %d already exists!",
                     object_classname(obj), id, port());
        }
    } else {
        error("BUG: t_node_imp: bad client");
        return false;
    }
    x_refcount++;
    return true;
}


// TODO: documenta
void t_node_imp::release(t_class *obj, void *x)
{
    if (obj == aoo_client_class){
        // client
        x_clientobj = nullptr;
    } else if (obj == aoo_send_class){
        x_client->removeSource((AooSource *)x);
    } else if (obj == aoo_receive_class){
        x_client->removeSink((AooSink *)x);
    } else {
        error("BUG: t_node_imp::release");
        return;
    }

    if (--x_refcount == 0){
        // last instance
        delete this;
    } else if (x_refcount < 0){
        error("BUG: t_node_imp::release: negative refcount!");
    }
}

#if NETWORK_THREAD_POLL
void t_node_imp::perform_io() {
    auto check_error = [this](auto err) {
        if (err != kAooOk && err != kAooErrorWouldBlock) {
            std::string msg;
            if (err == kAooErrorSocket) {
                msg = aoo::socket_strerror(aoo::socket_errno());
            } else {
                msg = aoo_strerror(err);
            }
            sys_lock();
            pd_error(x_clientobj, "%s: UDP error: %s",
                     (x_clientobj ? "aoo_client" : "aoo"), msg.c_str());
            // TODO: handle error
            sys_unlock();
            return false;
        } else {
            return true;
        }
    };

    while (!x_quit.load(std::memory_order_relaxed)) {
        auto t1 = aoo::time_tag::now();

        auto err1 = x_client->receive(0);
        if (!check_error(err1)) {
            break;
        }

        auto err2 = x_client->send(0);
        if (!check_error(err2)) {
            break;
        }

        if (err1 == kAooErrorWouldBlock && err2 == kAooErrorWouldBlock) {
            // sleep
            auto t2 = aoo::time_tag::now();
            auto remaining = aoo::time_tag::duration(t1, t2) - POLL_INTERVAL;
            auto dur = std::chrono::duration<double>(remaining);
            std::this_thread::sleep_for(dur);
        }
    }
}
#else
void t_node_imp::send() {
    auto err = x_client->send(kAooInfinite);
    if (err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket::strerror(aoo::socket::get_last_error());
        } else {
            msg = aoo_strerror(err);
        }
        critical_enter(0);
        object_error(x_clientobj, "%s: UDP send error: %s",
                 (x_clientobj ? "aoo_client" : "aoo"), msg.c_str());
        // TODO: handle error
        critical_exit(0);
    }
}

void t_node_imp::receive() {
    auto err = x_client->receive(kAooInfinite);
    if (err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket::strerror(aoo::socket::get_last_error());
        } else {
            msg = aoo_strerror(err);
        }
        critical_enter(0);
        object_error(x_clientobj, "%s: UDP receive error: %s",
                 (x_clientobj ? "aoo_client" : "aoo"), msg.c_str());
        // TODO: handle error
        critical_exit(0);
    }
}
#endif

/*////////////////////// aoo node //////////////////*/

