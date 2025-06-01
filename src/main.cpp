#include <Arduino.h>
#include <sensesp.h>
#include "sensesp_app_builder.h"
#include <sensesp/signalk/signalk_output.h>
#include <sensesp/transforms/linear.h>
#include <sensesp/transforms/lambda_transform.h>
#include <sensesp/sensors/digital_input.h>
#include <Preferences.h>  // Pour la persistance des données

using namespace sensesp;

const uint8_t PIN_IMPULSION = 18;
const uint8_t PIN_MONTER = 19;
const uint8_t PIN_DESCENDRE = 21;
const uint8_t PIN_RESET = 22;  // Pin du bouton de remise à zéro
const float LONGUEUR_PAR_IMPULSION_M = 0.168; // 6 maillons × 28 mm

Preferences prefs;  // Instance de Preferences pour la persistance

int impulsions = 0;
String sens = "stopped";
unsigned long t_last_monter_off = 0;
float longueur_cumulee = 0.0;

reactesp::ReactESP app;

void setup() {
  Serial.begin(115200);
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
    ->set_hostname("guindeau")
    ->set_wifi("TON_SSID", "TON_MOT_DE_PASSE")
    ->set_sk_server("192.168.1.100", 3000)  // Adresse du serveur Signal K
    ->get_app();

  // Ouverture de la mémoire pour lire la longueur précédente
  prefs.begin("guindeau", false);
  longueur_cumulee = prefs.getFloat("longueur", 0.0);  // Récupère la longueur sauvegardée

  // Capteur d’impulsions
  auto impulsion_input = new DigitalInputCounter(PIN_IMPULSION, INPUT_PULLUP, RISING, 100);

  // Entrées relais
  pinMode(PIN_MONTER, INPUT_PULLUP);
  pinMode(PIN_DESCENDRE, INPUT_PULLUP);
  pinMode(PIN_RESET, INPUT_PULLUP);  // Initialisation du bouton de remise à zéro

  // Détermination du sens de rotation
  auto direction_sensor = new LambdaTransform<int, String>(
    [](int dummy) {
      bool monter = digitalRead(PIN_MONTER);
      bool descendre = digitalRead(PIN_DESCENDRE);
      unsigned long now = millis();
      String s;

      if (monter) {
        t_last_monter_off = now;
        s = "up";
      } else if (descendre) {
        s = "down";
      } else {
        // Ni montée ni descente active
        if (now - t_last_monter_off < 3000) {
          s = "up";  // Inertie de montée
        } else {
          s = "down";  // Descente libre
        }
      }
      sens = s;
      return s;
    },
    "Sens de rotation"
  );

  // Transformation impulsions → mètres
  auto longueur_output = new Linear(LONGUEUR_PAR_IMPULSION_M, 0.0);

  // Relier la longueur à Signal K
  impulsion_input->connect_to(longueur_output)
                 ->connect_to(new SKOutputFloat("propulsion.anchorChain.length"));

  // Sauvegarder la longueur dans Preferences toutes les 10 secondes
  app.onRepeat(10000, []() {
    prefs.putFloat("longueur", longueur_cumulee);  // Sauvegarder la longueur actuelle
    Serial.print("Longueur sauvegardée : ");
    Serial.println(longueur_cumulee);
  });

  // Publier le sens dans Signal K chaque seconde
  app.onRepeat(1000, []() {
    static String last_sent = "";
    if (sens != last_sent) {
      auto sens_output = new SKOutputString("propulsion.anchorChain.direction");
      sens_output->set_input(sens);
      last_sent = sens;
    }
  });

  // Gérer la remise à zéro de la longueur
  app.onRepeat(100, []() {
    static unsigned long last_reset_time = 0;
    if (digitalRead(PIN_RESET) == LOW && millis() - last_reset_time > 500) {  // Délai anti-rebond
      // Réinitialiser la longueur
      longueur_cumulee = 0.0;
      prefs.putFloat("longueur", 0.0);  // Sauvegarder la longueur réinitialisée
      Serial.println("Longueur réinitialisée à 0.");
      last_reset_time = millis();  // Mettre à jour le temps pour éviter les réinitialisations multiples
    }
  });

  sensesp_app->start();
}

void loop() {
  app.tick();
}

// Fonction pour mettre à jour la longueur en fonction du sens
void updateLength(String direction) {
  if (direction == "up") {
    longueur_cumulee -= LONGUEUR_PAR_IMPULSION_M;  // Décrémenter la longueur en montée
  } else if (direction == "down") {
    longueur_cumulee += LONGUEUR_PAR_IMPULSION_M;  // Incrémenter la longueur en descente
  }
}
