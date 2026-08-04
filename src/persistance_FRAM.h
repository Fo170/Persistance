/**
 * @file persistance_FRAM.h
 * @brief Persistance — backend FRAM I2C (Fo170/FRAM_I2C_FM24CL16B).
 *
 * PersistanceFRAM : stocke les registres (double tampon validé CRC16) dans une
 * FRAM FM24CL16B 16-Kbit (2K x 8) sur bus I2C. Contrairement à l'EEPROM, la
 * FRAM écrit immédiatement : aucun commit() n'est nécessaire (écriture non
 * volatile instantanée, ~10^14 cycles). La taille est fixe : 2048 octets par
 * puce (adresses 0x000..0x7FF).
 *
 * Dépendance : https://github.com/Fo170/FRAM_I2C_FM24CL16B
 *
 * Exemple :
 *   PersistanceFRAM persistance;
 *   persistance.begin();
 *   persistance.addRegister("coulomb", sizeof(float));
 *   persistance.saveFloat("coulomb", 123.4f);
 *   float out;
 *   if (persistance.loadFloat("coulomb", &out)) { ... }
 *
 * Licence : GNU General Public License v3.0
 * Auteur   : Olivier FOURNET (olivier.fournet@free.fr)
 * Dépôt    : https://github.com/Fo170/Persistance
 */

#ifndef PERSISTANCE_FRAM_H
#define PERSISTANCE_FRAM_H

#include <Arduino.h>
#include <Wire.h>
#include <FRAM_I2C_FM24CL16B.h>
#include "persist_store.h"

class PersistanceFRAM : public PersistStore<PersistanceFRAM> {
public:
    /**
     * @brief Constructeur.
     * @param addrConfig Configuration d'adresse (FRAM_ADDR_000 à FRAM_ADDR_111), défaut 000.
     * @param baseAddr   Offset d'adresse de base dans la puce (défaut 0).
     */
    explicit PersistanceFRAM(fram_address_config_t addrConfig = FRAM_ADDR_000,
                             uint16_t baseAddr = 0)
        : _fram(addrConfig, baseAddr), _ready(false) {}

    // ---- Primitives média (utilisées par PersistStore) ----

    bool begin() {
        _ready = _fram.begin();
        return _ready;
    }

    void end() { _ready = false; }

    bool commit() { return _ready; }

    bool isReady() const { return _ready; }

    uint16_t getSize() const {
        return _ready ? _fram.getSize() : 0;
    }

    bool readBytes(uint16_t addr, void* buf, uint16_t len) {
        if (!_ready) return false;
        return _fram.read(addr, buf, len) == FRAM_OK;
    }

    bool writeBytes(uint16_t addr, const void* buf, uint16_t len) {
        if (!_ready) return false;
        return _fram.write(addr, buf, len) == FRAM_OK;
    }

private:
    FRAM_FM24CL16B _fram;
    bool           _ready;
};

#endif // PERSISTANCE_FRAM_H
