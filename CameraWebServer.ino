#include "esp_camera.h"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Basado en ESP32-CAM AI Thinker
// Modo normal: STA fija hacia el rover
// Modo rescate: AP propio solo si falla la conexión al rover
// No se usa WIFI_AP_STA para evitar picos innecesarios al arrancar.

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// --- Red fija del sistema rover + cámara ---
char wifi_ssid[33] = "ROVER_DIEGO";
char wifi_pass[65] = "12341234";

// AP de rescate de la cámara (subred distinta a la del rover)
static const char* RESCUE_AP_SSID = "CAMARA_RESCATE";
static const char* RESCUE_AP_PASS = "12341234";

// STA fija: la cámara siempre será 192.168.4.2 en la red del rover
IPAddress sta_local_IP(192, 168, 4, 2);
IPAddress sta_gateway(192, 168, 4, 1);
IPAddress sta_subnet(255, 255, 255, 0);
IPAddress sta_primaryDNS(8, 8, 8, 8);
IPAddress sta_secondaryDNS(8, 8, 4, 4);

// AP de rescate: otra subred para no crear ambigüedad
IPAddress ap_local_IP(192, 168, 8, 1);
IPAddress ap_gateway(192, 168, 8, 1);
IPAddress ap_subnet(255, 255, 255, 0);

bool wifi_sta_ok = false;
bool wifi_ap_ok = false;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL_MS = 10000;

void startCameraServer();
void setupLedFlash(int pin);

void saveWiFiConfig(const String& s, const String& p) {
  Serial.println("La WiFi de la camara es fija y no admite cambios remotos.");
  Serial.print("SSID solicitado e ignorado: ");
  Serial.println(s);
}

bool connectToRoverWiFi(uint32_t timeout_ms = 12000) {
  Serial.println();
  Serial.println("Conectando al rover por WiFi STA...");
  Serial.print("SSID objetivo: ");
  Serial.println(wifi_ssid);

  wifi_sta_ok = false;

  WiFi.persistent(false);
  delay(200);

  WiFi.disconnect(true, true);
  delay(600);

  WiFi.mode(WIFI_OFF);
  delay(1000);

  WiFi.mode(WIFI_STA);
  delay(800);

  WiFi.setSleep(false);
  delay(200);

  if (!WiFi.config(sta_local_IP, sta_gateway, sta_subnet,
                   sta_primaryDNS, sta_secondaryDNS)) {
    Serial.println("No se pudo configurar la IP fija STA");
  }

  WiFi.begin(wifi_ssid, wifi_pass);

  uint32_t t0 = millis();
  wl_status_t last = WL_IDLE_STATUS;

  while ((millis() - t0) < timeout_ms) {
    wl_status_t st = WiFi.status();
    if (st != last) {
      Serial.printf("Estado WiFi -> %d\n", (int)st);
      last = st;
    }
    if (st == WL_CONNECTED) {
      break;
    }
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifi_sta_ok = true;
    Serial.println("WiFi STA conectado al rover");
    Serial.print("SSID asociado: ");
    Serial.println(WiFi.SSID());
    Serial.print("BSSID asociado: ");
    Serial.println(WiFi.BSSIDstr());
    Serial.print("IP STA: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    return true;
  }

  Serial.println("No se pudo conectar al rover por STA");
  return false;
}

bool startRescueAP() {
  Serial.println();
  Serial.println("Activando AP de rescate...");

  wifi_ap_ok = false;

  WiFi.disconnect(true, true);
  delay(600);

  WiFi.mode(WIFI_OFF);
  delay(1000);

  WiFi.mode(WIFI_AP);
  delay(800);

  if (!WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet)) {
    Serial.println("No se pudo configurar la IP del AP de rescate");
  }

  if (WiFi.softAP(RESCUE_AP_SSID, RESCUE_AP_PASS)) {
    wifi_ap_ok = true;
    Serial.println("AP de rescate activo");
    Serial.print("SSID AP: ");
    Serial.println(RESCUE_AP_SSID);
    Serial.print("IP AP: ");
    Serial.println(WiFi.softAPIP());
    return true;
  }

  Serial.println("No se pudo arrancar el AP de rescate");
  return false;
}

void stopRescueAP() {
  if (!wifi_ap_ok) return;

  Serial.println("Desactivando AP de rescate...");
  WiFi.softAPdisconnect(true);
  delay(300);
  wifi_ap_ok = false;
}

void setup() {
  // Dejamos el brownout activo para detectar problemas reales.
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(500);

  Serial.println();
  Serial.println("Iniciando camara...");

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
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }
  Serial.println("Camera OK");

  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    Serial.println("Error obteniendo sensor");
    return;
  }

  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

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

#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  bool sta_ok = connectToRoverWiFi(15000);
  if (!sta_ok) {
    startRescueAP();
  }

  startCameraServer();

  if (wifi_sta_ok) {
    Serial.print("Camara lista por STA en http://");
    Serial.println(WiFi.localIP());
  } else if (wifi_ap_ok) {
    Serial.print("Camara en modo rescate en http://");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Camara iniciada, pero sin conectividad WiFi");
  }

  lastReconnectAttempt = millis();
}

void loop() {
  // Si la cámara estaba en rescate, intenta volver al rover periódicamente.
  if (!wifi_sta_ok && (millis() - lastReconnectAttempt) >= RECONNECT_INTERVAL_MS) {
    lastReconnectAttempt = millis();

    Serial.println();
    Serial.println("Reintentando conectar al rover...");

    bool was_ap = wifi_ap_ok;
    if (was_ap) {
      stopRescueAP();
      delay(300);
    }

    if (connectToRoverWiFi(8000)) {
      Serial.print("Reconectada al rover. IP STA: ");
      Serial.println(WiFi.localIP());
      wifi_ap_ok = false;
    } else {
      if (was_ap || !wifi_ap_ok) {
        startRescueAP();
      }
    }
  }

  if (wifi_sta_ok && WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println("Se perdio la conexion STA con el rover");
    wifi_sta_ok = false;
    lastReconnectAttempt = 0;
  }

  delay(50);
}