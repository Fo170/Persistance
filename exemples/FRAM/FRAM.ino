// Exemple : persistance FRAM I2C (FM24CL16B, adresse 0x50) avec PersistanceFRAM.
// Écrit un compteur de boot qui survit au reset, plus un float.

#include <persistance.h>

PersistanceFRAM persistance;

void setup() {
    Serial.begin(115200);
    delay(500);

    if (!persistance.begin()) {
        Serial.println("FRAM non détectée (vérifier SDA/SCL et adresse)");
        return;
    }
    Serial.print("FRAM détectée, taille : "); Serial.print(persistance.getSize());
    Serial.println(" octets");

    persistance.addRegister("boot",   sizeof(uint32_t));
    persistance.addRegister("data",   sizeof(float));

    // Compteur de boot persistant.
    uint32_t boot = 0;
    if (persistance.loadUint32("boot", &boot)) {
        boot++;
    } else {
        boot = 1;
    }
    persistance.saveUint32("boot", boot);
    Serial.print("Boot n° "); Serial.println(boot);

    // Float persistant.
    float value = 3.14159f;
    persistance.saveFloat("data", value);
    float out;
    if (persistance.loadFloat("data", &out)) {
        Serial.print("Valeur restaurée : "); Serial.println(out, 5);
    }
}

void loop() {
    // Rien ici : l'écriture FRAM est immédiate (pas de commit nécessaire).
}
