/**
 * @file persistance_LittleFS.h
 * @brief Persistance — backend LittleFS (fichier unique sur flash).
 *
 * PersistanceLittleFS : stocke les registres (double tampon validé CRC16) dans
 * un fichier unique sur le système de fichiers LittleFS (ex. ESP8266/ESP32).
 * Le fichier est lu/écrit par offset : l'API est identique à EEPROM/FRAM.
 * L'écriture est atomique : on écrit d'abord dans un fichier temporaire
 * (.tmp) puis rename() — une coupure laisse l'ancien fichier intact.
 *
 * Exemple :
 *   PersistanceLittleFS persistance;
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

#ifndef PERSISTANCE_LITTLEFS_H
#define PERSISTANCE_LITTLEFS_H

#include <Arduino.h>
#include <LittleFS.h>
#include "persist_store.h"

class PersistanceLittleFS : public PersistStore<PersistanceLittleFS> {
public:
    /**
     * @brief Constructeur.
     * @param path     Chemin du fichier de persistance (défaut "/persist.dat").
     * @param maxSize  Taille maximale du fichier (octets).
     */
    explicit PersistanceLittleFS(const char* path = "/persist.dat", uint16_t maxSize = 2048)
        : _path(path), _maxSize(maxSize), _ready(false) {}

    // ---- Primitives média (utilisées par PersistStore) ----

    bool begin() {
        _ready = LittleFS.begin();
        if (_ready && !LittleFS.exists(_path)) {
            // Crée le fichier s'il n'existe pas.
            File f = LittleFS.open(_path, "w");
            if (f) { f.close(); } else { _ready = false; }
        }
        return _ready;
    }

    void end() { _ready = false; }

    bool commit() { return _ready; }

    bool isReady() const { return _ready; }

    uint16_t getSize() const { return _maxSize; }

    bool readBytes(uint16_t addr, void* buf, uint16_t len) {
        if (!_ready || addr + len > _maxSize) return false;
        File f = LittleFS.open(_path, "r");
        if (!f) return false;
        if (!f.seek(addr)) { f.close(); return false; }
        size_t n = f.read(static_cast<uint8_t*>(buf), len);
        f.close();
        return n == len;
    }

    bool writeBytes(uint16_t addr, const void* buf, uint16_t len) {
        if (!_ready || addr + len > _maxSize) return false;

        // Lecture complète du fichier actuel dans un buffer.
        static uint8_t filebuf[2048];
        uint16_t curSize = 0;
        File old = LittleFS.open(_path, "r");
        if (old) {
            curSize = old.read(filebuf, _maxSize);
            old.close();
        }

        // Patch du buffer.
        if (addr + len > curSize) {
            memset(filebuf + curSize, 0, addr + len - curSize);
            curSize = addr + len;
        }
        memcpy(filebuf + addr, buf, len);

        // Écriture atomique : fichier temporaire puis rename.
        String tmp = String(_path) + ".tmp";
        File f = LittleFS.open(tmp.c_str(), "w");
        if (!f) return false;
        bool ok = f.write(filebuf, curSize) == curSize;
        f.close();
        if (ok) {
            ok = LittleFS.remove(_path);
            ok = LittleFS.rename(tmp.c_str(), _path) && ok;
        }
        return ok;
    }

private:
    const char* _path;
    uint16_t    _maxSize;
    bool        _ready;
};

#endif // PERSISTANCE_LITTLEFS_H
