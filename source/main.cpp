/*
 * ============================================================================
 * Fichier      : main.cpp
 * Auteur       : Mathis LAMBERT
 * Projet       : Simulation aléatoire + protocole CPE (micro:bit)
 * Date         : Mai 2025
 * Description  :
 *   - Génère des données aléatoires (BME280 & TSL256x simulés)
 *   - Affiche les mesures sur l'OLED toutes les secondes, selon l'ordre défini
 *   - Envoie un paquet CPE contenant les mesures toutes les 2 secondes
 *   - Réception : si un paquet CTRL est reçu, on ajuste l'ordre d'affichage
 *
 *   ⚠ Pour repasser en mode "capteurs réels", dé-commentez les blocs
 *     // CAPTEURS et commentez la partie // SIMULATION.
 * ============================================================================
 */

#include "MicroBit.h"
#include "bme280.h" // CAPTEURS
#include "ssd1306.h"
#include "tsl256x.h" // CAPTEURS
#include "cpe.h"     // Protocole CPE v2
#include <cstdlib>

#define RADIO_GROUP 42
#define DEVICE_ID 0x02 /* identifiant unique pour ce micro:bit */

static const uint8_t KEY[16] = {
    0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F};

MicroBit uBit;
MicroBitI2C i2c(I2C_SDA0, I2C_SCL0);
MicroBitPin P0(MICROBIT_ID_IO_P0, MICROBIT_PIN_P0, PIN_CAPABILITY_DIGITAL_OUT);
static ssd1306 *oled = nullptr; // construit après uBit.init()
static bme280 *bme = nullptr;   // construit après uBit.init()
static tsl256x *tsl = nullptr;  // construit après uBit.init()

static const char BLANK_LINE[] = "                "; // 16 espaces

/* === Variables globales === */
static uint8_t seq = 0; // Sequence radio
static uint8_t current_ctrl = cpe_ctrl_pack(CPE_S_T, CPE_S_L, CPE_S_H, CPE_S_P);
static cpe_measure_t lastMeasures{}; // zero-initialisé

/* === Prototypes === */
void onRadio(MicroBitEvent);
static void sendMeasureFrame(const cpe_measure_t *m);
static void generateOrReadSensors(cpe_measure_t *out);
static void displayMeasures(const cpe_measure_t &m);

/* --- utilitaire visuel ------------------------------------------- */
static inline void flash(uint8_t x, uint8_t y)
{
    uBit.display.image.setPixelValue(x, y, 255);
    fiber_sleep(50);
    uBit.display.image.setPixelValue(x, y, 0);
}

/* === Transmission de trames CPE === */
static void sendMeasureFrame(const cpe_measure_t *m)
{
    uint8_t frame[CPE_PAYLOAD_LEN];
    cpe_build_measure_frame(m, DEVICE_ID, seq++, frame);
    uBit.serial.send("[INFO] Envoi Paquet");
    int ret = uBit.radio.datagram.send(frame, CPE_PAYLOAD_LEN);

    if (ret != MICROBIT_OK)
    {
        uBit.serial.send("[ERROR] Envoi échoué\n");
        return;
    }
    uBit.serial.send("[INFO] Paquet envoyé\n");
    flash(0, 0);
}

/* === Affichage selon l'ordre courant === */
static void displayMeasures(const cpe_measure_t &m)
{
    if (!oled)
        return;

    char line[24];
    cpe_sensor_t order[4];
    cpe_ctrl_unpack(current_ctrl, order);

    for (int row = 0; row < 4; ++row)
        oled->display_line(row, 0, BLANK_LINE);

    for (int row = 0; row < 4; ++row)
    {
        switch (order[row])
        {
        case CPE_S_T:
            snprintf(line, sizeof(line), "T:%d.%02dC", m.temperature_centi / 100, abs(m.temperature_centi) % 100);
            break;
        case CPE_S_L:
            snprintf(line, sizeof(line), "Lux:%d", m.lux);
            break;
        case CPE_S_H:
            snprintf(line, sizeof(line), "H:%d.%02d%%", m.humidity_centi / 100, m.humidity_centi % 100);
            break;
        case CPE_S_P:
            snprintf(line, sizeof(line), "P:%d.%01dhPa", m.pressure_decihPa / 10, m.pressure_decihPa % 10);
            break;
        default:
            snprintf(line, sizeof(line), "--");
            break;
        }
        oled->display_line(row, 0, line);
    }
    oled->update_screen();
}

/* === Réception radio === */
void onRadio(MicroBitEvent)
{
    flash(0, 1); // signal de réception

    PacketBuffer p = uBit.radio.datagram.recv();
    if (p.length() != CPE_PAYLOAD_LEN)
    {
        uBit.serial.send("[ERROR] Paquet reçu de taille incorrecte\n");
        return;
    }

    cpe_frame_type_t ft;
    uint8_t dev_id;
    cpe_measure_t meas;
    uint8_t ctrl;

    if (cpe_parse_frame(p.getBytes(), &ft, &dev_id, &meas, &ctrl) != 0)
        return;

    uBit.serial.send("[INFO] Paquet reçu\n");
    uBit.serial.send("[INFO] Type: ");

    if (ft == CPE_FT_CONTROL)
    {
        current_ctrl = ctrl; // met à jour l'ordre d'affichage
        uBit.serial.send("[CTRL] Nouvel ordre OLED reçu\n");
    }
}

/* === Génération ou lecture des capteurs === */
static void generateOrReadSensors(cpe_measure_t *out)
{
    /* -----------------------------------------------------------------
     *  CAPTEURS : Lecture réelle (commentée pour la simulation)
     * ----------------------------------------------------------------- */
    uint32_t rawP;
    int32_t rawT;
    uint16_t rawH;
    uint16_t tsl_comb, tsl_ir;
    uint32_t tsl_lux;
    int res = bme->sensor_read(&rawP, &rawT, &rawH);
    int16_t tCenti = bme->compensate_temperature(rawT);
    uint16_t hCenti = bme->compensate_humidity(rawH);
    uint16_t pDeci = bme->compensate_pressure(rawP) / 10;
    int res_tsl = tsl->sensor_read(&tsl_comb, &tsl_ir, &tsl_lux);
    uint16_t lux = tsl_comb;
    /* -----------------------------------------------------------------*/

    /* -----------------------------------------------------------------
     *  SIMULATION : Génération aléatoire des mesures (active)
     * -----------------------------------------------------------------*/
    // int16_t tCenti = 1500 + uBit.random(1500);  // 15.00 °C .. 29.99 °C
    // uint16_t hCenti = 3000 + uBit.random(4000); // 30.00 % .. 69.99 %
    // uint16_t pDeci = 10000 + uBit.random(500);  // 1000.0 hPa .. 1049.9 hPa
    // uint16_t lux = uBit.random(2000);           // 0 .. 1999 lux

    out->temperature_centi = tCenti;
    out->humidity_centi = hCenti;
    out->pressure_decihPa = pDeci;
    out->lux = lux;

    char log[64];
    snprintf(log, sizeof(log), "[TRUE] T:%d.%02dC H:%d.%02d%% P:%d.%01dhPa Lux:%d\r\n",
             tCenti / 100, abs(tCenti) % 100,
             hCenti / 100, hCenti % 100,
             pDeci / 10, pDeci % 10,
             lux);
    uBit.serial.send(log);
}

/* === Programme principal === */
int main()
{
    uBit.init();
    uBit.serial.send("[INFO] micro:bit ready\n");

    /* --- Périphériques --- */
    oled = new ssd1306(&uBit, &i2c, &P0);
    uBit.serial.send("[INFO] OLED ok\n");

    bme = new bme280(&uBit, &i2c);  // CAPTEURS
    tsl = new tsl256x(&uBit, &i2c); // CAPTEURS
    uBit.serial.send("[INFO] Capteurs BME & TSL ok\n");

    cpe_init(KEY);

    uBit.radio.setTransmitPower(7);
    int ret = uBit.radio.setGroup(RADIO_GROUP);
    if (ret != MICROBIT_OK)
    {
        uBit.serial.send("[ERROR] setGroup failed\n");
        release_fiber();
    }

    int ret2 = uBit.radio.enable();
    if (ret2 != MICROBIT_OK)
    {
        uBit.serial.send("[ERROR] enable failed\n");
        release_fiber();
    }

    uBit.messageBus.listen(MICROBIT_ID_RADIO, MICROBIT_RADIO_EVT_DATAGRAM, onRadio);

    /* --- Boucle principale --- */
    uint32_t lastDisplayMs = 0;
    uint32_t lastSendMs = 0;

    while (true)
    {
        uint32_t now = system_timer_current_time();

        /* Affichage toutes les secondes */
        if (now - lastDisplayMs >= 1000)
        {
            lastDisplayMs = now;
            generateOrReadSensors(&lastMeasures);
            displayMeasures(lastMeasures);
        }

        /* Envoi radio toutes les deux secondes */
        if (now - lastSendMs >= 2000)
        {
            lastSendMs = now;
            sendMeasureFrame(&lastMeasures);
        }

        /* Bouton A : reset ordre OLED par défaut */
        if (uBit.buttonA.isPressed())
            current_ctrl = cpe_ctrl_pack(CPE_S_T, CPE_S_H, CPE_S_P, CPE_S_L);

        uBit.sleep(50); // petite pause pour laisser souffler le scheduler
    }

    release_fiber();
}
