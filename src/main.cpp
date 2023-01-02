#include <Arduino.h>
#include <WiFiManager.h> 
#include <EEPROM.h>
#include <Servo.h>
#include <AsyncHTTPRequest_Generic.h> 
#include <Ticker.h>
#include <ESP8266WiFi.h>

// Refer https://github.com/Madsn/gtfu/blob/master/Arduino/Long-polling/Long-polling.ino
// Refer https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding

#define LED_PIN         2       // D1 Mini Hardware LED
#define TRIGGER_PIN     5       // Physical momentary button 
#define SERVO_PIN       4       // Servo output

#define MAX_URL_LENGTH  128

#define HTTP_REQUEST_INTERVAL     5 // Interval to avoid spamming if we have server side issues

WiFiManager wifiManager; // WiFi AP controller
WiFiManagerParameter wfm_poll_url("poll_url", "Long Poll URL", "http://my.longpoll/unset", MAX_URL_LENGTH);

WiFiClient wifi;

Servo servo;

AsyncHTTPRequest httpRequest;
Ticker httpRequestTicker;

bool httpRingRequestPending = false;

void saveConfig () {
  Serial.println("Saving parameters to EEPROM");
  EEPROM.begin(256);

  const char* pollURL = wfm_poll_url.getValue();
  int length = wfm_poll_url.getValueLength();
  
  int addr=0;
  EEPROM.put(addr++, length);
  for (int i=0;i<length;i++) {
    EEPROM.put(addr++, pollURL[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}

void loadConfig() {
  Serial.println("Loading config");

  EEPROM.begin(256);

  int addr=0;
  int length = EEPROM.read(addr++);

  if (length == 255) { // Default value
    Serial.println("Skipped load as value was set to default (un-initialized memory)");
    EEPROM.end();
    return;
  }

  char data[length];
  for (int i=0;i<length;i++) {
    data[i] = EEPROM.read(addr++);
  }

  wfm_poll_url.setValue(data, MAX_URL_LENGTH);

  EEPROM.end();
}

void ring() {
    Serial.println("Ringing the gong... ");


    servo.attach(SERVO_PIN);
    digitalWrite(LED_PIN, LOW);
    servo.write(180);
    delay(250);
    servo.write(0);
    delay(500);
    digitalWrite(LED_PIN, HIGH);
    servo.detach();
}

void sendHttpRequest() {
  
  if (WiFi.isConnected() && (httpRequest.readyState() == readyStateUnsent || httpRequest.readyState() == readyStateDone))
  {
    Serial.printf("GET %s\n", wfm_poll_url.getValue());

    if (httpRequest.open("GET", wfm_poll_url.getValue()))
    {
      // Only send() if open() returns true, or crash
      httpRequest.send();
      httpRequest.setTimeout(60);
    }
    else
    {
      Serial.println(F("Can't send bad request"));
    }
  }
  else
  {
    //Serial.println(F("Can't send request"));
  }
}

void httpRequestCallback(void *optParm, AsyncHTTPRequest *request, int readyState)
{
  (void) optParm;

  Serial.printf("Ready state: %d\n", readyState);

  if (readyState == readyStateDone)
  {
    Serial.printf("HTTP Response code %d\n", request->responseHTTPcode());

    //if (request->responseHTTPcode() == 200)
    //{
      // Might have terminated early...
    //}
  }
}


/** 
 * Listen for data rather than the successful http response, as if something goes wrong
 * after chunking has started, we will still get a success response.
**/
void httpDataCallback(void *optParm, AsyncHTTPRequest *request, size_t dataSize)
{
  (void) optParm;
  String data = request->responseText();
  Serial.printf("Received data of size %d. Got string: \"%s\"\n", dataSize, data.c_str());
  data.trim();
  if (data.length() > 0) {
      httpRingRequestPending = true;
  }
}

void setup() {
  Serial.begin(115200); 
  Serial.println("\nStart Setup");
  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  loadConfig();

  wifiManager.addParameter(&wfm_poll_url);

  wifiManager.setSaveConfigCallback(saveConfig);
  wifiManager.setConfigPortalTimeout(600); // 10 minute config timeout after reboot (i.e power loss)

  if (digitalRead(TRIGGER_PIN) == LOW) {
    Serial.printf("Pin %d was LOW during setup, starting config portal.\n", TRIGGER_PIN);
    wifiManager.startConfigPortal("Gongbot", NULL);
  }
  else {
    wifiManager.autoConnect("Gongbot", NULL);
  }

  Serial.println("Connected to WiFi");
  WiFi.begin();
  digitalWrite(LED_PIN, HIGH);

  httpRequest.setDebug(true);
  httpRequest.onReadyStateChange(httpRequestCallback);
  httpRequest.onData(httpDataCallback);
  httpRequestTicker.attach(HTTP_REQUEST_INTERVAL, sendHttpRequest);
}

void loop(){
  if (digitalRead(TRIGGER_PIN) == LOW) {
    ring();
  }
  else if (httpRingRequestPending) {
    httpRingRequestPending = false;
    ring();
  }
}