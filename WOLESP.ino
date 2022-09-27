#include <WiFi.h>
#include <IOXhop_FirebaseESP32.h>
#include <AutoConnect.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>

#define FIREBASE_Host "https://wolespdb-default-rtdb.europe-west1.firebasedatabase.app/"
#define FIREBASE_key "WlGaovFnduqO9owzAnzkCV5vDF9tcvFZaYjx04yL"

WebServer Server;
AutoConnect Portal(Server);
AutoConnectConfig Config;
WiFiUDP UDP;
WakeOnLan WOL(UDP);

void rootPage() {
  char content[] = "El Jodas el Dosan el Dani el Xavos";
  Server.send(200, "text/plain", content);
}

void setup() {

  Serial.begin(115200);
  delay(1000);

  Config.autoReconnect = true;
  Config.retainPortal = true;

  Portal.config(Config);
  Server.on("/", rootPage);

  if (Portal.begin()) {
    Serial.println("WIFI Connected: " + WiFi.localIP().toString());
  }

  Firebase.begin(FIREBASE_Host, FIREBASE_key);                         // connect to firebase´

  Firebase.setString("PC 1: ", "OFF");                                          //Estado inicial para la salida1 (puede ser lo contrario)
  Firebase.setString("PC 2: ", "OFF");
  Firebase.setString("ResetWiFi", "false");
}

void loop() {

  String estado_led1 = "";                                        // Variable tipo string para el estado que mandará Firebase (puede ser cualquier tipo)                                       //Declaracion de GPIO
  String estado_led2 = "";
  String resetWiFi = "";
  String MACAdr = "00:18:8b:5b:2c:5f";
  Portal.handleClient();

  Serial.println("===================");
  Serial.println("INICIO DEL PROGRAMA");
  Serial.println("===================");

  estado_led1 = Firebase.getString("PC 1: ");
  estado_led2 = Firebase.getString("PC 2: ");
  resetWiFi = Firebase.getString("ResetWiFi");

  ////////////DEBUG
  Serial.println("La variable resetWiFI contiene: " + resetWiFi);

  if (resetWiFi == "true") {
    Serial.println("Reseteando conexión WiFi");
    if (Portal.begin()) {
      Serial.println("WIFI Connected: " + WiFi.localIP().toString());
      Firebase.setString("ResetWiFi", "false");
    }
  }
  //WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());



  if (estado_led1 == "ON") {

    Serial.println("Led1 esta ON");

  }

  else if (estado_led1 == "OFF") {

    Serial.println("Led1 esta OFF");
  }

  else {
    Serial.println("El valor que estas poniendo en Firebase es incorrecto (ON/OFF)");

  }

  //--------------LED 2----------------

  if (estado_led2 == "ON") {

    Serial.println("Led2 esta ON");

  }

  else if (estado_led2 == "OFF") {

    Serial.println("Led2 esta OFF");

  }

  else {
    Serial.println("El valor que estas poniendo en Firebase es incorrecto (ON/OFF) LED2");
  }

  Serial.println();
  ///EL MAji Paque
  Serial.println("Enviando paquete mágico a la MAC: " + MACAdr);
  WOL.sendMagicPacket(MACAdr);

}
