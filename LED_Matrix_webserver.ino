#include <AsyncHTTPRequest_Generic.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <PNGdec.h> 
#include "webpages.h"
#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>
#include "audio_reactive.h"
#include <EEPROM.h>
#include "pixelart.h"

// How many leds in your strip?
#define NUM_LEDS 256

// For led chips like WS2812, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
// Clock pin only needed for SPI based chipsets when not using hardware SPI
#define DATA_PIN 2

#define FIRMWARE_VERSION "v1.99.1"

#define EEPROM_SIZE 5
#define EEPROM_BRIGHTNESS   0
#define EEPROM_GAIN         1
#define EEPROM_SQUELCH      2
#define EEPROM_PATTERN      3
#define EEPROM_DISPLAY_TIME 4


PNG png;

const String default_ssid = "mySSID";
const String default_wifipassword = "myPASSWORD";
const char *softAP_ssid = "LEDMATRIX";
const char *softAP_wifipassword = "123456";
const String default_httpuser = "admin";
const String default_httppassword = "admin";
const int default_webserverporthttp = 80;
const int M_HEIGHT = 16;  // height of LED Matrix
const int M_WIDTH = 16;   // width of LED Matrix
const String modedesc0 = "Slide Show";
const String modedesc1 = "Sound Spectrum Analyzer";

// configuration structure
struct Config {
  String ssid;               // wifi ssid
  String wifipassword;       // wifi password
  String httpuser;           // username to access web admin
  String httppassword;       // password to access web admin
  int webserverporthttp;     // http port number for web admin
};

// variables
Config config;                        // configuration
bool shouldReboot = false;            // schedule a reboot
AsyncWebServer *server;               // initialise webserver

// FFT
uint8_t numBands;
uint8_t barWidth;
uint8_t pattern;
uint8_t brightness;
uint16_t displayTime;
bool autoChangePatterns = false;

// Colors and palettes
DEFINE_GRADIENT_PALETTE( purple_gp ) {
  0,   0, 212, 255,   //blue
255, 179,   0, 255 }; //purple
DEFINE_GRADIENT_PALETTE( outrun_gp ) {
  0, 141,   0, 100,   //purple
127, 255, 192,   0,   //yellow
255,   0,   5, 255 };  //blue
DEFINE_GRADIENT_PALETTE( greenblue_gp ) {
  0,   0, 255,  60,   //green
 64,   0, 236, 255,   //cyan
128,   0,   5, 255,   //blue
192,   0, 236, 255,   //cyan
255,   0, 255,  60 }; //green
DEFINE_GRADIENT_PALETTE( redyellow_gp ) {
  0,   200, 200,  200,   //white
 64,   255, 218,    0,   //yellow
128,   231,   0,    0,   //red
192,   255, 218,    0,   //yellow
255,   200, 200,  200 }; //white
CRGBPalette16 purplePal = purple_gp;
CRGBPalette16 outrunPal = outrun_gp;
CRGBPalette16 greenbluePal = greenblue_gp;
CRGBPalette16 heatPal = redyellow_gp;
uint8_t colorTimer = 0;

uint8_t peak[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t prevFFTValue[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t barHeights[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

int mode = 0;                         // modus operandi: 0 = Slide Show, 1 = Audio Spectrum Analyser


// Define the array of leds
cLEDMatrix<M_WIDTH, M_HEIGHT, HORIZONTAL_ZIGZAG_MATRIX> leds;
cLEDText ScrollingMsg;
//CRGB leds[NUM_LEDS]; // remove me

uint32_t ledpic[M_WIDTH*M_HEIGHT];  // storing picture information for LED panel (16x16, 24bit per pixel)
uint32_t leddefault[M_WIDTH*M_HEIGHT] = // initial values for LED panel
{
0x673ab7, 0x673ab7, 0x673ab7, 0x673ab7, 0x673ab7, 0x673ab7, 0x000000, 0x000000, 0x000000, 0x000000, 0x2196f3, 0x2196f3, 0x2196f3, 0x2196f3, 0x2196f3, 0x2196f3,
0x673ab7, 0x673ab7, 0x673ab7, 0x673ab7, 0x000000, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0x000000, 0x2196f3, 0x2196f3, 0x2196f3, 0x2196f3,
0x673ab7, 0x673ab7, 0x673ab7, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0x2196f3, 0x2196f3, 0x2196f3,
0x673ab7, 0x673ab7, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0x2196f3, 0x2196f3,
0x673ab7, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0x2196f3,
0x673ab7, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0x2196f3,
0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000,
0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000,
0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000,
0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000,
0xb71c1c, 0x000000, 0xffeb3b, 0xffeb3b, 0x000000, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0x000000, 0xffeb3b, 0xffeb3b, 0x000000, 0x4caf50,
0xb71c1c, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0x000000, 0x000000, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0x4caf50,
0xb71c1c, 0xb71c1c, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0x4caf50, 0x4caf50,
0xb71c1c, 0xb71c1c, 0xb71c1c, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0x4caf50, 0x4caf50, 0x4caf50,
0xb71c1c, 0xb71c1c, 0xb71c1c, 0xb71c1c, 0x000000, 0x000000, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0xffeb3b, 0x000000, 0x000000, 0x4caf50, 0x4caf50, 0x4caf50, 0x4caf50,
0xb71c1c, 0xb71c1c, 0xb71c1c, 0xb71c1c, 0xb71c1c, 0xb71c1c, 0x000000, 0x000000, 0x000000, 0x000000, 0x4caf50, 0x4caf50, 0x4caf50, 0x4caf50, 0x4caf50, 0x4caf50
};

// function defaults
String listFiles(bool ishtml = false);

void setup() {
  // init LED panel
  //FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical // remove me
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds[0], NUM_LEDS);
  FastLED.clear();
  FastLED.setBrightness(32);
  
  Serial.begin(115200);

  Serial.print("Firmware: "); Serial.println(FIRMWARE_VERSION);

  Serial.println("Booting ...");

  Serial.println("Mounting SPIFFS ...");
  if (!SPIFFS.begin(true)) {
    // if you have not used SPIFFS before on a ESP32, it will show this error.
    // after a reboot SPIFFS will be configured and will happily work.
    Serial.println("ERROR: Cannot mount SPIFFS, Rebooting");
    rebootESP("ERROR: Cannot mount SPIFFS, Rebooting");
  }

  Serial.print("SPIFFS Free: "); Serial.println(humanReadableSize((SPIFFS.totalBytes() - SPIFFS.usedBytes())));
  Serial.print("SPIFFS Used: "); Serial.println(humanReadableSize(SPIFFS.usedBytes()));
  Serial.print("SPIFFS Total: "); Serial.println(humanReadableSize(SPIFFS.totalBytes()));

  Serial.println(listFiles());

  Serial.println("Loading Configuration ...");

  config.ssid = default_ssid;
  config.wifipassword = default_wifipassword;
  config.httpuser = default_httpuser;
  config.httppassword = default_httppassword;
  config.webserverporthttp = default_webserverporthttp;

  
  uint8_t connectionAttempts = 0;
  Serial.print("\nConnecting to Wifi: ");
  WiFi.begin(config.ssid.c_str(), config.wifipassword.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    printArray(Wifilogo[connectionAttempts % 4], 500);
    //delay(1000);
    Serial.print(".");
    connectionAttempts++;
    if (connectionAttempts > 10) break;
  }

  if (WiFi.status() != WL_CONNECTED) {
    printArray(Wifilogo[0], 0);
    WiFi.softAP(softAP_ssid, softAP_wifipassword);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    showAPInfo();
  }
  
  
  Serial.println("\n\nNetwork Configuration:");
  Serial.println("----------------------");
  Serial.print("         SSID: "); Serial.println(WiFi.SSID());
  Serial.print("  Wifi Status: "); Serial.println(WiFi.status());
  Serial.print("Wifi Strength: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
  Serial.print("          MAC: "); Serial.println(WiFi.macAddress());
  Serial.print("           IP: "); Serial.println(WiFi.localIP());
  Serial.print("       Subnet: "); Serial.println(WiFi.subnetMask());
  Serial.print("      Gateway: "); Serial.println(WiFi.gatewayIP());
  Serial.print("        DNS 1: "); Serial.println(WiFi.dnsIP(0));
  Serial.print("        DNS 2: "); Serial.println(WiFi.dnsIP(1));
  Serial.print("        DNS 3: "); Serial.println(WiFi.dnsIP(2));
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) showIP();

  // configure web server
  Serial.println("Configuring Webserver ...");
  server = new AsyncWebServer(config.webserverporthttp);
  configureWebServer();

  // startup web server
  Serial.println("Starting Webserver ...");
  server->begin();
  
  setupAudio();
  if (M_WIDTH == 8) numBands = 8;
  else numBands = 16;
  barWidth = M_WIDTH / numBands;
  
  EEPROM.begin(EEPROM_SIZE);
  
  // It should not normally be possible to set the gain to 255
  // If this has happened, the EEPROM has probably never been written to
  // (new board?) so reset the values to something sane.
  if (EEPROM.read(EEPROM_GAIN) == 255) {
    EEPROM.write(EEPROM_BRIGHTNESS, 32);
    EEPROM.write(EEPROM_GAIN, 0);
    EEPROM.write(EEPROM_SQUELCH, 0);
    EEPROM.write(EEPROM_PATTERN, 0);
    EEPROM.write(EEPROM_DISPLAY_TIME, 10);
    EEPROM.commit();
  }

  // Read saved values from EEPROM
  FastLED.setBrightness( EEPROM.read(EEPROM_BRIGHTNESS));
  brightness = FastLED.getBrightness();
  gain = EEPROM.read(EEPROM_GAIN);
  squelch = EEPROM.read(EEPROM_SQUELCH);
  pattern = EEPROM.read(EEPROM_PATTERN);
  displayTime = EEPROM.read(EEPROM_DISPLAY_TIME);
  
  // show demo smiley :)
  printArray(leddefault, 5000);
}

void loop() {
  // reboot if we've told it to reboot
  if (shouldReboot) {
    rebootESP("Web Admin Initiated Reboot");
  }

  switch(mode){
  case 0:
    readFiles();
    break;
  case 1:
    showAudio();
    break;
  }
}

void printArray(uint32_t Panel[], int DelayTime) {
  for (int i = 0; i < NUM_LEDS; i++) {
    int x = i % M_WIDTH;
    int y = M_HEIGHT - i / M_HEIGHT -1;
    leds(x,y) = Panel[i];
  }
  FastLED.show();
  delay(DelayTime);
}

// Functions to access a file on the SD card
File myfile;

void * myOpen(const char *filename, int32_t *size) {
  //Serial.printf("Attempting to open %s\n", filename);
  myfile = SPIFFS.open(filename);
  *size = myfile.size();
  return &myfile;
}
void myClose(void *handle) {
  if (myfile) myfile.close();
}
int32_t myRead(PNGFILE *handle, uint8_t *buffer, int32_t length) {
  if (!myfile) return 0;
  return myfile.read(buffer, length);
}
int32_t mySeek(PNGFILE *handle, int32_t position) {
  if (!myfile) return 0;
  return myfile.seek(position);
}

// Function to draw pixels to the display
void PNGDraw(PNGDRAW *pDraw) {
uint16_t usPixels[M_WIDTH];

  png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);

  // Convert to RGB888 (true color)
  for(int i = 0; i < M_WIDTH; i++) {
    uint8_t r = ((((usPixels[i] >> 11) & 0x1F) * 527) + 23) >> 6;
    uint8_t g = ((((usPixels[i] >> 5) & 0x3F) * 259) + 33) >> 6;
    uint8_t b = (((usPixels[i] & 0x1F) * 527) + 23) >> 6;

    uint32_t RGB888 = r << 16 | g << 8 | b;

    // write into LED array
    ledpic[ i + M_WIDTH*pDraw->y ] = RGB888;
  }
}

void readFiles() {
  int rc;
  int filenum = 0;
  Serial.println("Reading Files from SPIFFS");
  File root = SPIFFS.open("/");
  File foundfile = root.openNextFile();
  // 
  while (foundfile) {
    if (mode > 0) { // stop loop if mode was changed
      break;
    }
    if (foundfile.isDirectory() == false) {
      //Serial.print("Filename :" + String(foundfile.name()) + "\n");
      const char *name = foundfile.name();
      const int len = strlen(name);
      if (len > 3 && strcmp(name + len - 3, "png") == 0) {
        // it's a PNG ;-)
        rc = png.open((const char *)name, myOpen, myClose, myRead, mySeek, PNGDraw);
        if (rc == PNG_SUCCESS) {
          //Serial.printf("image number %d\n", filenum);
          //Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d, buffer size: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType(), png.getBufferSize());
          // dump color values from PNG
          rc = png.decode(NULL, 0);
          // close file and increase counter
          png.close();
          filenum++;

          // now display on LED matrix
          printArray(ledpic, 1000);
        }
      }
    }
    
    foundfile = root.openNextFile();
  }
  foundfile.close();
  root.close();
}

void rebootESP(String message) {
  Serial.print("Rebooting ESP32: "); Serial.println(message);
  ESP.restart();
}

// list all of the files, if ishtml=true, return html rather than simple text
String listFiles(bool ishtml) {
  String returnText = "";
  Serial.println("Listing files stored on SPIFFS");
  File root = SPIFFS.open("/");
  File foundfile = root.openNextFile();
  if (ishtml) {
    returnText += "<table><tr><th align='left'>Name</th><th align='left'>Size</th><th></th><th></th></tr>";
  }
  while (foundfile) {
    if (ishtml) {
      returnText += "<tr align='left'><td>" + String(foundfile.name()) + "</td><td>" + humanReadableSize(foundfile.size()) + "</td>";
      returnText += "<td><button onclick=\"downloadDeleteButton(\'" + String(foundfile.name()) + "\', \'download\')\">Download</button>";
      returnText += "<td><button onclick=\"downloadDeleteButton(\'" + String(foundfile.name()) + "\', \'delete\')\">Delete</button></tr>";
    } else {
      returnText += "File: " + String(foundfile.name()) + " Size: " + humanReadableSize(foundfile.size()) + "\n";
    }
    foundfile = root.openNextFile();
  }
  if (ishtml) {
    returnText += "</table>";
  }
  root.close();
  foundfile.close();
  return returnText;
}

String changeMode(bool ishtml) {
  String returnText = "";
  mode = 1 - mode;
  if (ishtml) {
    returnText += "<p>Current mode: ";
    switch(mode){
      case 0:
        returnText += modedesc0;
        break;
      case 1:
        returnText += modedesc1;
    }
    returnText += "</p>";
  }
}

// Make size of files human readable
// source: https://github.com/CelliesProjects/minimalUploadAuthESP32
String humanReadableSize(const size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " KB";
  else if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0) + " MB";
  else return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

void showIP(){
  char strIP[16] = "               ";
  IPAddress ip = WiFi.localIP();
  ip.toString().toCharArray(strIP, 16);
  Serial.println(strIP);
  ScrollingMsg.SetFont(MatriseFontData);
  ScrollingMsg.Init(&leds, leds.Width(), ScrollingMsg.FontHeight() + 1, 0, 0);
  ScrollingMsg.SetText((unsigned char *)strIP, sizeof(strIP) - 1);
  ScrollingMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0xff, 0xff);
  ScrollingMsg.SetScrollDirection(SCROLL_LEFT);
  ScrollingMsg.SetFrameRate(160 / M_WIDTH);       // Faster for larger matrices

  while(ScrollingMsg.UpdateText() == 0) {
    FastLED.show();  
  }
}

void showAPInfo(){
  char strIP[100] = "                                                                                                   ";
  
  // SSID softAP_ssid, 
  // PASSWORD softAP_wifipassword
  // IP ADDRESS

  IPAddress IP = WiFi.softAPIP();
  String ssid = String(softAP_ssid);
  String pswd = String(softAP_wifipassword);
  
  
  String strAPInfo = "SSID: " + ssid + " Password: " + pswd + " IP address: " + IP.toString();
  
  Serial.println(strAPInfo);
  Serial.println(strAPInfo.length());
  strAPInfo.toCharArray(strIP, 100);
  ScrollingMsg.SetFont(MatriseFontData);
  ScrollingMsg.Init(&leds, leds.Width(), ScrollingMsg.FontHeight() + 1, 0, 0);
  ScrollingMsg.SetText((unsigned char *)strIP, strAPInfo.length());
  ScrollingMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0xff, 0xff);
  ScrollingMsg.SetScrollDirection(SCROLL_LEFT);
  ScrollingMsg.SetFrameRate(160 / M_WIDTH);       // Faster for larger matrices

  while(ScrollingMsg.UpdateText() == 0) {
    FastLED.show();  
  }  
}
void showAudio(){
  if (pattern != 5) FastLED.clear();
  
  uint8_t divisor = 1;                                                    // If 8 bands, we need to divide things by 2
  if (numBands == 8) divisor = 2;                                         // and average each pair of bands together
  
  for (int i = 0; i < 16; i += divisor) {
    uint8_t fftValue;
    
    if (numBands == 8) fftValue = (fftResult[i] + fftResult[i+1]) / 2;    // Average every two bands if numBands = 8
    else fftValue = fftResult[i];

    fftValue = ((prevFFTValue[i/divisor] * 3) + fftValue) / 4;            // Dirty rolling average between frames to reduce flicker
    barHeights[i/divisor] = fftValue / (255 / M_HEIGHT);                  // Scale bar height
    
    if (barHeights[i/divisor] > peak[i/divisor])                          // Move peak up
      peak[i/divisor] = min(M_HEIGHT, (int)barHeights[i/divisor]);
      
    prevFFTValue[i/divisor] = fftValue;                                   // Save prevFFTValue for averaging later
    
  }

  // Draw the patterns
  for (int band = 0; band < numBands; band++) {
    drawPatterns(band);
  }

  // Decay peak
  EVERY_N_MILLISECONDS(60) {
    for (uint8_t band = 0; band < numBands; band++)
      if (peak[band] > 0) peak[band] -= 1;
  }

  EVERY_N_SECONDS(30) {
    // Save values in EEPROM. Will only be commited if values have changed.
    EEPROM.write(EEPROM_BRIGHTNESS, brightness);
    EEPROM.write(EEPROM_GAIN, gain);
    EEPROM.write(EEPROM_SQUELCH, squelch);
    EEPROM.write(EEPROM_PATTERN, pattern);
    EEPROM.write(EEPROM_DISPLAY_TIME, displayTime);
    EEPROM.commit();
  }
  
  EVERY_N_SECONDS_I(timingObj, displayTime) {
    timingObj.setPeriod(displayTime);
    if (autoChangePatterns) pattern = (pattern + 1) % 6;
  }
  
  FastLED.setBrightness(brightness);
  FastLED.show();
}

void drawPatterns(uint8_t band) {
  
  uint8_t barHeight = barHeights[band];
  
  // Draw bars
  switch (pattern) {
    case 0:
      rainbowBars(band, barHeight);
      break;
    case 1:
      // No bars on this one
      break;
    case 2:
      purpleBars(band, barHeight);
      break;
    case 3:
      centerBars(band, barHeight);
      break;
    case 4:
      changingBars(band, barHeight);
      EVERY_N_MILLISECONDS(10) { colorTimer++; }
      break;
    case 5:
      createWaterfall(band);
      EVERY_N_MILLISECONDS(30) { moveWaterfall(); }
      break;
  }

  // Draw peaks
  switch (pattern) {
    case 0:
      whitePeak(band);
      break;
    case 1:
      outrunPeak(band);
      break;
    case 2:
      whitePeak(band);
      break;
    case 3:
      // No peaks
      break;
    case 4:
      // No peaks
      break;
    case 5:
      // No peaks
      break;
  }
}


//////////// Patterns ////////////

void rainbowBars(uint8_t band, uint8_t barHeight) {
  int xStart = barWidth * band;
  for (int x = xStart; x < xStart + barWidth; x++) {
    for (int y = 0; y <= barHeight; y++) {
      leds(x,y) = CHSV((x / barWidth) * (255 / numBands), 255, 255);
    }
  }
}

void purpleBars(int band, int barHeight) {
  int xStart = barWidth * band;
  for (int x = xStart; x < xStart + barWidth; x++) {
    for (int y = 0; y < barHeight; y++) {
      leds(x,y) = ColorFromPalette(purplePal, y * (255 / barHeight));
    }
  }
}

void changingBars(int band, int barHeight) {
  int xStart = barWidth * band;
  for (int x = xStart; x < xStart + barWidth; x++) {
    for (int y = 0; y < barHeight; y++) {
      leds(x,y) = CHSV(y * (255 / M_HEIGHT) + colorTimer, 255, 255); 
    }
  }
}

void centerBars(int band, int barHeight) {
  int xStart = barWidth * band;
  for (int x = xStart; x < xStart + barWidth; x++) {
    if (barHeight % 2 == 0) barHeight--;
    int yStart = ((M_HEIGHT - barHeight) / 2 );
    for (int y = yStart; y <= (yStart + barHeight); y++) {
      int colorIndex = constrain((y - yStart) * (255 / barHeight), 0, 255);
      leds(x,y) = ColorFromPalette(heatPal, colorIndex);
    }
  }
}

void whitePeak(int band) {
  int xStart = barWidth * band;
  int peakHeight = peak[band];
  for (int x = xStart; x < xStart + barWidth; x++) {
    leds(x,peakHeight) = CRGB::White;
  }
}

void outrunPeak(int band) {
  int xStart = barWidth * band;
  int peakHeight = peak[band];
  for (int x = xStart; x < xStart + barWidth; x++) {
    leds(x,peakHeight) = ColorFromPalette(outrunPal, peakHeight * (255 / M_HEIGHT));
  }
}

void createWaterfall(int band) {
  int xStart = barWidth * band;
  // Draw bottom line
  for (int x = xStart; x < xStart + barWidth; x++) {
    leds(x,0) = CHSV(constrain(map(fftResult[band],0,254,160,0),0,160), 255, 255);
  }
}

void moveWaterfall() {
  // Move screen up starting at 2nd row from top
  for (int y = M_HEIGHT - 2; y >= 0; y--) {
    for (int x = 0; x < M_WIDTH; x++) {
      leds(x,y+1) = leds(x,y);
    }
  }
}
