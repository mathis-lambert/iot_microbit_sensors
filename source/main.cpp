/*
 * ============================================================================
 * Fichier      : main.cpp
 * Auteur       : Mathis LAMBERT
 * Projet       : Acquisition et transmission de données BME280 et TSL256x chiffrées via micro:bit
 * Date         : Mai 2025
 * Description  :
 *   Ce programme lit les données environnementales (température, humidité, pression, luminosité)
 *   à partir d'un capteur BME280 et TSL256x, les affiche sur un écran OLED, puis les transmet
 *   en radio via le protocole personnalisé CPE (Capteurs - Payload - Encryption).
 *
 *   La trame radio est de 7 octets :
 *     - 1 octet de séquence (SEQ), servant également de nonce
 *     - 9 octets de données capteurs chiffrées (AES-128 en mode CTR)
 *
 *   Le chiffrement est symétrique et utilise la bibliothèque TinyCrypt.
 *
 * Dépendances :
 *   - micro:bit runtime API
 *   - bme280.h : lecture capteur
 *   - ssd1306.h : affichage OLED
 *   - tsl256x.h : lecture capteur
 *   - cpe.h / cpe.c : protocole de chiffrement CPE
 * ============================================================================
 */


 #include "MicroBit.h"
 #include "bme280.h"
 #include "ssd1306.h"
 #include "tsl256x.h"
 #include <cstdlib>
 #include "cpe.h"
 
 #define RADIO_GROUP 42
 
 static const uint8_t KEY[16] = {
     0x00, 0x01, 0x02, 0x03,
     0x04, 0x05, 0x06, 0x07,
     0x08, 0x09, 0x0A, 0x0B,
     0x0C, 0x0D, 0x0E, 0x0F
 };
 
 MicroBit uBit;
 MicroBitI2C i2c(I2C_SDA0, I2C_SCL0);
 MicroBitPin oledReset(MICROBIT_ID_IO_P0, MICROBIT_PIN_P0, PIN_CAPABILITY_DIGITAL_OUT);
 
 static const char BLANK_LINE[] = "                "; // ligne vide de l'écran OLED (16 caractères)
 
 /* === Prototype : réception et affichage dynamique === */
 void onRadio(MicroBitEvent);
 
 /* === Transmission des mesures via CPE === */
 void sendCPE(const cpe_measure_t *m, uint8_t seq)
 {
     uint8_t f[CPE_PAYLOAD_LEN];
     cpe_build_frame(m, seq, f);
     uBit.radio.datagram.send(f, CPE_PAYLOAD_LEN);
 }
 
 /* === Affichage dynamique selon control === */
 void oled_display_measures(ssd1306 &oled, const cpe_measure_t &m)
 {
     char line[32];
     // Affichage selon le mode Control (TLH ou LTH)
     if (m.control == CPE_CTRL_TLH) {
         snprintf(line, sizeof(line), "T:%d.%02dC",  m.temperature_centi/100, abs(m.temperature_centi)%100);
         oled.display_line(0, 0, line);
 
         snprintf(line, sizeof(line), "Lux:%d",  m.lux);
         oled.display_line(1, 0, line);
 
         snprintf(line, sizeof(line), "H:%d.%02d%%", m.humidity_centi/100, m.humidity_centi%100);
         oled.display_line(2, 0, line);
 
         snprintf(line, sizeof(line), "P:%d.%01dhPa", m.pressure_decihPa/10, m.pressure_decihPa%10);
         oled.display_line(3, 0, line);
 
     } else if (m.control == CPE_CTRL_LTH) {
         snprintf(line, sizeof(line), "Lux:%d",  m.lux);
         oled.display_line(0, 0, line);
 
         snprintf(line, sizeof(line), "T:%d.%02dC",  m.temperature_centi/100, abs(m.temperature_centi)%100);
         oled.display_line(1, 0, line);
 
         snprintf(line, sizeof(line), "H:%d.%02d%%", m.humidity_centi/100, m.humidity_centi%100);
         oled.display_line(2, 0, line);
 
         snprintf(line, sizeof(line), "P:%d.%01dhPa", m.pressure_decihPa/10, m.pressure_decihPa%10);
         oled.display_line(3, 0, line);
     } else {
         // mode inconnu : fallback classique
         snprintf(line, sizeof(line), "T:%d.%02dC",  m.temperature_centi/100, abs(m.temperature_centi)%100);
         oled.display_line(0, 0, line);
         snprintf(line, sizeof(line), "H:%d.%02d%%", m.humidity_centi/100, m.humidity_centi%100);
         oled.display_line(1, 0, line);
         snprintf(line, sizeof(line), "P:%d.%01dhPa", m.pressure_decihPa/10, m.pressure_decihPa%10);
         oled.display_line(2, 0, line);
         snprintf(line, sizeof(line), "Lux:%d",  m.lux);
         oled.display_line(3, 0, line);
     }
     oled.update_screen();
 }
 
 /* === Handler de réception radio === */
 void onRadio(MicroBitEvent)
 {
     PacketBuffer p = uBit.radio.datagram.recv();
     if (p.length() == CPE_PAYLOAD_LEN) {
         cpe_measure_t m = {0};
         if (cpe_parse_frame(p.getBytes(), &m) == 0) {
             // Nettoie l'écran avant affichage
             for (int i = 0; i < 4; i++) oled.display_line(i, 0, BLANK_LINE);
             oled_display_measures(oled, m);
         }
     }
 }
 
 int main()
 {
     uBit.init();
     uBit.radio.setGroup(RADIO_GROUP);
     uBit.radio.enable();
 
     cpe_init(KEY);
 
     bme280   bme(&uBit, &i2c);
     ssd1306  oled(&uBit, &i2c, &oledReset);
     tsl256x  tsl(&uBit, &i2c);
     oled.power_on();
 
     uint16_t tsl_comb = 0, tsl_ir = 0, tsl_lux = 0;
     uint32_t rawP; int32_t rawT; uint16_t rawH;
     static uint8_t seq = 0;
 
     // Ajout handler de réception radio
     uBit.messageBus.listen(MICROBIT_ID_RADIO, MICROBIT_RADIO_EVT_DATAGRAM, onRadio);
 
     while (true) {
         // Lecture des capteurs
         bme.sensor_read(&rawP, &rawT, &rawH);
         int16_t  tCenti = bme.compensate_temperature(rawT);   // 0,01 °C
         uint16_t hCenti = bme.compensate_humidity(rawH);      // 0,01 %RH
         uint16_t pDeci  = bme.compensate_pressure(rawP) / 10; // 0,1 hPa
         tsl.sensor_read(&tsl_comb, &tsl_ir, &tsl_lux);
 
         // Construction de la structure à transmettre
         cpe_measure_t measures;
         measures.temperature_centi = tCenti;
         measures.humidity_centi    = hCenti;
         measures.pressure_decihPa  = pDeci;
         measures.lux               = (int16_t)tsl_lux;
         measures.control           = CPE_CTRL_TLH; // Envoie TLH par défaut
 
         sendCPE(&measures, seq);
         seq++;
 
         uBit.sleep(2000);
     }
 
     release_fiber();
 }
 