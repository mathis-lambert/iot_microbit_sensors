#ifndef CPE_H
#define CPE_H

#include <stdint.h>
#include <stddef.h>

#define CPE_PAYLOAD_LEN    10   /* Octets envoyés sur la radio          */
#define CPE_PLAINTEXT_LEN   9   /* Octets de données capteurs bruts     */
#define CPE_KEY_LEN        16   /* Taille de la clé AES‑128             */

/** Codes de modes de contrôle pour Control */
typedef enum {
    CPE_CTRL_TLH = 0,
    CPE_CTRL_LTH = 1
} cpe_ctrl_t;

/** Structure portant les mesures, en unités fixes :
 *  - temperature_centi : 0,01 °C  (‑327,68 à +327,67 °C)
 *  - humidity_centi    : 0,01 %RH (0 à 655,35 %)
 *  - pressure_decihPa  : 0,1 hPa  (0 à 6 553,5 hPa)
 *  - lux               : brut     (0 à 65535) 
 *  - control           : cpe_ctrl_t (0 -> 'TLH' ou 1 -> 'LTH')
 */
typedef struct {
    int16_t  temperature_centi;  // 0,01 °C
    uint16_t humidity_centi;     // 0,01 %RH
    uint16_t pressure_decihPa;   // 0,1 hPa
    int16_t  lux;                // Intensity lumière, brut
    uint8_t  control;            // cpe_ctrl_t
} cpe_measure_t;

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise la couche crypto du protocole.
 *  @param key  clé AES‑128 partagée (16 octets)
 */
void cpe_init(const uint8_t key[CPE_KEY_LEN]);

/** Construit (encode + chiffre) une trame radio CPE.
 *  @param m         mesures à encoder
 *  @param seq       numéro de séquence (0‑255) – sert de nonce
 *  @param out_frame tampon d'au moins CPE_PAYLOAD_LEN octets
 */
void cpe_build_frame(const cpe_measure_t *m, uint8_t seq, uint8_t out_frame[CPE_PAYLOAD_LEN]);

/** Parse (déchiffre + décode) une trame CPE reçue.
 *  @param frame     tableau de CPE_PAYLOAD_LEN octets
 *  @param measures  structure en sortie
 *  @return 0 si OK, ‑1 si erreur
 */
int  cpe_parse_frame(const uint8_t frame[CPE_PAYLOAD_LEN], cpe_measure_t *measures);

#ifdef __cplusplus
}
#endif

#endif /* CPE_H */
