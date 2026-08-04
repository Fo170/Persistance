/**
 * @file persistance.h
 * @brief Persistance — gestionnaire de sauvegardes pour EEPROM, FRAM et LittleFS.
 *
 * Bibliothèque header-only : inclut les trois backends ainsi que le cœur
 * commun (enregistrements validés CRC16 + double tampon anti-panne).
 *
 * Backends disponibles :
 *   - PersistanceEEPROM   → Arduino EEPROM.h (512 o typique) ;
 *   - PersistanceFRAM     → Fo170/FRAM_I2C_FM24CL16B (2 Ko, écriture immédiate) ;
 *   - PersistanceLittleFS → Arduino LittleFS.h (fichier unique, écriture atomique).
 *
 * API unifiée (après begin() et addRegister()) :
 *   saveByte/loadByte, saveUint16/loadUint16, saveUint32/loadUint32,
 *   saveFloat/loadFloat, saveBlock/loadBlock, saveString/loadString,
 *   isValid, needsInit, getVersion, clear, format.
 *
 * Licence : GNU General Public License v3.0
 * Auteur   : Olivier FOURNET (olivier.fournet@free.fr)
 * Dépôt    : https://github.com/Fo170/Persistance
 */

#ifndef PERSISTANCE_H
#define PERSISTANCE_H

#include "persistance_types.h"
#include "persistance_EEPROM.h"
#include "persistance_FRAM.h"
#include "persistance_LittleFS.h"

#endif // PERSISTANCE_H
