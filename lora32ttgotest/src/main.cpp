#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiManager.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

/*
* WiFi + Firebase definition
*/
#define TRIGGER_PIN 0
#define WIFI_SSID "Pierre"
#define WIFI_PASSWORD "201250cp"
#define API_KEY "AIzaSyBHZ74aD10sHGBqkXuLpUW8HfpLAuDj9jU"
#define DATABASE_URL "https://esp32-rtdb2-default-rtdb.firebaseio.com/"
#define USER_EMAIL "chris.pierre343@gmail.com"
#define USER_PASSWORD "123456"
DefaultNetwork network;
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);

WiFiClientSecure ssl_client;
uint64_t chipId = ESP.getEfuseMac();
String folderName = "DAT_" + String(chipId, HEX);

using AsyncClient = AsyncClientClass;
FirebaseApp app; // Objeto FirebaseApp
RealtimeDatabase Database; // Objeto RealtimeDatabase
AsyncClient aClient(ssl_client, getNetwork(network));
void asyncCB(AsyncResult &aResult);
unsigned long sendDataPrevMillis = 0;

/* Definicoes para comunicação com radio LoRa */
#define SCK_LORA           5
#define MISO_LORA          19
#define MOSI_LORA          27
#define RESET_PIN_LORA     14
#define SS_PIN_LORA        18
#define HIGH_GAIN_LORA     20  /* dBm */
#define BAND               915E6  /* 915MHz de frequencia */

/* Definicoes do OLED */
#define OLED_SDA_PIN    21
#define OLED_SCL_PIN    22
#define SCREEN_WIDTH    128 
#define SCREEN_HEIGHT   64  
#define OLED_ADDR       0x3C 
#define OLED_RESET      -1

long informacao_recebida = 0;
/* Offset de linhas no display OLED */
#define OLED_LINE1     0
#define OLED_LINE2     10
#define OLED_LINE3     20
#define OLED_LINE4     30
#define OLED_LINE5     40
#define OLED_LINE6     50

#define DEBUG_SERIAL_BAUDRATE    115200
#define pinBT 2
bool estado;
bool estadoanterior = estado;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;  // delay de 50 ms para debounce

//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// void display_init(void);
// bool init_comunicacao_lora(void);
void firebaseSendData();
void printResult(AsyncResult &aResult);

// void display_init(void) {
//     if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
//         Serial.println("[LoRa Receiver] Falha ao inicializar comunicacao com OLED");        
//     } else {
//         Serial.println("[LoRa Receiver] Comunicacao com OLED inicializada com sucesso");
//         display.clearDisplay();
//         display.setTextSize(1);
//         display.setTextColor(WHITE);
//     }
// }

// bool init_comunicacao_lora(void) {
//     bool status_init = false;
//     Serial.println("[LoRa Receiver] Tentando iniciar comunicacao com o radio LoRa...");
//     SPI.begin(SCK_LORA, MISO_LORA, MOSI_LORA, SS_PIN_LORA);
//     LoRa.setPins(SS_PIN_LORA, RESET_PIN_LORA, LORA_DEFAULT_DIO0_PIN);
    
//     if (!LoRa.begin(BAND)) {
//         Serial.println("[LoRa Receiver] Comunicacao com o radio LoRa falhou. Nova tentativa em 1 segundo...");        
//         delay(1000);
//         status_init = false;
//     } else {
//         LoRa.setTxPower(HIGH_GAIN_LORA); 
//         Serial.println("[LoRa Receiver] Comunicacao com o radio LoRa ok");
//         status_init = true;
//     }

//     return status_init;
// }

void setup() {
    // Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    // display_init();
    // display.clearDisplay();    
    // display.setCursor(0, OLED_LINE1);
    // display.print("Aguarde...");
    // display.display();
    pinMode(pinBT, INPUT);
    Serial.begin(DEBUG_SERIAL_BAUDRATE);
    while (!Serial);

    // while(init_comunicacao_lora() == false);

    //--------------------------------------WiFi------------------------------
    WiFi.mode(WIFI_STA);
    Serial.println("\nStarting");
    pinMode(TRIGGER_PIN, INPUT_PULLUP);
    WiFiManager wifiManager;
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    if (digitalRead(TRIGGER_PIN) == LOW) {
        Serial.println("Button pressed! Starting configuration portal...");
        wifiManager.resetSettings(); 
        delay(1000);
    }

    if (!wifiManager.autoConnect(folderName.c_str())) {
        Serial.println("Failed to connect and hit timeout. Restarting...");
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        ESP.restart();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    Serial.println("Connected to Wi-Fi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    //-----------------------------------------------------------------------

    ssl_client.setInsecure();

    initializeApp(aClient, app, getAuth(user_auth), asyncCB, "authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
}

void loop() {
    int leitura = digitalRead(pinBT);

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (leitura != estadoanterior) {
            estadoanterior = leitura;
            lastDebounceTime = millis();
            informacao_recebida = leitura;

            Serial.println(informacao_recebida);
            firebaseSendData();
            //delay(100);
        }
    }
    // char byte_recebido;
    // int packet_size = 0;
    // int lora_rssi = 0;
    // char *ptInformaraoRecebida = NULL;

    // packet_size = LoRa.parsePacket();
     
    // if (packet_size == sizeof(informacao_recebida)) {
    //     ptInformaraoRecebida = (char *)&informacao_recebida;  
    //     while (LoRa.available()) {
    //         byte_recebido = (char)LoRa.read();
    //         *ptInformaraoRecebida = byte_recebido;
    //         ptInformaraoRecebida++;
    //     }

        // lora_rssi = LoRa.packetRssi();
        // display.clearDisplay();   
        // display.setCursor(0, OLED_LINE1);
        // display.print("RSSI: ");
        // display.println(lora_rssi);
        // display.setCursor(0, OLED_LINE2);
        // display.print("Informacao: ");
        // display.setCursor(0, OLED_LINE3);
        // display.println(informacao_recebida);

        // display.display();      
}

String path = "/" + folderName;
String childName1 = "/Input/chaveboia";


void firebaseSendData() {
    app.loop();
    Database.loop();
    if (app.ready()) {
        sendDataPrevMillis = millis();

        JsonWriter writer;
        object_t json;

        writer.create(json, "Input/chaveboia", informacao_recebida);

        Database.set<object_t>(aClient, path, json, asyncCB, "pushJsonTask");
    }
}

void asyncCB(AsyncResult &aResult) {
    printResult(aResult);
}

void printResult(AsyncResult &aResult) {
    if (aResult.isEvent()) {
        Serial.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
    }

    if (aResult.isDebug()) {
        Serial.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError()) {
        Serial.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.available()) {
        if (aResult.to<RealtimeDatabaseResult>().name().length())
            Serial.printf("task: %s, name: %s\n", aResult.uid().c_str(), aResult.to<RealtimeDatabaseResult>().name().c_str());
        Serial.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
    }
}
