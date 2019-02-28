#include <define_plugin_sets.h>
#ifdef USES_P127

#include <Arduino.h>
#include <ESPEasy-Globals.h>
// #include <WebServer.ino>
//#######################################################################################################
//#################################### Plugin 127: Teleinfo #############################################
//#################################### This plugin transmits data of energy counter to HTTP server  #####
//#################################### Compatible with jeedom plugin Teleinfo                       #####
//#######################################################################################################

#define PLUGIN_127
#define PLUGIN_ID_127 127
#define PLUGIN_NAME_127 "Teleinfo"

/**************************************************\
CONFIG
TaskDevicePluginConfig settings:
0: Port
1: IINST
\**************************************************/

#define P127_BUFFER_SIZE 128
boolean Plugin_127_init = false;
// 0 = Cycle not complete, 1 = ADCO read, 2 = MOTDETAT read
int P127_CYCLE = 0;
String P127_SendData = "";
String P127_URL = "";
int P127_PORT = 80;
String P127_HOST = "";

#define P127_VALUES_COUNTER 1
String P127_values[P127_VALUES_COUNTER];
String P127_indexes[P127_VALUES_COUNTER];

#define P127_INDEX_IINST 0

boolean Plugin_127(byte function, struct EventStruct *event, String &string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
    {
      Device[++deviceCount].Number = PLUGIN_ID_127;
      Device[deviceCount].SendDataOption = false;
      Device[deviceCount].TimerOption = false;
      Device[deviceCount].Ports = 0;
      Device[deviceCount].PullUpOption = false;
      Device[deviceCount].InverseLogicOption = false;
      Device[deviceCount].FormulaOption = false;
      Device[deviceCount].Custom = true;
      Device[deviceCount].ValueCount = 0;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_127);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {

      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      char deviceTemplate[2][128];
      LoadCustomTaskSettings(event->TaskIndex, (byte *)&deviceTemplate, sizeof(deviceTemplate));

      addFormTextBox(F("Host"), F("Plugin_127_host"), deviceTemplate[0], 128);
      addFormNumericBox(F("Host"), F("Plugin_127_port"), PCONFIG(0));
      addFormTextBox(F("Start url"), F("Plugin_127_url"), deviceTemplate[1], 128);
      addFormNumericBox(F("IINST Index"), F("Plugin_127_IINST_Index"), PCONFIG(1));

      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      char deviceTemplate[2][128];

      safe_strncpy(deviceTemplate[0], WebServer.arg(F("Plugin_127_host")), 128);
      safe_strncpy(deviceTemplate[1], WebServer.arg(F("Plugin_127_url")), 128);
      PCONFIG(0) = getFormItemInt(F("Plugin_127_port"));
      PCONFIG(1) = getFormItemInt(F("Plugin_127_IINST_Index"));

      SaveCustomTaskSettings(event->TaskIndex, (byte *)&deviceTemplate, sizeof(deviceTemplate));
      success = true;
      break;
    }

    case PLUGIN_INIT: // ok
    {
      char deviceTemplate[2][128];
      LoadCustomTaskSettings(event->TaskIndex, (byte *)&deviceTemplate, sizeof(deviceTemplate));
      P127_HOST = String(deviceTemplate[0]);
      P127_PORT = PCONFIG(0);
      P127_URL = String(deviceTemplate[1]);
      P127_indexes[P127_INDEX_IINST] = PCONFIG(1);
      Serial.begin(1200, SERIAL_7E1); //Liaison série avec les paramètres
      Plugin_127_init = true;
      success = true;
      break;
    }

    case PLUGIN_TEN_PER_SECOND:
    {
      addLog(LOG_LEVEL_DEBUG, F("P127_Teleinfo"));
    }

    case PLUGIN_SERIAL_IN:
    {
      addLog(LOG_LEVEL_DEBUG, F("PLUGIN_SERIAL_IN"));
      uint8_t serial_buf[P127_BUFFER_SIZE];
      int RXWait = 10;
      int timeOut = RXWait;
      size_t bytes_read = 0;

      while (timeOut > 0)
      {
        while (Serial.available())
        {
          if (bytes_read < P127_BUFFER_SIZE)
          {
            serial_buf[bytes_read] = Serial.read();
            bytes_read++;
          }
          else
          {
            Serial.read(); // when the buffer is full, just read remaining input, but do not store...
          }
          timeOut = RXWait; // if serial received, reset timeout counter
        }
        delay(1);
        timeOut--;
      }

      if (bytes_read == P127_BUFFER_SIZE) // if we have a full buffer, drop the last position to stuff with string end marker
      {
        while (Serial.available())
        { // read possible remaining data to avoid sending rubbish...
          Serial.read();
        }
        bytes_read--;
        // and log buffer full situation
        // strcpy_P(log, PSTR("Teleinfo: serial buffer full!"));
        // addLog(LOG_LEVEL_ERROR, log);
      }
      serial_buf[bytes_read] = 0; // before logging as a char array, zero terminate the last position to be safe.
      //	char log[P127_BUFFER_SIZE + 40];
      //	sprintf_P(log, PSTR("Ser2N: S>: %s"), (char*)serial_buf);
      //	addLog(LOG_LEVEL_DEBUG, log);

      // We can also use the rules engine for local control!

      String message = (char *)serial_buf;
      addLog(LOG_LEVEL_DEBUG, message);
      int NewLinePos = message.indexOf("\r\n");
      if (NewLinePos > 0)
      {
        message = message.substring(0, NewLinePos);
      }

      // message.replace("\r", "");
      if (message.length() > 5)
      {
        int indexValue = message.indexOf(" ");
        String nameValue = message.substring(1, indexValue);
        String value = message.substring(indexValue + 1, message.length() - 3);
        String checksum = message.substring(message.length() - 2);

        switch (P127_CYCLE)
        {
        case 0:
        {
          // ADCO not already read, do nothing to read all informations
          if (nameValue == "ADCO")
            P127_CYCLE = 1;
          break;
        }
        case 1:
        {
          // ADCO read, start to store the expected values
          if (nameValue == "IINST" || nameValue == "IINST1")
            P127_values[P127_INDEX_IINST] = value;
          else if (nameValue == "IINST2")
            P127_values[P127_INDEX_IINST] = P127_values[P127_INDEX_IINST] + ";" + value;
          else if (nameValue == "IINST3")
            P127_values[P127_INDEX_IINST] = P127_values[P127_INDEX_IINST] + ";" + value;
          /*else if (nameValue == "BASE")
            P127_values[P127_INDEX_BASE] = value;
          else if (nameValue == "IMAX")
            P127_values[P127_INDEX_IMAX] = value;*/
          else if (nameValue == "MOTDETAT")
          {
            P127_sendtoHTTP(P127_HOST, P127_PORT, P127_URL);
            P127_values[P127_INDEX_IINST] = "";
            P127_CYCLE = 0;
          }
          break;
        }
        }
      }

      success = true;
      break;
    }
  }

  return success;
}

boolean P127_checksum(String valuename, String value, String checksum)
{
  String data = "";
  int i;
  char sum = 0;
  char sumchar;
  sumchar = checksum.charAt(0);
  data = valuename + " " + value;
  for (i = 0; i < data.length(); i++)
  {
    sum = sum + data.charAt(i);
  }
  sum = (sum & 0x3F) + 0x20;

  if (sum == sumchar)
  {
    return true;
  }
  else
  {
    return false;
  }
}

boolean P127_sendValues()
{
  boolean success = true;
  char urlCommand[128];

  // Send IINST
  // Electricity 3 Ampere /json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=Ampere_1;=Ampere_2;=Ampere_3;
  sprintf_P(urlCommand, PSTR("/json.htm?type=command&param=udevice&idx=%s&nvalue=0&svalue=%s"), P127_indexes[P127_INDEX_IINST].c_str(), P127_values[P127_INDEX_IINST].c_str());
  success = P127_sendtoHTTP(P127_HOST, P127_PORT, P127_URL + urlCommand);

  return success;
}

boolean P127_sendtoHTTP(String hostname, int port, String url)
{
  char log[80];
  boolean success = false;
  char host[128];

  hostname.toCharArray(host, 128);
  sprintf_P(log, PSTR("%s%s using port %u"), "HTTP : connecting to ", host, port);
  addLog(LOG_LEVEL_DEBUG, log);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(host, port))
  {
    connectionFailures++;
    strcpy_P(log, PSTR("HTTP : connection failed"));
    addLog(LOG_LEVEL_ERROR, log);
    return false;
  }
  statusLED(true);
  if (connectionFailures)
    connectionFailures--;

  url.toCharArray(log, 80);
  addLog(LOG_LEVEL_DEBUG_MORE, log);

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long timer = millis() + 200;
  while (!client.available() && millis() < timer)
    delay(1);

  // Read all the lines of the reply from server and print them to Serial
  while (client.available())
  {
    String line = client.readStringUntil('\n');
    line.toCharArray(log, 80);
    addLog(LOG_LEVEL_DEBUG_MORE, log);
    if (line.substring(0, 15) == "HTTP/1.1 200 OK")
    {
      strcpy_P(log, PSTR("HTTP : Succes!"));
      addLog(LOG_LEVEL_DEBUG, log);
      success = true;
    }
    delay(1);
  }
  strcpy_P(log, PSTR("HTTP : closing connection"));
  addLog(LOG_LEVEL_DEBUG, log);

  client.flush();
  client.stop();

  return success;
}

#endif