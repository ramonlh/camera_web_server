
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

bool wm_nonblocking = false; // change to true to use non blocking

WiFiManager wm; // global wm instance
WiFiManagerParameter custom_field; // global param ( for non blocking w params )

String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback(){
  Serial.println("[CALLBACK] saveParamCallback lanzado");
  Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
}

void checkButton(){
  // check for button press
  if ( digitalRead(WIFI_TRIGGER_PIN) == LOW ) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if( digitalRead(WIFI_TRIGGER_PIN) == LOW ){
      Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if( digitalRead(WIFI_TRIGGER_PIN) == LOW ){
        Serial.println("Boton presionado");
        Serial.println("Borrando configuración, reiniciando");
        wm.resetSettings();
        ESP.restart();
      }
      
      // start portal w delay
      Serial.println("Iniciando portal configuración");
      wm.setConfigPortalTimeout(120);
      
      if (!wm.startConfigPortal("OnDemandAP","password")) {
        Serial.println("No se pudo conectar");
        delay(3000);
        // ESP.restart();
      } else {
        //if you get here you have connected to the WiFi
        Serial.println("conectado...Bien :)");
      }
    }
  }
}

void init_wifi_manager()
{
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  
  Serial.println("\n Iniciando WiFi");
  pinMode(WIFI_TRIGGER_PIN, INPUT);
  int customFieldLength = 40;
  // test custom html(radio)
  const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input
  
  wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(saveParamCallback);

  std::vector<const char *> menu = {"wifi","info","param","sep","restart","exit"};
  wm.setMenu(menu);

  wm.setClass("invert");
  //set static ip
  wm.setSTAStaticIPConfig(WIFI_IP, WIFI_GW, WIFI_MASK); // set static ip,gw,sn
//  wm.setSTAStaticIPConfig(IPAddress(192,168,1,91), IPAddress(192,168,1,1), IPAddress(255,255,255,0)); // set static ip,gw,sn
  wm.setShowStaticFields(true); // force show static ip fields
  wm.setShowDnsFields(true);    // force show dns field always

  wm.setConfigPortalTimeout(120); // auto close configportal after n seconds
  bool res;  
  res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  //res = wm.autoConnect("AutoConnectAP","password"); // password protected ap
  if(!res) {
    Serial.println("No se pudo conectar");
    // ESP.restart();
    } 
  else {
    //if you get here you have connected to the WiFi    
    Serial.println("conectado...Bien :)");
  }
}  

void wifimanager_loop()
{
  if(wm_nonblocking) wm.process(); // avoid delays() in loop when non-blocking and other long running code  
  checkButton();

}

