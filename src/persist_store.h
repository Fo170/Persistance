/**
 * @file persist_store.h
 * @brief Persistance — moteur générique de stockage par registres + double tampon.
 *
 * Header interne (non publié) : base CRTP PersistStore<Impl> qui implémente toute la
 * logique indépendante du média :
 *   - table de registres nommés (PersistTable) ;
 *   - écriture double tampon (slot A / slot B) avec séquence — une coupure pendant
 *     l'écriture laisse toujours l'autre slot valide ;
 *   - validation magic + CRC16 au chargement, fallback valeurs par défaut ;
 *   - API typée : saveByte/loadByte, saveUint16/loadUint16, saveUint32/loadUint32,
 *     saveFloat/loadFloat, saveBlock/loadBlock, saveString/loadString.
 *
 * La classe dérivée (Impl) doit fournir les primitives média :
 *   bool begin();  void end();  bool commit();
 *   bool readBytes(uint16_t addr, void* buf, uint16_t len);
 *   bool writeBytes(uint16_t addr, const void* buf, uint16_t len);
 *   uint16_t getSize();  bool isReady();
 *
 * Licence : GNU General Public License v3.0
 * Auteur   : Olivier FOURNET (olivier.fournet@free.fr)
 */

#ifndef PERSIST_STORE_H
#define PERSIST_STORE_H

#include "persistance_types.h"

// Taille d'un tampon d'enregistrement (en-tête + données max + CRC).
#define PERSIST_RECORD_BUFFER  (PERSIST_RECORD_HEADER + PERSIST_MAX_DATA_SIZE + 2)

template <class Impl>
class PersistStore {
public:
    PersistStore() {}

    bool begin()                { return impl().begin(); }
    void end()                  { impl().end(); }
    bool commit()               { return impl().commit(); }
    bool isReady() const        { return impl().isReady(); }
    uint16_t getSize() const    { return impl().getSize(); }
    uint16_t getMaxAddr() const { uint16_t s = getSize(); return s > 0 ? s - 1 : 0; }

    bool addRegister(const char* name, uint16_t size, uint8_t version = 1, uint16_t offset = 0) {
        return _table.addRegister(name, size, version, offset);
    }

    uint8_t  registerCount() const { return _table.count(); }
    uint16_t totalSize() const     { return _table.totalSize(); }

    // ---------- Écritures typées ----------

    bool saveByte(const char* name, uint8_t v) {
        return writeRecord(name, PERSIST_TYPE_BYTE, &v, 1);
    }
    bool saveUint16(const char* name, uint16_t v) {
        return writeRecord(name, PERSIST_TYPE_UINT16, &v, 2);
    }
    bool saveUint32(const char* name, uint32_t v) {
        return writeRecord(name, PERSIST_TYPE_UINT32, &v, 4);
    }
    bool saveFloat(const char* name, float v) {
        return writeRecord(name, PERSIST_TYPE_FLOAT, &v, 4);
    }
    bool saveBlock(const char* name, const void* data, uint16_t len) {
        return writeRecord(name, PERSIST_TYPE_BLOCK, data, len);
    }
    bool saveString(const char* name, const char* str) {
        return writeRecord(name, PERSIST_TYPE_STRING, str, strlen(str));
    }

    // ---------- Lectures typées ----------

    bool loadByte(const char* name, uint8_t* v) {
        return readRecord(name, PERSIST_TYPE_BYTE, v, 1);
    }
    bool loadUint16(const char* name, uint16_t* v) {
        return readRecord(name, PERSIST_TYPE_UINT16, v, 2);
    }
    bool loadUint32(const char* name, uint32_t* v) {
        return readRecord(name, PERSIST_TYPE_UINT32, v, 4);
    }
    bool loadFloat(const char* name, float* v) {
        return readRecord(name, PERSIST_TYPE_FLOAT, v, 4);
    }
    bool loadBlock(const char* name, void* data, uint16_t len) {
        return readRecord(name, PERSIST_TYPE_BLOCK, data, len);
    }
    bool loadString(const char* name, char* str, uint16_t maxLen) {
        if (!readRecord(name, PERSIST_TYPE_STRING, str, maxLen - 1)) return false;
        str[maxLen - 1] = '\0';
        return true;
    }

    // ---------- Validation ----------

    bool isValid(const char* name) {
        uint8_t seq, version; uint16_t size; int slot;
        return readWinning(name, seq, version, size, slot) != nullptr;
    }

    bool needsInit(const char* name) { return !isValid(name); }

    uint8_t getVersion(const char* name) {
        uint8_t seq, version; uint16_t size; int slot;
        const PersistRecord* r = readWinning(name, seq, version, size, slot);
        return r ? r->version : 0;
    }

    bool clear(const char* name) {
        const PersistRegister* reg = _table.getRegister(name);
        if (!reg) return false;
        uint8_t zero[8] = {0};
        bool okA = impl().writeBytes(reg->offset, zero, 8);
        bool okB = impl().writeBytes(reg->offset + reg->slotSize, zero, 8);
        commit();
        return okA && okB;
    }

    bool format() {
        bool ok = true;
        for (uint8_t i = 0; i < _table.count(); i++) {
            const PersistRegister* reg = _table.regAt(i);
            if (!reg) continue;
            uint8_t zero[8] = {0};
            ok = impl().writeBytes(reg->offset, zero, 8) && ok;
            ok = impl().writeBytes(reg->offset + reg->slotSize, zero, 8) && ok;
        }
        commit();
        return ok;
    }

protected:
    /**
     * @brief Lit les deux slots d'un registre et renvoie le plus récent valide.
     * @return Pointeur vers l'enregistrement gagnant (statique), ou nullptr.
     */
    const PersistRecord* readWinning(const char* name, uint8_t& seqOut, uint8_t& verOut,
                                     uint16_t& sizeOut, int& slotOut) {
        const PersistRegister* reg = _table.getRegister(name);
        if (!reg) return nullptr;

        static uint8_t  bufs[2][PERSIST_RECORD_BUFFER];
        static PersistRecord recs[2];

        for (int s = 0; s < 2; s++) {
            uint16_t addr = reg->offset + (uint16_t)s * reg->slotSize;
            bool ok = impl().readBytes(addr, bufs[s], reg->slotSize);
            recs[s] = PersistRecord();
            if (ok && persistDecode(bufs[s], reg->size, recs[s])) {
                recs[s].data = bufs[s] + PERSIST_RECORD_HEADER;
            }
        }

        int win = -1;
        if (recs[0].isValid() && recs[1].isValid()) {
            win = persistSeqNewer(recs[0].seq, recs[1].seq) ? 0 : 1;
        } else if (recs[0].isValid()) {
            win = 0;
        } else if (recs[1].isValid()) {
            win = 1;
        }
        if (win < 0) return nullptr;

        seqOut = recs[win].seq;
        verOut = recs[win].version;
        sizeOut = recs[win].size;
        slotOut = win;
        return &recs[win];
    }

    bool readRecord(const char* name, uint8_t expectedType, void* data, uint16_t maxLen) {
        uint8_t seq, version; uint16_t size; int slot;
        const PersistRecord* rec = readWinning(name, seq, version, size, slot);
        if (!rec) return false;
        if (rec->type != expectedType) return false;
        if (rec->size > maxLen) return false;
        if (data && rec->size > 0) memcpy(data, rec->data, rec->size);
        return true;
    }

    bool writeRecord(const char* name, uint8_t type, const void* data, uint16_t len) {
        const PersistRegister* reg = _table.getRegister(name);
        if (!reg || len > reg->size) return false;

        // Séquence suivante : (séquence du slot gagnant + 1), ou 1 si vierge.
        uint8_t nextSeq = 1;
        uint8_t curSeq, curVer; uint16_t curSize; int curSlot = 0;
        const PersistRecord* cur = readWinning(name, curSeq, curVer, curSize, curSlot);
        if (cur) nextSeq = (uint8_t)(cur->seq + 1);

        static uint8_t buf[PERSIST_RECORD_BUFFER];
        persistEncode(buf, nextSeq, reg->version, type, data, len);
        uint16_t recordBytes = persistRecordSize(len);

        // Double tampon : on écrit dans le slot opposé au gagnant actuel.
        // Si aucun slot valide (première écriture), on écrit dans le slot A.
        int targetSlot = (cur == nullptr) ? 0 : (1 - curSlot);
        uint16_t target = reg->offset + (uint16_t)targetSlot * reg->slotSize;

        bool ok = impl().writeBytes(target, buf, recordBytes);
        if (ok) commit();
        return ok;
    }

    PersistTable _table;

private:
    Impl& impl() { return *static_cast<Impl*>(this); }
    const Impl& impl() const { return *static_cast<const Impl*>(this); }
};

#endif // PERSIST_STORE_H
