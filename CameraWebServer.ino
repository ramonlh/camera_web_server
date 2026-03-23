
#include "esp_camera.h"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Basado en ESP32-CAM AI Thinker
// Modo normal: STA fija hacia el rover
// Modo rescate: AP propio solo si falla la conexión al rover
// Esta version añade trazas de diagnostico para ver claramente
// el modo WiFi activo y en qué punto se abren los servidores HTTP.

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// --- Red fija del sistema rover + cámara ---
char wifi_ssid[33] = "ROVER_DIEGO";
char wifi_pass[65] = "12341234";

// AP de rescate de la cámara (subred distinta a la del rover)
static const char* RESCUE_AP_SSID = "CAMARA_RESCATE";
static const char* RESCUE_AP_PASS = "12341234";

// STA fija: la cámara siempre será 192.168.4.2 en la red del rover
IPAddress sta_local_IP(192, 168, 4, 20);
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

static const char* wifiModeToStr(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_OFF: return "WIFI_OFF";
    case WIFI_STA: return "WIFI_STA";
    case WIFI_AP: return "WIFI_AP";
    case WIFI_AP_STA: return "WIFI_AP_STA";
    default: return "WIFI_MODE?";
  }
}

static const char* wlStatusToStr(wl_status_t st) {
  switch (st) {
    case WL_NO_SHIELD: return "WL_NO_SHIELD";
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
    default: return "WL_UNKNOWN";
  }
}

void logWiFiSnapshot(const char* tag) {
  Serial.println();
  Serial.printf("[WIFI] %s\n", tag);
  Serial.printf("[WIFI] mode=%s  status=%s (%d)\n",
                wifiModeToStr(WiFi.getMode()),
                wlStatusToStr(WiFi.status()),
                (int)WiFi.status());
  Serial.printf("[WIFI] sta_ok=%d  ap_ok=%d\n", wifi_sta_ok ? 1 : 0, wifi_ap_ok ? 1 : 0);
  Serial.printf("[WIFI] STA localIP=%s  gateway=%s\n",
                WiFi.localIP().toString().c_str(),
                WiFi.gatewayIP().toString().c_str());
  Serial.printf("[WIFI] AP  softAPIP=%s  estaciones=%d\n",
                WiFi.softAPIP().toString().c_str(),
                WiFi.softAPgetStationNum());
}

void saveWiFiConfig(const String& s, const String& p) {
  Serial.println("La WiFi de la camara es fija y no admite cambios remotos.");
  Serial.print("SSID solicitado e ignorado: ");
  Serial.println(s);
}

bool connectToRoverWiFi(uint32_t timeout_ms = 12000) {
  Serial.println();
  Serial.println("[STA] Conectando al rover por WiFi STA...");
  Serial.print("[STA] SSID objetivo: ");
  Serial.println(wifi_ssid);
  logWiFiSnapshot("antes de preparar STA");

  wifi_sta_ok = false;

  WiFi.persistent(false);
  delay(100);

  // Mantener el AP si ya está activo para no tumbar el acceso web
  WiFi.mode(wifi_ap_ok ? WIFI_AP_STA : WIFI_STA);
  delay(300);
  Serial.printf("[STA] modo tras WiFi.mode(): %s\n", wifiModeToStr(WiFi.getMode()));

  WiFi.setSleep(false);
  delay(100);

  if (!WiFi.config(sta_local_IP, sta_gateway, sta_subnet,
                   sta_primaryDNS, sta_secondaryDNS)) {
    Serial.println("[STA] No se pudo configurar la IP fija STA");
  } else {
    Serial.printf("[STA] IP fija solicitada: %s  GW: %s\n",
                  sta_local_IP.toString().c_str(),
                  sta_gateway.toString().c_str());
  }

  WiFi.begin(wifi_ssid, wifi_pass);
  Serial.println("[STA] WiFi.begin() lanzado");

  uint32_t t0 = millis();
  wl_status_t last = WL_IDLE_STATUS;

  while ((millis() - t0) < timeout_ms) {
    wl_status_t st = WiFi.status();
    if (st != last) {
      Serial.printf("[STA] Estado WiFi -> %s (%d)\n", wlStatusToStr(st), (int)st);
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
    Serial.println("[STA] WiFi STA conectado al rover");
    Serial.print("[STA] SSID asociado: ");
    Serial.println(WiFi.SSID());
    Serial.print("[STA] BSSID asociado: ");
    Serial.println(WiFi.BSSIDstr());
    Serial.print("[STA] IP STA: ");
    Serial.println(WiFi.localIP());
    Serial.print("[STA] Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("[STA] RSSI: ");
    Serial.println(WiFi.RSSI());
    logWiFiSnapshot("despues de conectar STA");
    return true;
  }

  Serial.println("[STA] No se pudo conectar al rover por STA");
  logWiFiSnapshot("fallo de STA");
  return false;
}

bool startRescueAP() {
  Serial.println();
  Serial.println("[AP] Activando AP de rescate...");
  logWiFiSnapshot("antes de arrancar AP");

  wifi_ap_ok = false;

  // Si el STA sigue vivo, mantener ambos modos
  WiFi.mode(wifi_sta_ok ? WIFI_AP_STA : WIFI_AP);
  delay(300);
  Serial.printf("[AP] modo tras WiFi.mode(): %s\n", wifiModeToStr(WiFi.getMode()));

  if (!WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet)) {
    Serial.println("[AP] No se pudo configurar la IP del AP de rescate");
  } else {
    Serial.printf("[AP] IP AP configurada: %s\n", ap_local_IP.toString().c_str());
  }

  if (WiFi.softAP(RESCUE_AP_SSID, RESCUE_AP_PASS)) {
    wifi_ap_ok = true;
    Serial.println("[AP] AP de rescate activo");
    Serial.print("[AP] SSID AP: ");
    Serial.println(RESCUE_AP_SSID);
    Serial.print("[AP] IP AP: ");
    Serial.println(WiFi.softAPIP());
    logWiFiSnapshot("despues de arrancar AP");
    return true;
  }

  Serial.println("[AP] No se pudo arrancar el AP de rescate");
  logWiFiSnapshot("fallo de AP");
  return false;
}

void stopRescueAP() {
  if (!wifi_ap_ok) return;

  Serial.println("[AP] Desactivando AP de rescate...");
  WiFi.softAPdisconnect(true);
  delay(300);
  wifi_ap_ok = false;
  logWiFiSnapshot("despues de desactivar AP");
}

void setup() {
  // Dejamos el brownout activo para detectar problemas reales.
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(500);

  Serial.println();
  Serial.println("================ INICIO CAMARA ================");
  Serial.println("Iniciando camara...");
  logWiFiSnapshot("estado inicial");

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
    Serial.printf("[CAM] Camera init failed with error 0x%x\n", err);
    return;
  }
  Serial.println("[CAM] Camera OK");

  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    Serial.println("[CAM] Error obteniendo sensor");
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

  logWiFiSnapshot("antes de startCameraServer()");
  Serial.println("[HTTP] Llamando a startCameraServer()...");
  Serial.println("[HTTP] Si todo va bien, deben aparecer mensajes de apertura en 80 y 81.");
  startCameraServer();
  Serial.println("[HTTP] startCameraServer() ha retornado");
  logWiFiSnapshot("despues de startCameraServer()");

  if (wifi_sta_ok) {
    Serial.print("[HTTP] Camara lista por STA en http://");
    Serial.println(WiFi.localIP());
    Serial.print("[HTTP] Stream esperado en http://");
    Serial.print(WiFi.localIP());
    Serial.println(":81/stream");
  } else if (wifi_ap_ok) {
    Serial.print("[HTTP] Camara en modo rescate en http://");
    Serial.println(WiFi.softAPIP());
    Serial.print("[HTTP] Stream esperado en http://");
    Serial.print(WiFi.softAPIP());
    Serial.println(":81/stream");
  } else {
    Serial.println("[HTTP] Camara iniciada, pero sin conectividad WiFi");
  }

  lastReconnectAttempt = millis();
  Serial.println("================ FIN SETUP CAMARA ================");
}

void loop() {
  // Si la cámara estaba en rescate, intenta volver al rover periódicamente.
  // Importante: NO apagar el AP antes del intento, para no tumbar la web.
  if (!wifi_sta_ok && (millis() - lastReconnectAttempt) >= RECONNECT_INTERVAL_MS) {
    lastReconnectAttempt = millis();

    Serial.println();
    Serial.println("[LOOP] Reintentando conectar al rover...");
    logWiFiSnapshot("antes de reintento STA");

    if (connectToRoverWiFi(8000)) {
      Serial.print("[LOOP] Reconectada al rover. IP STA: ");
      Serial.println(WiFi.localIP());

      if (wifi_ap_ok) {
        Serial.println("[LOOP] STA recuperada; ahora si se apaga el AP de rescate.");
        stopRescueAP();
      }
    } else {
      Serial.println("[LOOP] STA sigue caida; se mantiene o reactiva el AP de rescate.");
      if (!wifi_ap_ok) {
        startRescueAP();
      } else {
        logWiFiSnapshot("AP mantenido durante fallo STA");
      }
    }
  }

  if (wifi_sta_ok && WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println("[LOOP] Se perdio la conexion STA con el rover");
    wifi_sta_ok = false;
    lastReconnectAttempt = 0;
    logWiFiSnapshot("STA perdida");
  }

  delay(50);
}
