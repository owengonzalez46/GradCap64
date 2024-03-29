
/*
   Animated GIFs Display Code for SmartMatrix and 32x32 RGB LED Panels

   Uses SmartMatrix Library for Teensy 3.1 written by Louis Beaudoin at pixelmatix.com

   Written by: Craig A. Lindley

   Copyright (c) 2014 Craig A. Lindley
   Refactoring by Louis Beaudoin (Pixelmatix)

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
   the Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
   FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
   COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
   This example displays 32x32 GIF animations loaded from a SD Card connected to the Teensy 3.1
   The GIFs can be up to 32 pixels in width and height.
   This code has been tested with 32x32 pixel and 16x16 pixel GIFs, but is optimized for 32x32 pixel GIFs.

   Wiring is on the default Teensy 3.1 SPI pins, and chip select can be on any GPIO,
   set by defining SD_CS in the code below
   Function     | Pin
   DOUT         |  11
   DIN          |  12
   CLK          |  13
   CS (default) |  15

   This code first looks for .gif files in the /gifs/ directory
   (customize below with the GIF_DIRECTORY definition) then plays random GIFs in the directory,
   looping each GIF for DISPLAY_TIME_SECONDS

   This example is meant to give you an idea of how to add GIF playback to your own sketch.
   For a project that adds GIF playback with other features, take a look at
   Light Appliance and Aurora:
   https://github.com/CraigLindley/LightAppliance
   https://github.com/pixelmatix/aurora

   If you find any GIFs that won't play properly, please attach them to a new
   Issue post in the GitHub repo here:
   https://github.com/pixelmatix/AnimatedGIFs/issues
*/

/*
   CONFIGURATION:
    - update the "SmartMatrix configuration and memory allocation" section to match the width and height and other configuration of your display
    - Note for 128x32 and 64x64 displays - need to reduce RAM:
      set kRefreshDepth=24 and kDmaBufferRows=2 or set USB Type: "None" in Arduino,
      decrease refreshRate in setup() to 90 or lower to get good an accurate GIF frame rate
    - WIDTH and HEIGHT are defined in GIFParseFunctions.cpp, update to match the size of your GIFs
      only play GIFs that are size WIDTHxHEIGHT or smaller
*/
#include <MatrixHardware_Teensy3_ShieldV4.h>        // SmartLED Shield for Teensy 3 (V4)
#include <SmartMatrix.h>

#include <SD.h>
#include <SPI.h>
#include "GIFDecoder.h"

#define HWSERIAL Serial1

#define DISPLAY_TIME_SECONDS 10

#define ENABLE_SCROLLING  1

#define COLOR_DEPTH 24

// range 0-255
const int defaultBrightness = 255;

const rgb24 COLOR_BLACK = {
  0, 0, 0
};


/* SmartMatrix configuration and memory allocation */
#define COLOR_DEPTH 24                  // known working: 24, 48 - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 64;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 64;       // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 36;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4
const uint8_t kPanelType = SMARTMATRIX_HUB75_64ROW_MOD32SCAN; // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);        // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
#if ENABLE_SCROLLING == 1
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);
#endif

// Chip select for SD card on the SmartMatrix Shield
#define SD_CS BUILTIN_SDCARD

#define GIF_DIRECTORY "/gifs/"

int num_files;

String message; //string that stores the incoming message

void screenClearCallback(void) {
  backgroundLayer.fillScreen({0, 0, 0});
}

void updateScreenCallback(void) {
  backgroundLayer.swapBuffers();
}

void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue) {
  backgroundLayer.drawPixel(x, y, {red, green, blue});
}

// Setup method runs once, when the sketch starts
void setup() {
  setScreenClearCallback(screenClearCallback);
  setUpdateScreenCallback(updateScreenCallback);
  setDrawPixelCallback(drawPixelCallback);

  // Seed the random number generator
  randomSeed(analogRead(14));

  Serial.begin(115200);
  HWSERIAL.begin(9600);

  // Initialize matrix
  matrix.setRotation(rotation180);
  matrix.addLayer(&backgroundLayer);
  //matrix.addLayer(&scrollingLayer);


#if ENABLE_SCROLLING == 1
  matrix.addLayer(&scrollingLayer);
  scrollingLayer.setOffsetFromTop(6);
#endif

  matrix.begin();

  //matrix.setRefreshRate(90);
  // for large panels, set the refresh rate lower to leave more CPU time to decoding GIFs (needed if GIFs are playing back slowly)

  // Clear screen
  backgroundLayer.fillScreen(COLOR_BLACK);
  backgroundLayer.swapBuffers();

  // initialize the SD card at full speed
  pinMode(SD_CS, OUTPUT);
  if (!SD.begin(SD_CS)) {
    //#if ENABLE_SCROLLING == 1
    //    scrollingLayer.start("No SD card", -1);
    //#endif
    //    HWSERIAL.println("No SD card");
    //    while (1);
  }

  // Determine how many animated GIF files exist
  num_files = enumerateGIFFiles(GIF_DIRECTORY, false);

  if (num_files < 0) {
    //#if ENABLE_SCROLLING == 1
    //    scrollingLayer.start("No gifs directory", -1);
    //#endif
    //    HWSERIAL.println("No gifs directory");
    //    while (1);
  }

  if (!num_files) {
    //#if ENABLE_SCROLLING == 1
    //    scrollingLayer.start("Empty gifs directory", -1);
    //#endif
    //    HWSERIAL.println("Empty gifs directory");
    //    while (1);
  }
}


void loop() {

  unsigned long futureTime;
  char pathname[30];

  int index = 0;

  String dispText;
  char charBuf[1000];
  const int leftEdgeOffset = 1;
  const int delayBetweenCharacters = 10000;

  const uint transitionTime = 8000;

  unsigned long currentMillis;

  index = 0;
  delay (200);
  backgroundLayer.fillScreen(COLOR_BLACK);
  backgroundLayer.swapBuffers();
  getGIFFilenameByIndex(GIF_DIRECTORY, index, pathname);

  // Do forever
  while (true) {

    while (HWSERIAL.available())
    { //while there is data available on the serial monitor
      message += char(HWSERIAL.read()); //store string from serial command
      delay(50);
    }
    if (!HWSERIAL.available())
    {
      if (message != "")
      { //if data is available
        HWSERIAL.println(message); //show the data
        if (message.toInt() || message == "0")
        {
          HWSERIAL.println("got it");
          // Can clear screen for new animation here, but this might cause flicker with short animations
          index = message.toInt();
          delay (200);
          backgroundLayer.fillScreen(COLOR_BLACK);
          backgroundLayer.swapBuffers();
          getGIFFilenameByIndex(GIF_DIRECTORY, index, pathname);
          message = "";
        }
        else if (message.charAt(0) == '*')
        {
          backgroundLayer.fillScreen(COLOR_BLACK);
          backgroundLayer.swapBuffers();
          index = 7;
          delay (200);
          backgroundLayer.fillScreen(COLOR_BLACK);
          backgroundLayer.swapBuffers();
          getGIFFilenameByIndex(GIF_DIRECTORY, index, pathname);
          delay (200);
          message.remove(0,1);
          message.toCharArray(charBuf, 1000);
          scrollingLayer.setColor({0xff, 0xff, 0xff});
          scrollingLayer.setMode(wrapForward);
          scrollingLayer.setSpeed(25);
          scrollingLayer.setFont(font8x13);
          scrollingLayer.start(charBuf, 1);
          message = "";
        }
        else
        {
          HWSERIAL.println("not it");
          message = ""; //clear the data
        }
      }
    }

    //    if (index >= num_files) {
    //      index = 0;
    //    }

    // Calculate time in the future to terminate animation
    //futureTime = millis() + (DISPLAY_TIME_SECONDS * 1000);
    processGIFFile(pathname);
  }
}
