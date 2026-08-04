// Exemple : persistance LittleFS (fichier /persist.dat) avec PersistanceLittleFS.
// Écrit un float + une chaîne, relit au boot.

#include <Persistance.h>

PersistanceLittleFS persistance("/persist.dat", 2048);

void setup() {
    Serial.begin(115200);
    delay(500);

    if (!persistance.begin()) {
        Serial.println("LittleFS.begin échoué");
        return;
    }
    Serial.println("LittleFS monté");

    persistance.addRegister("level",  sizeof(float));
    persistance.addRegister("nom",    16);

    float level = 87.5f;
    persistance.saveFloat("level", level);
    persistance.saveString("nom", "batterie_1");

    float outLevel;
    char  outNom[16];
    if (persistance.loadFloat("level", &outLevel) &&
        persistance.loadString("nom", outNom, sizeof(outNom))) {
        Serial.print("Niveau : "); Serial.print(outLevel, 1);
        Serial.print(" %, nom : "); Serial.println(outNom);
    } else {
        Serial.println("Échec de relecture");
    }
}

void loop() {
    // Rien ici.
}
