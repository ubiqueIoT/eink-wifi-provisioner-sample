#include "WiFiProv.h"
#include "WiFi.h"
#if __has_include("qrcode.h")
#include "qrcode.h"
#endif
#include "Adafruit_EPD.h"

#define EPD_DC 33
#define EPD_CS 15
#define EPD_BUSY -1  // can set to -1 to not use a pin (will wait a fixed delay)
#define SRAM_CS 32
#define EPD_RESET -1  // can set to -1 and share with microcontroller Reset!
#define EPD_SPI &SPI  // primary SPI

#define COLOR1 EPD_BLACK
#define COLOR2 EPD_RED

#define DISPLAY_HEIGHT 250
#define DISPLAY_WIDTH 122

const char *pop = "abcd1234";           // Proof of possession - otherwise called a PIN - string provided by the device, entered by the user in the phone app
const char *service_name = "PROV_123";  // Name of your device (the Espressif apps expects by default device name starting with "Prov_")
const char *service_key = NULL;         // Password used for SofAP method (NULL = no password needed)
bool clearProvisioningOnReset = true;

Adafruit_SSD1680 display(DISPLAY_HEIGHT, DISPLAY_WIDTH, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS,
                         EPD_BUSY, EPD_SPI);

void initializeDisplay() {
  display.begin();
  display.setRotation(3);
  display.clearBuffer();
}

void displayProvisioningScreen(esp_qrcode_handle_t qrcode) {
  display.clearBuffer();
  esp_qrcode_print_console(qrcode);
  int size = esp_qrcode_get_size(qrcode);

  int elementSize = 3;
  int padding = 10;
  int startingY = (DISPLAY_HEIGHT / 2) - ((size * elementSize) / 2);
  int startingX = (DISPLAY_WIDTH / 2) - ((size * elementSize) / 2);

  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      if (esp_qrcode_get_module(qrcode, x, y)) {
        display.fillRect((x * elementSize) + startingX,
                         (y * elementSize) + startingY,
                         elementSize,
                         elementSize,
                         EPD_BLACK);
      } else {
        display.fillRect((x * elementSize) + startingX,
                         (y * elementSize) + startingY,
                         elementSize,
                         elementSize,
                         EPD_WHITE);
      }
    }
  }

  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(EPD_BLACK);
  display.setTextWrap(true);
  display.print("Scan the QR code to connect this device to a WiFi network!");

  display.display();
}

// WARNING: systemProvisionerEvt is called from a separate FreeRTOS task (thread)!
void systemProvisionerEvt(arduino_event_t *sys_event) {
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("\nConnected IP address : ");
      Serial.println(IPAddress(sys_event->event_info.got_ip.ip_info.ip.addr));
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("\nDisconnected. Connecting to the AP again... ");
      break;
    case ARDUINO_EVENT_PROV_START:
      Serial.println("\nProvisioning started\nGive Credentials of your access point using smartphone app");
      break;
    case ARDUINO_EVENT_PROV_CRED_RECV:
      {
        Serial.println("\nReceived Wi-Fi credentials");
        Serial.print("\tSSID : ");
        Serial.println((const char *)sys_event->event_info.prov_cred_recv.ssid);
        Serial.print("\tPassword : ");
        Serial.println((char const *)sys_event->event_info.prov_cred_recv.password);
        break;
      }
    case ARDUINO_EVENT_PROV_CRED_FAIL:
      {
        Serial.println("\nProvisioning failed!\nPlease reset to factory and retry provisioning\n");
        if (sys_event->event_info.prov_fail_reason == WIFI_PROV_STA_AUTH_ERROR)
          Serial.println("\nWi-Fi AP password incorrect");
        else
          Serial.println("\nWi-Fi AP not found....Add API \" nvs_flash_erase() \" before beginProvision()");
        break;
      }
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      Serial.println("\nProvisioning Successful");
      break;
    case ARDUINO_EVENT_PROV_END:
      Serial.println("\nProvisioning Ends");
      break;
    default:
      Serial.print("\nUnhandled event: ");
      Serial.println(sys_event->event_id);
      break;
  }
}

void initializeWifiProvisioner() {
  WiFi.onEvent(systemProvisionerEvt);
  Serial.println("Begin Provisioning using Soft AP");

  WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP,
                          WIFI_PROV_SCHEME_HANDLER_NONE,
                          WIFI_PROV_SECURITY_1,
                          pop,
                          service_name,
                          service_key,
                          NULL,
                          clearProvisioningOnReset);  // Reset provisioning each time the device resets

  char payload[150] = { 0 };
  snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\""
                                     ",\"pop\":\"%s\",\"transport\":\"%s\"}",
           "v1", service_name, pop, "softap");
  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  cfg.display_func = displayProvisioningScreen;
  esp_qrcode_generate(&cfg, payload);
}

void setup() {
  Serial.begin(115200);
  initializeDisplay();
  initializeWifiProvisioner();
}

void loop() {
}
