#pragma once

#include "aoo_max_client.h"

///////////////////////// MAX function prototypes
//// standard set
void *aoo_client_new(t_symbol *s, long argc, t_atom *argv);
void aoo_client_free(t_aoo_client *x);
void aoo_client_assist(t_aoo_client *x, void *b, long m, long a, char *s);