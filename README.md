# Persistance

Gestionnaire de sauvegardes pour **EEPROM**, **FRAM** et **LittleFS** sur Arduino/ESP8266/ESP32.

Bibliothèque **header-only** : chaque backend expose la même API unifiée par **registres nommés**, avec enregistrements validés (magic + version + **CRC16 interne**) et **écriture double tampon anti-panne** (une coupure pendant l'écriture laisse toujours l'autre slot valide).

## Fonctionnalités

- **3 backends interchangeables** : `PersistanceEEPROM` (Arduino EEPROM), `PersistanceFRAM` ([Fo170/FRAM_I2C_FM24CL16B](https://github.com/Fo170/FRAM_I2C_FM24CL16B)), `PersistanceLittleFS` (fichier unique, écriture atomique).
- **Registres nommés** : chaque donnée = un slot nommé, adresses calculées automatiquement (plus de conflits de plage entre librairies).
- **Validation intégrale** : magic + version + CRC16 (implémenté en interne, aucune dépendance) → `isValid()`, `needsInit()`, `getVersion()`.
- **Anti-panne** : double tampon (slot A/B) avec compteur de séquence ; écriture LittleFS atomique (tmp + rename).
- **API typée** : `saveByte/loadByte`, `saveUint16/loadUint16`, `saveUint32/loadUint32`, `saveFloat/loadFloat`, `saveBlock/loadBlock`, `saveString/loadString`.

## Installation

### PlatformIO

```ini
[env:your_board]
lib_deps =
    Fo170/Persistance >= 1.0
```

### Arduino IDE

Sketch → Include Library → Manage Libraries → rechercher « Persistance », ou Ajouter .ZIP Library avec le dépôt.

## Exemple rapide

```cpp
#include <Persistance.h>

PersistanceEEPROM persistance(512);

void setup() {
    persistance.begin();
    persistance.addRegister("coulomb", sizeof(float));
    persistance.addRegister("config",  32);

    float v = 123.4f;
    persistance.saveFloat("coulomb", v);

    float out;
    if (persistance.loadFloat("coulomb", &out)) {
        Serial.print("Coulomb restauré : "); Serial.println(out);
    }
}
```

Même code avec un autre backend : remplacez `PersistanceEEPROM` par `PersistanceFRAM` ou `PersistanceLittleFS` (mêmes signatures).

## Changement de backend

| | EEPROM | FRAM | LittleFS |
|---|---|---|---|
| Header | `persistance_EEPROM.h` | `persistance_FRAM.h` | `persistance_LittleFS.h` |
| Taille | `begin(size)` (défaut 512) | fixe 2 Ko / puce | `maxSize` (défaut 2048) |
| Commit | `EEPROM.commit()` auto | écriture immédiate | `sync()` via tmp+rename |
| Stockage | EEPROM de la carte | puce I2C (FM24CL16B) | fichier `/persist.dat` |

## Compatibilité

Testé ou compatible avec : ESP8266, ESP32, AVR (Uno/Mega/Nano), Teensy, STM32 (via Arduino Core).

## Licence

GPL-3.0 — voir `LICENSE`.

## Auteur

Olivier FOURNET — olivier.fournet@free.fr — https://github.com/Fo170
