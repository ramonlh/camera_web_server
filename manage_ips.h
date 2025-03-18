

#include <ArduinoJson.h>

unsigned long time_delay60 = 60000;
unsigned long last_time;

void init_manageips()
  {
  last_time= millis();
  }

void send_IPs()
{
}

void get_IPs()
{
}

void send_and_get_ips()
{
}

void manage_ips_loop()
{
  if ((millis()-last_time) > time_delay60)
    {
    last_time=millis();
    }
}