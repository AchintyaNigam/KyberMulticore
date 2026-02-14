#include "profile.h"
#include <string.h>

keygen_profile_t kg_prof;
enc_profile_t enc_prof;
dec_profile_t dec_prof;

void profile_reset(void)
{
    memset(&kg_prof, 0, sizeof(kg_prof));
    memset(&enc_prof, 0, sizeof(enc_prof));
    memset(&dec_prof, 0, sizeof(dec_prof));
}
