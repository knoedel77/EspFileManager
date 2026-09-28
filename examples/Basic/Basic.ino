#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>


#include <EspFileManager.h>

#define MICROSD_SPI_SS_PIN 5
#define ST_SSID "XXXXXXXXXXX"
#define ST_PASS "XXXXXXXXXXX"

AsyncWebServer server(80);
EspFileManager FileManager;

void setup() 
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
	WiFi.begin(ST_SSID, ST_PASS);
	while (WiFi.status() != WL_CONNECTED);
	{
		Serial.print(".");
		delay(50);
	}
  Serial.print("Connected to wifi... \nIP: ");
  Serial.println(WiFi.localIP());

  if (!SD.begin(MICROSD_SPI_SS_PIN)) {
    Serial.println("SD card init failed");
  } else {
    Serial.println("SD card initialized");
    FileManager.setSDFileSource(&SD);
  }

  if(!LittleFS.begin(true)) {
    Serial.println("LittleFS init failed");
  } else {
    Serial.println("LittleFS initialized");
    FileManager.setLittleFSFileSource(&LittleFS);
  }

  FileManager.setServer(&server);

  server.begin();
}

void loop() 
{
  
}