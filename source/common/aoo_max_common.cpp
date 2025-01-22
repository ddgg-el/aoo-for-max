#include "aoo_max_common.hpp"

double get_elapsed_ms(AooNtpTime tt) {
    return aoo::time_tag::duration(g_start_time, tt);
}

void format_makedefault(AooFormatStorage &f, int nchannels)
{
    AooFormatPcm_init((AooFormatPcm *)&f, nchannels,
                      sys_getsr(), 64, kAooPcmFloat32);
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

int address_to_atoms(const aoo::ip_address& addr, int argc, t_atom *a)
{
    if (argc < 2){
        return 0;
    }
    atom_setsym(a, gensym(addr.name()));
    atom_setfloat(a + 1, addr.port());
    return 2;
}

int stream_message_to_atoms(const AooStreamMessage& msg, int argc, t_atom *argv) {
    assert(argc > 2);
    atom_setfloat(argv, msg.channel);
    AooData data { msg.type, msg.data, (AooSize)msg.size };
    return data_to_atoms(data, argc - 1, argv + 1) + 1;
}

// TODO: speriamo bene
double clock_getsystimeafter(double deltime)
{
	return systime_ms()*deltime;
}

// make sure we only actually query the time once per DSP tick.
// This is used in aoo_send~ and aoo_receive~, but also in aoo_client,
// if dejitter is disabled.
uint64_t get_osctime(){
    thread_local double lastclocktime = -1;
    thread_local aoo::time_tag osctime;

    auto now = gettime();
    if (now != lastclocktime){
        osctime = aoo::time_tag::now();
        lastclocktime = now;
    }
    return osctime;
}