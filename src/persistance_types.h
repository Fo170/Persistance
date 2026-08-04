/**
 * @file persistance_types.h
 * @brief Persistance — types communs : enregistrements validés, CRC16 interne, table de registres.
 *
 * Bibliothèque header-only pour gérer la persistance de données sur EEPROM, FRAM et LittleFS.
 * Ce fichier définit le cœur commun :
 *   - format d'enregistrement : magic + seq + version + type + taille + données + CRC16 ;
 *   - implémentation interne du CRC16 (table-driven, aucune dépendance externe) ;
 *   - encodage/décodage des enregistrements (validation magic + CRC) ;
 *   - table de registres nommés (allocation d'espace par slot, adresses calculées).
 *
 * Licence : GNU General Public License v3.0
 * Auteur   : Olivier FOURNET (olivier.fournet@free.fr)
 * Dépôt    : https://github.com/Fo170/Persistance
 */

#ifndef PERSISTANCE_TYPES_H
#define PERSISTANCE_TYPES_H

#include <Arduino.h>
#include <string.h>

// ============================================================
// CONSTANTES DU FORMAT D'ENREGISTREMENT
// ============================================================

// Magic value : distingue une zone vierge / invalide d'une donnée réelle.
#define PERSIST_MAGIC            0x3412CDABUL

// Taille maximale d'un enregistrement (données utiles).
#define PERSIST_MAX_DATA_SIZE    256

// Type d'enregistrement (stocké pour déceler un mauvais cast au chargement).
enum PersistType : uint8_t {
    PERSIST_TYPE_UNDEFINED = 0,
    PERSIST_TYPE_BYTE,
    PERSIST_TYPE_UINT16,
    PERSIST_TYPE_UINT32,
    PERSIST_TYPE_FLOAT,
    PERSIST_TYPE_BLOCK,
    PERSIST_TYPE_STRING
};

// En-tête d'un enregistrement (hors données et CRC).
//   [0..3] magic   uint32
//   [4]    seq     uint8  (séquence de la sauvegarde, pour l'anti-panne double tampon)
//   [5]    version uint8  (version du format du registre)
//   [6]    type    uint8
//   [7..8] taille  uint16 (taille des données utiles)
#define PERSIST_RECORD_HEADER    9

// ============================================================
// CRC16 INTERNE (poly 0x1021, init 0xFFFF)
// ============================================================

namespace PersistCRC {
    /**
     * @brief Calcule le CRC16 (CCITT, poly 0x1021, init 0xFFFF) d'un buffer.
     * @param data Pointeur sur les données
     * @param len  Nombre d'octets
     * @return CRC16 calculé
     */
    inline uint16_t crc16(const uint8_t* data, uint16_t len) {
        uint16_t crc = 0xFFFF;
        for (uint16_t i = 0; i < len; i++) {
            crc ^= (uint16_t)(data[i]) << 8;
            for (uint8_t bit = 0; bit < 8; bit++) {
                if (crc & 0x8000)
                    crc = (crc << 1) ^ 0x1021;
                else
                    crc <<= 1;
            }
        }
        return crc;
    }
}

// ============================================================
// ENREGISTREMENT (format sur le média)
// ============================================================

struct PersistRecord {
    uint32_t magic;
    uint8_t  seq;      // séquence de sauvegarde (mod 256)
    uint8_t  version;  // version du format du registre
    uint8_t  type;     // PersistType
    uint16_t size;     // taille des données utiles
    const uint8_t* data;

    bool isValid() const {
        return magic == PERSIST_MAGIC && size <= PERSIST_MAX_DATA_SIZE;
    }
};

/**
 * @brief Taille totale sur le média d'un enregistrement (données + en-tête + CRC).
 */
inline uint16_t persistRecordSize(uint16_t dataSize) {
    return PERSIST_RECORD_HEADER + dataSize + 2;
}

/**
 * @brief Détermine si seqA est plus récente que seqB (gère le retour à zéro mod 256).
 */
inline bool persistSeqNewer(uint8_t seqA, uint8_t seqB) {
    return (uint8_t)(seqA - seqB) < 128;
}

/**
 * @brief Encode un enregistrement dans un buffer (taille >= persistRecordSize(size)).
 */
inline void persistEncode(uint8_t* dst, uint8_t seq, uint8_t version, uint8_t type,
                          const void* data, uint16_t size) {
    uint32_t magic = PERSIST_MAGIC;
    memcpy(dst, &magic, 4);
    dst[4] = seq;
    dst[5] = version;
    dst[6] = type;
    memcpy(dst + 7, &size, 2);
    if (size > 0 && data) memcpy(dst + PERSIST_RECORD_HEADER, data, size);

    uint16_t crc = PersistCRC::crc16(dst, PERSIST_RECORD_HEADER + size);
    dst[PERSIST_RECORD_HEADER + size] = (uint8_t)(crc & 0xFF);
    dst[PERSIST_RECORD_HEADER + size + 1] = (uint8_t)(crc >> 8);
}

/**
 * @brief Décode et valide un enregistrement depuis un buffer.
 * @param src     Buffer lu depuis le média
 * @param maxSize Taille maximale des données attendues
 * @param out     Enregistrement rempli si valide
 * @return true si magic + CRC valides
 */
inline bool persistDecode(const uint8_t* src, uint16_t maxSize, PersistRecord& out) {
    memcpy(&out.magic, src, 4);
    out.seq = src[4];
    out.version = src[5];
    out.type = src[6];
    memcpy(&out.size, src + 7, 2);
    out.data = src + PERSIST_RECORD_HEADER;

    if (out.magic != PERSIST_MAGIC) return false;
    if (out.size > PERSIST_MAX_DATA_SIZE || out.size > maxSize) return false;

    uint16_t expectedCrc = (uint16_t)(src[PERSIST_RECORD_HEADER + out.size])
                           | ((uint16_t)(src[PERSIST_RECORD_HEADER + out.size + 1]) << 8);
    uint16_t actualCrc = PersistCRC::crc16(src, PERSIST_RECORD_HEADER + out.size);
    return expectedCrc == actualCrc;
}

// ============================================================
// TABLE DE REGISTRES
// ============================================================
// Un registre = un slot nommé réservant 2 emplacements (double tampon) contigus.
// La taille physique réservée est donc : 2 * persistRecordSize(size).

#define PERSIST_MAX_REGISTERS 16

struct PersistRegister {
    char     name[24];        // nom du registre (slot)
    uint16_t offset;          // adresse de début dans le média (octets)
    uint16_t size;            // taille des données utiles (octets)
    uint16_t slotSize;        // taille d'un emplacement (header + data + crc)
    uint8_t  version;         // version du format de ce registre
    bool     inUse;
};

class PersistTable {
public:
    PersistTable() : _count(0) {}

    /**
     * @brief Ajoute un registre nommé dans la table.
     * @param name    Nom du registre (<= 23 caractères)
     * @param size    Taille des données (octets)
     * @param version Version initiale du format
     * @param offset  Adresse de début (0 = auto-après le dernier, aligné sur 4)
     * @return true si ajouté
     */
    bool addRegister(const char* name, uint16_t size, uint8_t version = 1, uint16_t offset = 0) {
        if (!name || !name[0] || _count >= PERSIST_MAX_REGISTERS) return false;

        uint16_t slotSize = persistRecordSize(size);
        uint16_t start = offset;
        if (offset == 0 && _count > 0) {
            start = (_registers[_count - 1].offset + 2 * (uint16_t)_registers[_count - 1].slotSize + 3) & ~3U;
        }

        strncpy(_registers[_count].name, name, sizeof(_registers[_count].name) - 1);
        _registers[_count].name[sizeof(_registers[_count].name) - 1] = '\0';
        _registers[_count].offset = start;
        _registers[_count].size = size;
        _registers[_count].slotSize = slotSize;
        _registers[_count].version = version;
        _registers[_count].inUse = true;
        _count++;
        return true;
    }

    /**
     * @brief Retrouve un registre par nom.
     * @return Pointeur sur le registre ou nullptr
     */
    const PersistRegister* getRegister(const char* name) const {
        for (uint8_t i = 0; i < _count; i++) {
            if (_registers[i].inUse && strcmp(_registers[i].name, name) == 0)
                return &_registers[i];
        }
        return nullptr;
    }

    uint8_t count() const { return _count; }

    /**
     * @brief Accès direct à un registre par index (0..count()-1).
     */
    const PersistRegister* regAt(uint8_t index) const {
        if (index >= _count) return nullptr;
        return &_registers[index];
    }

    /**
     * @brief Taille totale réservée sur le média.
     */
    uint16_t totalSize() const {
        if (_count == 0) return 0;
        return _registers[_count - 1].offset + 2 * (uint16_t)_registers[_count - 1].slotSize;
    }

private:
    PersistRegister _registers[PERSIST_MAX_REGISTERS];
    uint8_t _count;
};

#endif // PERSISTANCE_TYPES_H
