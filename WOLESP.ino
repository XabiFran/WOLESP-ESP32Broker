//Version que permite recibir acciones mediante eventos, siendo el esp32 un listener, incorporada la funcion PING y ajustes
#define ESP_DRD_USE_SPIFFS true
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiManager.h>
#include <ESP_DoubleResetDetector.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <ESP32Ping.h>
#define JSON_CONFIG_FILE "/sample_config.json"

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#define FIREBASE_PROJECT_ID "wolespdb"
#define API_KEY "AIzaSyDk52kOpbvh9GUPoHi6eH51LKikC_nTt5g"
#define DATABASE_URL "wolespdb-default-rtdb.europe-west1.firebasedatabase.app"

//Firebase objects
FirebaseData stream;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
//WOL Objects
WiFiUDP UDP;
WakeOnLan WOL(UDP);

DoubleResetDetector *drd;

//flag for saving data
bool shouldSaveConfig = false;

char testString[50] = "default value";
char testString2[50] = "default value";
char testPass[50] = "default value";
bool mail_received = true;
bool pass_received = true;
int testNumber = 1500;
bool encendido = false;

unsigned long sendDataPrevMillis = 0;

int count = 0;

volatile bool dataChanged = false;

/**
*Función encargada de recibir y tratar las escrituras de la base de datos.
*@param data - Los datos que se han escrito.
*
*/
void streamCallback(FirebaseStream data)
{
  Serial.printf("stream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
                data.streamPath().c_str(),
                data.dataPath().c_str(),
                data.dataType().c_str(),
                data.eventType().c_str());
  printResult(data); // see addons/RTDBHelper.h
  if (data.dataTypeEnum() == fb_esp_rtdb_data_type_integer)
    Serial.println(data.to<int>());
  else if (data.dataTypeEnum() == fb_esp_rtdb_data_type_float)
    Serial.println(data.to<float>(), 5);
  else if (data.dataTypeEnum() == fb_esp_rtdb_data_type_double)
    printf("%.9lf\n", data.to<double>());
  else if (data.dataTypeEnum() == fb_esp_rtdb_data_type_boolean)
    Serial.println(data.to<bool>() ? "true" : "false");
  else if (data.dataTypeEnum() == fb_esp_rtdb_data_type_string)
    Serial.println(data.to<String>());
  else if (data.dataTypeEnum() == fb_esp_rtdb_data_type_json)
  {
    FirebaseJson *json = data.to<FirebaseJson *>();
    Serial.println(json->raw());
    if (data.payloadLength() <= 450) {
      StaticJsonDocument<450> doc;
      DeserializationError error = deserializeJson(doc, json->raw());
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }
      const char* PCIDGot = doc["PCID"];// "-NKBETolWMyhD1JcDHEV"
      String PCID = PCIDGot;
      const char* deviceIDGot = doc["deviceID"]; // "-NKAzwGEw1pd1EiZyElc"
      String deviceID = deviceIDGot;
      const char* actionGotChar = doc["action"]; // "encender"
      String actionGot = actionGotChar;
      const char* ipGot = doc["ip"]; // "IP DEL ORDENADOR 1"
      String StringIP = ipGot;
      const char* macGot = doc["mac"]; // "Con su mac"
      String StringMAC = macGot;
      String uid = auth.token.uid.c_str();
      String route = "/UsersData/" + uid + "/Devices/" + deviceID + "/PCs/" + PCID + "/on";
      String PCResultRoute = "/UsersData/" + uid + "/Devices/" + deviceID + "/PCs/" + PCID + "/result";
      String resultRoute = "/UsersData/" + uid + "/Devices/" + deviceID + "/result";
      Serial.print(F("ROUTE: "));
      Serial.println(route);
      Serial.print(F("Action: "));
      Serial.println(actionGot);
      Serial.print(F("IP: "));
      Serial.println(StringIP);
      Serial.print(F("MAC: "));
      Serial.println(macGot);
      bool foo;
      if (actionGot == "encender")
        encenderPC(StringIP, StringMAC, route, resultRoute, PCResultRoute);
      else if (actionGot == "ping")
        foo = pingearPC(StringIP, StringMAC, route, PCResultRoute);
      else if (actionGot == "apagar")
        Serial.print("ACCIÓN APAGAR EL PC\n");
      else
        Serial.print("No se entiende la acción\n");
    }
  }
  else if (data.dataTypeEnum() == fb_esp_rtdb_data_type_array)
  {
    FirebaseJsonArray *arr = data.to<FirebaseJsonArray *>();
    Serial.println(arr->raw());
  }
  Serial.printf("Received stream payload size: %d (Max. %d)\n\n", data.payloadLength(), data.maxPayloadLength());
  dataChanged = true;
}

/**
*Función encargada de encender un equipo, comprobar el encendido y escribir el resultado en la base de datos.
*@param ip - IP del ordenador.
*@param mac - MAC del ordenador.
*@param route - Ruta del estado del ordenador.
*@param resultRoute - Ruta del resultado de la acción.
*@param PCResultRoute - Ruta del resultado de la acción en el ordenador.
*
*/
void encenderPC(String ip, String mac, String route, String resultRoute, String PCResultRoute) {
  Serial.println(F("LLEGO A LA FUNCIÓN ENCENDER"));
  WOL.sendMagicPacket(mac);
  delay(5000);
  if (pingearPC(ip, mac, route, PCResultRoute)) {
    Serial.printf("Set bool... %s\n", Firebase.RTDB.setBool(&fbdo, route, "true") ? "ok" : fbdo.errorReason().c_str());
    Serial.printf("Set string... %s\n", Firebase.RTDB.setString(&fbdo, resultRoute, F("Vhatxosas")) ? "ok" : fbdo.errorReason().c_str());
    Serial.printf("Set string... %s\n", Firebase.RTDB.setString(&fbdo, resultRoute, F("turnedOn")) ? "ok" : fbdo.errorReason().c_str());
    //Serial.printf("Set string... %s\n", Firebase.RTDB.setString(&fbdo, PCResultRoute, F("turnedOn")) ? "ok" : fbdo.errorReason().c_str());
  } else {
    Serial.printf("Set bool false... %s\n", Firebase.RTDB.setBool(&fbdo, route, 0 == 1) ? "ok" : fbdo.errorReason().c_str());
    Serial.printf("Set string... %s\n", Firebase.RTDB.setString(&fbdo, resultRoute, F("Vhatxosas")) ? "ok" : fbdo.errorReason().c_str());
    Serial.printf("Set string... %s\n", Firebase.RTDB.setString(&fbdo, resultRoute, F("notTurnedOn")) ? "ok" : fbdo.errorReason().c_str());
    //Serial.printf("Set string... %s\n", Firebase.RTDB.setString(&fbdo, PCResultRoute, F("notTurnedOn")) ? "ok" : fbdo.errorReason().c_str());
  }
}

/**
*Función encargada de diagnosticar un equipo y escribir el resultado en la base de datos.
*@param ip - IP del ordenador.
*@param mac - MAC del ordenador.
*@param route - Ruta del estado del ordenador.
*@param PCResultRoute - Ruta del resultado de la acción en el ordenador.
*
*/
bool pingearPC(String ip, String mac, String route, String PCResultRoute) {
  Serial.println(F("LLEGO A LA FUNCIÓN PINGEAR"));
  Serial.println(route);
  IPAddress apip;
  const char* rawIP = ip.c_str();
  if (apip.fromString(rawIP)) { // Intentar parsear la IP
    Serial.print(F("PC IP: "));
    Serial.println(apip); // Imprimir la IP Parseada
  } else {
    Serial.println(F("Fallo parseando la IP"));
  }

  if (Ping.ping(apip)) {
    Serial.printf("Set bool true... %s\n", Firebase.RTDB.setBool(&fbdo, route, "true") ? "ok" : fbdo.errorReason().c_str());
    Serial.printf("Set string... %s\n", Firebase.RTDB.setString(&fbdo, PCResultRoute, F("turnedOn")) ? "ok" : fbdo.errorReason().c_str());
    return true;
  } else {
    Serial.printf("Set bool false... %s\n", Firebase.RTDB.setBool(&fbdo, route, 0 == 1) ? "ok" : fbdo.errorReason().c_str());
    Serial.printf("Set string... %s\n", Firebase.RTDB.setString(&fbdo, PCResultRoute, F("notTurnedOn")) ? "ok" : fbdo.errorReason().c_str());
    return false;
  }
}

/**
*Función encargada de controlar el timeout de la base de datos.
*@param timeout - Indica si el timeout ha sucedido o no.
*
*/
void streamTimeoutCallback(bool timeout)
{
  if (timeout)
    Serial.println("stream timed out, resuming...\n");

  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void printWelcome() {
  Serial.println(F("==================="));
  Serial.println(F("INICIO DEL PROGRAMA"));
  Serial.println(F("==================="));
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
}

void fcsUploadCallback(CFS_UploadStatusInfo info)
{
  if (info.status == fb_esp_cfs_upload_status_init)
  {
    Serial.printf("\nUploading data (%d)...\n", info.size);
  }
  else if (info.status == fb_esp_cfs_upload_status_upload)
  {
    Serial.printf("Uploaded %d%s\n", (int)info.progress, "%");
  }
  else if (info.status == fb_esp_cfs_upload_status_complete)
  {
    Serial.println(F("Upload completed "));
  }
  else if (info.status == fb_esp_cfs_upload_status_process_response)
  {
    Serial.print(F("Processing the response... "));
  }
  else if (info.status == fb_esp_cfs_upload_status_error)
  {
    Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
  }
}

/**
 *Función encargada de iniciar sesión en firebase con correo y contraseña y configurar los parámetros necesarios.
 *
 */
void configFirebase() {
  String mail = testString;
  String pass = testString2;
  Serial.println(F("==================="));
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = mail;
  auth.user.password = pass;
  Serial.printf("Email got: %s | Pass got: %s \n", mail.c_str(), pass.c_str());
  
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  Serial.println(F("Getting User UID"));
  while ((auth.token.uid) == "") {
    Serial.print(F('.'));
    delay(1000);
  }

  Serial.print(F("User UID: "));
  Serial.print(auth.token.uid.c_str());

  String uid = auth.token.uid.c_str();
  if (!Firebase.RTDB.beginStream(&stream, "/UsersData/" + uid + "/Devices"))
    Serial.printf("sream begin error, %s\n\n", stream.errorReason().c_str());

  Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
}

/** 
 *Función encargada de guardar el archivo de configuración en los SPIFFS.  
 */
void saveConfigFile()
{
  Serial.println(F("Saving config"));
  StaticJsonDocument<512> json;
  json["testString"] = testString;
  json["testString2"] = testString2;

  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    Serial.println("failed to open config file for writing");
  }

  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0)
  {
    Serial.println(F("Failed to write to file"));
  }
  configFile.close();
}

/** 
 *Función encargada de cargar el archivo de configuración guardado en los SPIFFS.  
 */
bool loadConfigFile()
{
  Serial.println("mounting FS...");

  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        Serial.println("opened config file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error)
        {
          Serial.println("\nparsed json");

          strcpy(testString, json["testString"]);
          strcpy(testString2, json["testString2"]);

          return true;
        }
        else
        {
          Serial.println("failed to load json config");
        }
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }
  return false;
}

void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered Conf Mode");

  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());

  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
}

String getCustomParamValue(WiFiManager *myWiFiManager, String name)
{
  String value;

  int numArgs = myWiFiManager->server->args();
  for (int i = 0; i < numArgs; i++) {
    Serial.println(myWiFiManager->server->arg(i));
  }
  if (myWiFiManager->server->hasArg(name))
  {
    value = myWiFiManager->server->arg(name);
  }
  return value;
}
/**
 *Función que se ejecuta al iniciar el ESP32, se encarga de llamar a todas las funciones de configuración inicial.
 *
 */

void setup()
{
  WOL.setRepeat(3, 100);
  bool forceConfig = false;

  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  if (drd->detectDoubleReset())
  {
    Serial.println(F("Forcing config mode as there was a Double reset detected"));
    forceConfig = true;
  }

  bool spiffsSetup = loadConfigFile();
  if (!spiffsSetup)
  {
    Serial.println(F("Forcing config mode as there is no saved config"));
    forceConfig = true;
  }

  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  delay(10);

  WiFiManager wm;

  wm.setSaveConfigCallback(saveConfigCallback);
  
  wm.setAPCallback(configModeCallback);

  WiFiManagerParameter custom_text_box("key_text", "Enter your e-mail here", testString, 50);

  WiFiManagerParameter custom_text_box2("key_text2", "Enter your password here", testString2, 50);

  wm.addParameter(&custom_text_box);
  wm.addParameter(&custom_text_box2);

  Serial.println("hello");

  if (forceConfig)
  {
    if (!wm.startConfigPortal("WOLESP_AP", "admin1234"))
    {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      ESP.restart();
      delay(5000);
    }
  }
  else
  {
    if (!wm.autoConnect("WOLESP_AP", "admin1234"))
    {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    }
  }

  WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  printWelcome();

  if (shouldSaveConfig)
  {
    strncpy(testString, custom_text_box.getValue(), sizeof(testString));
    Serial.print("testString: ");
    Serial.println(testString);

    strncpy(testString2, custom_text_box2.getValue(), sizeof(testString2));
    Serial.print("testString2: ");
    Serial.println(testString2);

    saveConfigFile();
  }
  configFirebase();
}
/**
 *Función que se repetirá infinitamente para mantener la autenticación de firebase y la función de reset. 
 * 
 */
void loop()
{
  drd->loop();
  String uid = auth.token.uid.c_str();
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();
    count++;
    //FirebaseJson json;
    //json.add("data", "hello");
    //json.add("num", count);
    //Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo, "/test/stream/data/json", &json) ? "ok" : fbdo.errorReason().c_str());
  }
}
