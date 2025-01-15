#include "aoo_common.hpp"
#include "codec/aoo_pcm.h"

void format_makedefault(AooFormatStorage &f, int nchannels)
{
    AooFormatPcm_init((AooFormatPcm *)&f, nchannels,
                      sys_getsr(), 64, kAooPcmFloat32);
}

int datatype_element_size(AooDataType type) {
    switch (type) {
    case kAooDataFloat32:
        return 4;
    case kAooDataFloat64:
        return 8;
    case kAooDataInt16:
        return 2;
    case kAooDataInt32:
        return 4;
    case kAooDataInt64:
        return 8;
    default:
        return 1;
    }
}

//originariamente era in setup
static AooNtpTime g_start_time;
double get_elapsed_ms(AooNtpTime tt) {
    return aoo::time_tag::duration(g_start_time, tt);
}

int stream_message_to_atoms(const AooStreamMessage& msg, int argc, t_atom *argv) {
    assert(argc > 2);
    atom_setfloat(argv, msg.channel);
    AooData data { msg.type, msg.data, (AooSize)msg.size };
    return data_to_atoms(data, argc - 1, argv + 1) + 1;
}

int data_to_atoms(const AooData& data, int argc, t_atom *argv) {
    assert(data.size != 0);
    auto numatoms = data.size / datatype_element_size(data.type) + 1;
    if (numatoms > argc) {
        return 0;
    }

    switch (data.type) {
    case kAooDataFloat32:
    case kAooDataFloat64:
    case kAooDataInt16:
    case kAooDataInt32:
    case kAooDataInt64:
        atom_setsym(argv, gensym("f"));
        break;
    default:
        atom_setsym(argv, gensym(aoo_dataTypeToString(data.type)));
        break;
    }

    auto ptr = data.data;
    for (int i = 1; i < numatoms; ++i) {
        double f;
        switch (data.type) {
        case kAooDataFloat32:
            f = aoo::read_bytes<float>(ptr);
            break;
        case kAooDataFloat64:
            f = aoo::read_bytes<double>(ptr);
            break;
        case kAooDataInt16:
            f = aoo::read_bytes<int16_t>(ptr);
            break;
        case kAooDataInt32:
            f = aoo::read_bytes<int32_t>(ptr);
            break;
        case kAooDataInt64:
            f = aoo::read_bytes<int64_t>(ptr);
            break;
        default:
            f = *ptr++;
            break;
        }
        atom_setfloat(argv + i, f);
    }
    assert(ptr <= (data.data + data.size));
    return numatoms;
}