#include "cpe.h"
#include <string.h>
#include <tinycrypt/aes.h>
#include <tinycrypt/ctr_mode.h>

static uint8_t g_key[CPE_KEY_LEN];

/* --- Packing/unpacking optimisé big-endian --- */
// big-endian : octet de poids fort en premier
// little-endian : octet de poids faible en premier donc il faut inverser
static void pack(const cpe_measure_t *m, uint8_t p[CPE_PLAINTEXT_LEN])
{
    p[0] = ((uint16_t)m->temperature_centi) >> 8; // décalage de 8 bits à droite
    p[1] = ((uint16_t)m->temperature_centi) & 0xFF; // masque pour garder les 8 bits de poids faible
    p[2] = (m->humidity_centi) >> 8; 
    p[3] = (m->humidity_centi) & 0xFF;
    p[4] = (m->pressure_decihPa) >> 8;
    p[5] = (m->pressure_decihPa) & 0xFF;
    p[6] = ((uint16_t)m->lux) >> 8;
    p[7] = ((uint16_t)m->lux) & 0xFF;
    p[8] = m->control;
}

static void unpack(const uint8_t p[CPE_PLAINTEXT_LEN], cpe_measure_t *m)
{
    m->temperature_centi =  (int16_t)((p[0] << 8) | p[1]);
    m->humidity_centi    = (uint16_t)((p[2] << 8) | p[3]);
    m->pressure_decihPa  = (uint16_t)((p[4] << 8) | p[5]);
    m->lux               =  (int16_t)((p[6] << 8) | p[7]);
    m->control           = p[8];
}

static void crypt(uint8_t *buf, const uint8_t iv[16])
{
    struct tc_aes_key_sched_struct ks;
    tc_aes128_set_encrypt_key(&ks, g_key);
    uint8_t ctr[16];
    memcpy(ctr, iv, 16);
    tc_ctr_mode(buf, CPE_PLAINTEXT_LEN, buf, CPE_PLAINTEXT_LEN, ctr, &ks);
}

/* --- API --- */
void cpe_init(const uint8_t key[CPE_KEY_LEN])
{
    memcpy(g_key, key, CPE_KEY_LEN);
}

void cpe_build_frame(const cpe_measure_t *m, uint8_t seq, uint8_t out_frame[CPE_PAYLOAD_LEN])
{
    uint8_t iv[16] = {0};
    iv[15] = seq;

    uint8_t buf[CPE_PLAINTEXT_LEN];
    pack(m, buf);
    crypt(buf, iv);

    out_frame[0] = seq;
    memcpy(out_frame + 1, buf, CPE_PLAINTEXT_LEN);
}

int cpe_parse_frame(const uint8_t frame[CPE_PAYLOAD_LEN], cpe_measure_t *measures)
{
    if (!frame || !measures) return -1;

    uint8_t seq = frame[0];
    uint8_t iv[16] = {0};
    iv[15] = seq;

    uint8_t buf[CPE_PLAINTEXT_LEN];
    memcpy(buf, frame + 1, CPE_PLAINTEXT_LEN);
    crypt(buf, iv);
    unpack(buf, measures);
    return 0;
}
