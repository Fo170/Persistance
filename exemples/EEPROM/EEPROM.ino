// Exemple : persistance EEPROM avec PersistanceEEPROM
// Écrit un float (registre "coulomb") + un bloc config, puis relit au boot.

#include <persistance.h>

PersistanceEEPROM persistance(512);

struct Config {
    uint16_t sample_ms;
    uint8_t  filtrage;
    char     nom[12];
};

void setup() {
    Serial.begin(115200);
    delay(500);

    if (!persistance.begin()) {
        Serial.println("EEPROM.begin échoué");
        return;
    }

    // Déclaration des registres (slots nommés).
    persistance.addRegister("coulomb", sizeof(float));
    persistance.addRegister("config",  sizeof(Config));

    // Chargement au boot.
    float coulomb = 0.0f;
    if (persistance.loadFloat("coulomb", &coulomb)) {
        Serial.print("Coulomb restauré : "); Serial.println(coulomb, 2);
    } else {
        Serial.println("Premier démarrage (coulomb vierge) — valeur par défaut 0");
    }

    Config cfg;
    if (persistance.loadBlock("config", &cfg, sizeof(cfg))) {
        Serial.print("Config restaurée : sample_ms="); Serial.print(cfg.sample_ms);
        Serial.print(", filtrage="); Serial.print(cfg.filtrage);
        Serial.print(", nom="); Serial.println(cfg.nom);
    } else {
        Serial.println("Config vierge — valeurs par défaut");
    }
}

void loop() {
    // Exemple : sauvegarde périodique d'un compteur coulomb simulé.
    static unsigned long last = 0;
    if (millis() - last >= 5000) {
        last = millis();
        float coulomb = 42.5f;
        if (persistance.saveFloat("coulomb", coulomb)) {
            Serial.print("Coulomb sauvegardé : "); Serial.println(coulomb, 2);
        }
    }
}
