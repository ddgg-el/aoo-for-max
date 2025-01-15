#include "aoo_common.hpp"

extern t_class *aoo_client_class;

struct t_aoo_client;

bool aoo_client_find_peer(t_aoo_client *x, const aoo::ip_address& addr,
                          t_symbol *& group, t_symbol *& user);

bool aoo_client_find_peer(t_aoo_client *x, t_symbol *group, t_symbol *user,
                          aoo::ip_address& addr);


class t_node_imp final : public t_node
{

private:

    t_object * x_clientobj = nullptr;
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


};

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