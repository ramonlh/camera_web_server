
#include <Preferences.h>
#include "esp_camera.h"
#include <WiFi.h>

// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"

Preferences prefs;

char wifi_ssid[33] = "ROVER_DIEGO";
char wifi_pass[65] = "12341234";

char ap_ssid[33] = "CAMARA_ROVER";
char ap_pass[65] = "12341234";

IPAddress sta_local_IP(192, 168, 4, 2);
IPAddress sta_gateway(192, 168, 4, 1);
IPAddress sta_subnet(255, 255, 255, 0);
IPAddress sta_primaryDNS(8, 8, 8, 8);
IPAddress sta_secondaryDNS(8, 8, 4, 4);

IPAddress ap_local_IP(192, 168, 4, 1);
IPAddress ap_gateway(192, 168, 4, 1);
IPAddress ap_subnet(255, 255, 255, 0);

bool wifi_sta_ok = false;
bool wifi_ap_ok = false;

void saveWiFiConfig(const String& s, const String& p);

IPAddress local_IP(192, 168, 4, 2); // IP deseada (ajústala según tu red)
IPAddress gateway(192, 168, 4, 1);    // Gateway (normalmente el router)
IPAddress subnet(255, 255, 255, 0);   // Máscara de subred
IPAddress primaryDNS(8, 8, 8, 8);     // DNS primario (opcional)
IPAddress secondaryDNS(8, 8, 4, 4);   // DNS secundario (opcional)

void startCameraServer();
void setupLedFlash(int pin);

void loadWiFiConfig() {
  prefs.begin("camcfg", true);
  String s = prefs.getString("ssid", "ROVER_DIEGO");
  String p = prefs.getString("pass", "12341234");
  prefs.end();

  strlcpy(wifi_ssid, s.c_str(), sizeof(wifi_ssid));
  strlcpy(wifi_pass, p.c_str(), sizeof(wifi_pass));
}

void saveWiFiConfig(const String& s, const String& p) {
  prefs.begin("camcfg", false);
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  prefs.end();

  strlcpy(wifi_ssid, s.c_str(), sizeof(wifi_ssid));
  strlcpy(wifi_pass, p.c_str(), sizeof(wifi_pass));
}

bool connectToSavedWiFi(uint32_t timeout_ms = 12000) {
  Serial.println();
  Serial.println("=== WIFI STA ===");
  Serial.print("SSID guardado: ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(300);

  if (!WiFi.config(sta_local_IP, sta_gateway, sta_subnet, sta_primaryDNS, sta_secondaryDNS)) {
    Serial.println("No se pudo configurar IP estática STA");
  }

  WiFi.begin(wifi_ssid, wifi_pass);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeout_ms) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi STA conectado");
    Serial.print("IP STA: ");
    Serial.println(WiFi.localIP());
    wifi_sta_ok = true;
    return true;
  }

  Serial.println("No se pudo conectar en modo STA");
  wifi_sta_ok = false;
  return false;
}

void startRescueAP() {
  Serial.println();
  Serial.println("=== WIFI AP RESCATE ===");

  WiFi.mode(WIFI_AP_STA);

  if (!WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet)) {
    Serial.println("No se pudo configurar IP del AP");
  }

  bool ok = WiFi.softAP(ap_ssid, ap_pass);

  if (ok) {
    Serial.println("AP de rescate activo");
    Serial.print("SSID AP: ");
    Serial.println(ap_ssid);
    Serial.print("IP AP: ");
    Serial.println(WiFi.softAPIP());
    wifi_ap_ok = true;
  } else {
    Serial.println("No se pudo arrancar el AP de rescate");
    wifi_ap_ok = false;
  }
}

void startWiFiWithFallback() {
  bool connected = connectToSavedWiFi();

  if (!connected) {
    startRescueAP();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("Inicio");
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
//  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_VGA;
//  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
    }
  else
    Serial.println("Camera OK");

  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    Serial.println("Error obteniendo sensor");
    return;
  }
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_VGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

  // Setup LED FLash if LED pin is defined in camera_pins.h
  #if defined(LED_GPIO_NUM)
    setupLedFlash(LED_GPIO_NUM);
  #endif
    // Configurar IP estática antes de conectar
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Error al configurar IP estática");
  }

  WiFi.setSleep(false);

  loadWiFiConfig();
  startWiFiWithFallback();

  startCameraServer();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nNo se pudo conectar al WiFi");
    return;
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());



  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void loop() {
  // Do nothing. Everything is done in another task by the web server
  delay(1);
}
