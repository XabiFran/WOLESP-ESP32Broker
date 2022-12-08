//Version que permite "encender" ordenadores mediante la variable TURNON
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
#define JSON_CONFIG_FILE "/sample_config.json"

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#define FIREBASE_PROJECT_ID "wolespdb"
#define API_KEY "AIzaSyDk52kOpbvh9GUPoHi6eH51LKikC_nTt5g"

//Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
//WOL Objects
WiFiUDP UDP;
WakeOnLan WOL(UDP);

DoubleResetDetector *drd;

//flag for saving data
bool shouldSaveConfig = false;

char testString[50] = "deafult value";
char testString2[50] = "deafult value";
char testPass[50] = "deafult value";
bool mail_received = true;
bool pass_received = true;
int testNumber = 1500;
bool encendido = false;

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


void encender(String ruta, String PCID) {
  String documentPath = ruta + "/" + PCID;
  FirebaseJson content;

  content.set("fields/on/booleanValue", true);
  Serial.print("Ruta: ");
  Serial.println(documentPath.c_str());
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "on")) {
    //Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
    return;
  } else {
    Serial.println(fbdo.errorReason());
  }
}
void apagar(String ruta, String PCID) {
  String documentPath = ruta + "/" + PCID;
  FirebaseJson content;

  content.set("fields/on/booleanValue", false);
  Serial.print("Ruta: ");
  Serial.println(documentPath.c_str());

  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "on")) {
    Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
    return;
  } else {
    Serial.println(fbdo.errorReason());
  }
}

void configFirebase() {
  String mail = testString;
  String pass = testString2;
  Serial.println(F("==================="));
  config.api_key = API_KEY;
  auth.user.email = mail;
  auth.user.password = pass;
  Serial.printf("Email got: %s | Pass got: %s \n", mail.c_str(), pass.c_str());
  //Serial.println("Email coded: " + auth.user.email + " Pass coded: " + auth.user.password);
  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h
  // Limit the size of response payload to be collected in FirebaseData
  fbdo.setResponseSize(2048);
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  // For sending payload callback
  config.cfs.upload_callback = fcsUploadCallback;
  Serial.println(F("Getting User UID"));
  while ((auth.token.uid) == "") {
    Serial.print(F('.'));
    delay(1000);
  }

  Serial.print(F("User UID: "));
  Serial.print(auth.token.uid.c_str());
}

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

bool loadConfigFile()
{
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  // May need to make it begin(true) first time you are using SPIFFS
  // NOTE: This might not be a good way to do this! begin(true) reformats the spiffs
  // it will only get called if it fails to mount, which probably means it needs to be
  // formatted, but maybe dont use this if you have something important saved on spiffs
  // that can't be replaced.
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
  //end read
  return false;
}

//callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// This gets called when the config mode is launced, might
// be useful to update a display with this info.
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered Conf Mode");

  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());

  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
}

// Custom HTML WiFiManagerParameter don't support getValue directly
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

void getPCData(String ruta, String PCID) {
  StaticJsonDocument<768> pcJson;
  DeserializationError desErr;
  String documentPath = ruta + "/" + PCID;
  if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), "")) {
    //Serial.printf("PC Extraído:\n%s\n\n", fbdo.payload().c_str());
    desErr = deserializeJson(pcJson, fbdo.payload().c_str());
    if (desErr) {
      Serial.print(F("Deserialization error: "));
      Serial.println(desErr.c_str());
    }
    JsonObject fields = pcJson["fields"];
    const char* rawMac = fields["mac"]["stringValue"];
    bool turnOn = fields["turnOn"]["booleanValue"];
    String PCMac = rawMac;
    Serial.print(F("La MAC de este PC es: "));
    Serial.println(PCMac);
    if (turnOn) {
      Serial.print(F("Turning on..."));
      Serial.println(PCMac);
      if (WOL.sendMagicPacket(PCMac) == true)
        Serial.print(F("Pues devuelve verdadero"));
      else
        Serial.print(F("Pues parece que devuelve falso"));
      FirebaseJson content;
      //Si consigue encenderlo
      content.set("fields/turnOn/booleanValue", false);
      content.set("fields/on/booleanValue", true);
      if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "turnOn,on")) {
        Serial.print(F("Encendido!"));
      } else {
        Serial.println(fbdo.errorReason());
      }
    }
  } else
    Serial.println(fbdo.errorReason());
}

void pingPCFunctions(){
  
  }

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

  //WiFi.disconnect();
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  Serial.begin(115200);
  delay(10);

  // wm.resetSettings(); // wipe settings

  WiFiManager wm;

  //wm.resetSettings(); // wipe settings
  //set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);

  //--- additional Configs params ---

  // Text box (String)
  WiFiManagerParameter custom_text_box("key_text", "Enter your e-mail here", testString, 50); // 50 == max length

  WiFiManagerParameter custom_text_box2("key_text2", "Enter your password here", testString2, 50);


  //add all your parameters here
  wm.addParameter(&custom_text_box);
  wm.addParameter(&custom_text_box2);

  Serial.println("hello");

  if (forceConfig)
  {
    if (!wm.startConfigPortal("WOLESP_AP", "clock123"))
    {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }
  }
  else
  {
    if (!wm.autoConnect("WOLESP_AP", "clock123"))
    {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    }
  }

  // If we get here, we are connected to the WiFi
  WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //save the custom parameters to FS
  printWelcome();

  if (shouldSaveConfig)
  {
    // Lets deal with the user config values

    // Copy the string value
    strncpy(testString, custom_text_box.getValue(), sizeof(testString));
    Serial.print("testString: ");
    Serial.println(testString);

    // Copy the string value
    strncpy(testString2, custom_text_box2.getValue(), sizeof(testString2));
    Serial.print("testString2: ");
    Serial.println(testString2);

    saveConfigFile();
  }
}

void loop()
{
  drd->loop();
  String uid = auth.token.uid.c_str();
  pingPCFunctions();
  if (mail_received && pass_received) {
    configFirebase();
    if (Firebase.ready())
    {
      Serial.println("Estamos dentro.");
      String documentPath = "Users/" + uid + "/PCs";
      //documentPath = documentPath + "/PCs";

      String PCID = "eIyzIi5z3FEPORPpRjUj";
      encendido = !encendido;
      const char* nextPageToken = "";
      const char* desPCRoute = "";
      String PCName = "";
      StaticJsonDocument<2048> pcJson;
      DeserializationError desErr;
      do {
        if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "" /* databaseId can be (default) or empty */, documentPath.c_str(), 1 /* The maximum number of documents to return */, nextPageToken /* The nextPageToken value returned from a previous List request, if any. */, "" /* The order to sort results by. For example: priority desc, name. */, "count" /* the field name to mask */, false /* showMissing, iIf the list should show missing documents. A missing document is a document that does not exist but has sub-documents. */)) {
          //Serial.printf("ok %i\n%s\n\n", i, fbdo.payload().c_str());

          desErr = deserializeJson(pcJson, fbdo.payload().c_str());
          if (desErr) {
            Serial.print(F("Deserialization error: "));
            Serial.println(desErr.c_str());
          }

          desPCRoute = pcJson["documents"][0]["name"];
          PCName = desPCRoute;
          PCName.remove(0, 87);
          if (PCName != "")
            getPCData(documentPath, PCName);
          nextPageToken = pcJson["nextPageToken"];
          //Serial.print(F("Contenido de PCName: " ));
          //Serial.println(PCName);
          //Serial.print(F("Contenido de nextPageToken: " ));
          //Serial.println(nextPageToken);
        } else {
          Serial.println(fbdo.errorReason());
        }
        if (nextPageToken != NULL) {
          if (*nextPageToken == '\0') {
            //Serial.print(F("JODER EL TOKEN ESTÁ '\0' \n"));
            break;
          }
        }
      } while (nextPageToken != NULL);
      /*if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), ""))
        Serial.printf("Pues la coleccion es\n%s\n\n", fbdo.payload().c_str());
        else
        Serial.println(fbdo.errorReason());
        intento de deserializacion de toda la lista de PCs
          StaticJsonDocument<2048> pcJson;

        DeserializationError desErr = deserializeJson(pcJson, fbdo.payload().c_str());
        if (desErr) {
        Serial.print(F("Deserialization error: "));
        Serial.println(desErr.c_str());
        }

        const char* desPCList = pcJson["documents"];

        Serial.print(F("Tamaño de desPCList: "));
        Serial.println(sizeof desPCList / sizeof desPCList[0]);*/

      /*FirebaseJson content;

        content.set("fields/on/booleanValue", false);

        if(Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "on")){
        Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
        return;
        }else{
        Serial.println(fbdo.errorReason());
        }

        if(Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw())){
        Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
        return;
        }else{
        Serial.println(fbdo.errorReason());
        }*/
      if (encendido) {
        encender(documentPath, PCID);
      } else {
        apagar(documentPath, PCID);
      }
    }
    delay(10000);

  }
}
