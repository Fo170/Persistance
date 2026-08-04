/**
 * @file persistance_EEPROM.h
 * @brief Persistance — backend EEPROM (Arduino EEPROM.h).
 *
 * PersistanceEEPROM : stocke les registres (double tampon validé CRC16) dans
 * l'EEPROM de la carte. L'espace EEPROM est typiquement 512 octets (ESP8266,
 * configurable). Chaque écriture doit être suivie de commit() pour être
 * persistée — ce commit est appelé automatiquement après chaque writeRecord.
 *
 * Exemple :
 *   PersistanceEEPROM persistance(512);
 *   persistance.begin();
 *   persistance.addRegister("coulomb", sizeof(float));
 *   float v = 123.4f;
 *   persistance.saveFloat("coulomb", v);
 *   float out;
 *   if (persistance.loadFloat("coulomb", &out)) { ... }
 *
 * Licence : GNU General Public License v3.0
 * Auteur   : Olivier FOURNET (olivier.fournet@free.fr)
 * Dépôt    : https://github.com/Fo170/Persistance
 */

#ifndef PERSISTANCE_EEPROM_H
#define PERSISTANCE_EEPROM_H

#include <Arduino.h>
#include <EEPROM.h>
#include "persist_store.h"

class PersistanceEEPROM : public PersistStore<PersistanceEEPROM> {
public:
    /**
     * @brief Constructeur.
     * @param size Taille de l'espace EEPROM à utiliser (octets). Défaut 512 (ESP8266).
     */
    explicit PersistanceEEPROM(uint16_t size = 512)
        : _size(size), _ready(false) {}

    // ---- Primitives média (utilisées par PersistStore) ----

    bool begin() {
        EEPROM.begin(_size);
        _ready = true;
        return true;
    }

    void end() {
        if (_ready) { EEPROM.end(); _ready = false; }
    }

    bool commit() {
        return _ready ? EEPROM.commit() : false;
    }

    bool isReady() const { return _ready; }

    uint16_t getSize() const { return _size; }

    bool readBytes(uint16_t addr, void* buf, uint16_t len) {
        if (!_ready || addr + len > _size) return false;
        uint8_t* p = static_cast<uint8_t*>(buf);
        for (uint16_t i = 0; i < len; i++) p[i] = EEPROM.read(addr + i);
        return true;
    }

    bool writeBytes(uint16_t addr, const void* buf, uint16_t len) {
        if (!_ready || addr + len > _size) return false;
        const uint8_t* p = static_cast<const uint8_t*>(buf);
        for (uint16_t i = 0; i < len; i++) EEPROM.write(addr + i, p[i]);
        return true;
    }

private:
    uint16_t _size;
    bool     _ready;
};

#endif // PERSISTANCE_EEPROM_H
