#include "aoo_common.hpp"
#include "codec/aoo_pcm.h"

void format_makedefault(AooFormatStorage &f, int nchannels)
{
    AooFormatPcm_init((AooFormatPcm *)&f, nchannels,
                      sys_getsr(), 64, kAooPcmFloat32);
}