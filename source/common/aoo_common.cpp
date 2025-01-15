#include "aoo_common.hpp"
#include "codec/aoo_pcm.h"
#include "codec/aoo_null.h"

#if AOO_USE_OPUS
#include "codec/aoo_opus.h"
#endif


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

int format_to_atoms(const AooFormat &f, int argc, t_atom *argv)
{
    if (argc < 4){
        error("BUG: format_to_atoms: too few atoms!");
        return 0;
    }
    t_symbol *codec = gensym(f.codecName);
    atom_setsym(argv, codec);
    atom_setfloat(argv + 1, f.numChannels);
    atom_setfloat(argv + 2, f.blockSize);
    atom_setfloat(argv + 3, f.sampleRate);

    if (codec == gensym(kAooCodecNull)){
        // null <channels> <blocksize> <samplerate>
        return 4;
    } else if (codec == gensym(kAooCodecPcm)){
        // pcm <channels> <blocksize> <samplerate> <bitdepth>
        if (argc < 5){
            error("BUG: format_to_atoms: too few atoms for pcm format!");
            return 0;
        }
        auto& fmt = (AooFormatPcm &)f;
        int nbytes;
        switch (fmt.bitDepth){
        case kAooPcmInt8:
            nbytes = 1;
            break;
        case kAooPcmInt16:
            nbytes = 2;
            break;
        case kAooPcmInt24:
            nbytes = 3;
            break;
        case kAooPcmFloat32:
            nbytes = 4;
            break;
        case kAooPcmFloat64:
            nbytes = 8;
            break;
        default:
            object_error(0, "format_to_atoms: bad bitdepth argument %d", fmt.bitDepth);
            return 0;
        }
        atom_setfloat(argv + 4, nbytes);
        return 5;
    }
#if AOO_USE_OPUS
    else if (codec == gensym(kAooCodecOpus)){
        // opus <channels> <blocksize> <samplerate> <application>
        if (argc < 5){
            error("BUG: format_to_atoms: too few atoms for opus format!");
            return 0;
        }

        auto& fmt = (AooFormatOpus &)f;

        // application type
        t_symbol *type;
        switch (fmt.applicationType){
        case OPUS_APPLICATION_VOIP:
            type = gensym("voip");
            break;
        case OPUS_APPLICATION_RESTRICTED_LOWDELAY:
            type = gensym("lowdelay");
            break;
        case OPUS_APPLICATION_AUDIO:
            type = gensym("audio");
            break;
        default:
            object_error(0, "format_to_atoms: bad application type argument %d",
                  fmt.applicationType);
            return 0;
        }
        atom_setsym(argv + 4, type);

        return 5;
    }
#endif
    else {
        object_error(0, "format_to_atoms: unknown format %s!", codec->s_name);
    }
    return 0;
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

double clock_getsystimeafter(double deltime){
    return systime_ms()*deltime;
}