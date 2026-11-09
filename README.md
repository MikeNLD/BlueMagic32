# BlueMagic32

![](https://img.shields.io/badge/Version-1.2.0-green.svg) ![](https://img.shields.io/badge/BlackmagicCameraControl-1.3-green.svg) ![](https://img.shields.io/badge/arduino--esp32-3.x-green.svg)

Arduino ESP32 Library for connecting to Blackmagic Cameras using Bluetooth Low Energy.

Control/Read camera parameters like Recording, Focus, Aperture, Shutter Angle, White balance, etc...

#### Tested With BlackMagic Pocket Cinema Camera 4K

# Background

[This project's roots start way back in January 2019](https://forum.blackmagicdesign.com/viewtopic.php?f=2&t=82040#p473595)

Bluetooth Camera control apps are great! but whether you need a physical/tactile button as a record trigger to use on your rig or you're creating your own custom camera control solution, apps can have their limits. But no more!

ESP32 is a micro-controller from Espressif that can be found in lots of low-cost / pre-made designs and has features like WiFi and Bluetooth!

This library should work with vrtiually any ESP32 device, but for those stepping into the scene for the first time I would highly recommend the [M5StickC](https://m5stack.com/products/stick-c) as this development device comes with a builtin display, buttons, battery and lots more!

[![Early Demo](https://i.vimeocdn.com/video/835307994-15ec876a9098c07fe9444ae9f01e86f0136dbbaa29adf732a05264fbdab01533-d?mw=1700&mh=956&q=70)](https://vimeo.com/376290054)


![esp32](docs/devices.jpg)

# Getting Started

## Installation

### Video 

Demonstration of how to install all the required software components and simple real world demo. 

[![Install + Usage Demo](https://i.vimeocdn.com/video/848581621-28580adffc5412a29420a32fc0070ae7e90a2ceac4e7a4f7e02ae12443ba31c1-d?mw=1700&mh=956&q=70)](https://vimeo.com/386151760)


### Prerequisites

You will need to install these software packages first. Get the latest version of the Arduino IDE ( version 1.8 or later ) and the ESP32 Arduino library ( version 3.0 or later ). Boards with Bluetooth Low Energy are supported: ESP32, ESP32-S3, ESP32-C3, ESP32-C5, ESP32-C6 and ESP32-H2. Below are download links / guides on how to install them.

- [Arduino IDE](https://www.arduino.cc/en/main/software)

- [ESP32 Arduino Library](https://github.com/espressif/arduino-esp32/blob/master/docs/arduino-ide/boards_manager.md)

If using an M5StickC you also need to install their library:

- [M5StickC Library](https://github.com/m5stack/M5StickC)

### Installing BlueMagic32

Download this repository as a .zip file and add it to the Arduino Library.

In the Arduino IDE in the toolbar go to:

- _Sketch > Include Library > Add .ZIP Library_

## Usage

#### Setup & First Connection

Include the library in your sketch

```
#include <BlueMagic32.h>
```

Create a connection with a device name ( here we use "BlueMagic32" it will show up as the device connected to the camera.) and get the controller object, this should be done in your setup(). We also start Serial here for later use.

```
void setup() {

    Serial.begin(115200);
    BMDConnection.begin("BlueMagic32");
    BMDControl = BMDConnection.connect();

}
```

Upon your first connection, you will need to enter the passcode displayed on your camera. To do this we will open the built in Serial Monitor by going to:

- _Tools > Serial Monitor_

Make sure the baud rate is the same as what we have declared in the setup(), which in this case is: 115200

- _115200 baud_

Now also make sure line ending is set to: Newline

- _Newline_

When the camera displays the passcode, enter that code into the input box at the top and click send. Afterwards the ESP32 and the camera should now be bonded and will remember each other, so when you reboot/ power cycle your ESP32 it will autoconnect to the camera.

#### Write Settings

Upon successful connection, we can now use the controller like this:

we use the **available()** method to make sure we only read/write when a successful connection is established. Any calls to BMDControl should be wrapped in this method to make sure it is safe to call.

```
if (BMDConnection.available()) {

    // Starts recording
    BMDControl->record(true);

    // Sets ISO to 1250
    BMDControl->iso(1250);

    // Sets absolute focus, 0.0 near -> 1.0 far
    BMDControl->focus(0.5);

    // Sets white balance to 5600k
    BMDControl->whiteBalance(5600);

}
```

#### Read Settings

To read camera settings live, we can use the **changed()** method to only update our variables when the camera settings change:

```
if(BMDControl->changed()){

    // Get recording state
    bool recording = BMDControl->isRecording();

    // Get the current shutter setting ( as shutter angle )
    int shutter = BMDControl->getShutter();

    // Get the current frame rate setting
    int frameRate = BMDControl->getFrameRate();

    // Get the current iso setting
    int iso = BMDControl->getIso();

}
```

#### Timecode

To get live timecode:

```
// Get the current timecode as a string
String tc = BMDControl->timecode();

// Get the current timecode as a number
long tc = BMDControl->timecodeRaw();
```

#### Custom Commands

For advanced users, you can use the **custom()** method to send your own data array for camera control:

```
// send the start recording command
uint8_t data[12] = {255, 5, 0, 0, 10, 1, 1, 0, 2, 0, 0, 0};
BMDControl->custom(data, 12);
```

#### Connection Management

`connect()` pairs the strongest camera in range the first time and afterwards reconnects to the most recently used camera. If that camera is not reachable, the other stored cameras in range are tried.

#### Multiple Cameras

Several cameras can be stored: 3 on ESP32-S3/C3/C5/C6/H2 and 8 on the original ESP32. When the list is full, pairing a new camera forgets the least recently used one.

```
int found = BMDConnection.scanForCameras(5);      // scan for 5 seconds
for (int i = 0; i < found; i++) {
    Serial.println(BMDConnection.foundCameraName(i));
}
BMDControl = BMDConnection.pair(0);                // pair and store found camera 0

BMDControl = BMDConnection.connect(1);             // connect to stored camera 1
BMDConnection.forgetCamera(1);                     // forget stored camera 1
```

The controller object stays the same when switching cameras.

To show which camera asks for its PIN, for example on a display, set a callback before `begin()`:

```
BMDConnection.setPinCodeCallback([](const String &name, const String &address) -> uint32_t {
    return readPinFromButtons(name);
});
```

When a camera no longer knows the ESP32 (its pairing was removed on the camera), the ESP32 pairs again after 3 failed attempts. Change this with `setMaxAuthFailures(n)`; 0 disables it.

To forget all cameras call `BMDConnection.clearPairing();` and clear the paired devices on your cameras.

## API Reference

#### Connection Methods

```
  void begin();
  void begin(String name);

  void begin(String name, Preferences &pref);

  bool scan(bool active, int duration);

  int scanForCameras(uint32_t seconds = 5);
  uint8_t foundCameraCount();
  String foundCameraName(uint8_t index);
  String foundCameraAddress(uint8_t index);
  int foundCameraRssi(uint8_t index);
  bool foundCameraIsStored(uint8_t index);

  BlueMagicCameraController *pair(uint8_t foundIndex);

  BlueMagicCameraController *connect();
  BlueMagicCameraController *connect(uint8_t index);

  void disconnect();

  void clearPairing();

  uint8_t cameraCount();
  String cameraName(uint8_t index);
  String cameraAddress(uint8_t index);
  bool renameCamera(uint8_t index, String name);
  void forgetCamera(uint8_t index);

  bool available();
  int connectedIndex();

  void setPinCodeCallback(PinCodeCallback callback);
  void setPinCodeCallback(CameraPinCodeCallback callback);
  void setConnectTimeout(uint32_t milliseconds);
  void setMaxAuthFailures(uint8_t failures);
```

#### Controller Methods

```
  bool changed();

  void setCamera(uint8_t index);
  void custom(uint8_t *data, size_t len);

  uint8_t getCameraStatus();
  int8_t getTransportMode();

  void record(bool recording);
  void toggleRecording();
  bool isRecording();

  void play(bool play);
  bool isPlaying();

  void preview(bool preview);
  bool isPreviewing();

  void codec(CODEC_TYPE c, CODEC_QUALITY q);
  int8_t getCodecType();
  int8_t getCodecQuality();

  void focus(float value);
  float getFocus();
  void instantAutoFocus();

  void ois(bool enabled);
  bool getOis();

  void zoom(float value);
  float getZoom();

  void aperture(float value);
  void autoAperture();
  float getAperture();

  void autoExposureMode(int value);

  void iso(int32_t iso);
  int32_t getIso();

  void shutterSpeed(int32_t value);
  void shutterAngle(int32_t value);
  int32_t getShutter();

  void whiteBalance(int16_t whiteBalance, int16_t tint);
  void autoWhiteBalance();
  int16_t getWhiteBalance();
  int16_t getTint();

  void frameRate(int16_t frameRate);
  int16_t getFrameRate();

  void sensorFrameRate(int16_t sensorFrameRate);
  int16_t getSensorFrameRate();

  void resolution(int16_t width, int16_t height);
  int16_t getFrameWidth();
  int16_t getFrameHeight();

  String timecode();
  uint32_t timecodeRaw();


  // for advanced users looking to see raw data streaming from the ble characteristics, use these methods:

  bool settingsChanged();
  uint8_t *settingsData();

  bool timecodeChanged();
  uint8_t *timecodeData();

  bool statusChanged();
  uint8_t *statusData();

```

## Examples

Included in the library are a couple of basic examples to get you up and running. These are located in the examples folder and can be found in the Arduino IDE by going to:

- _File > Examples > BlueMagic32_

Open them, adjust yoru board settings and upload!

### Basic Remote Trigger

Start/Stop recording using the built-in boot button.

![basic-trigger](docs/basic.jpg)

### Multi Camera

Scan, pair, switch between and forget cameras from the Serial Monitor.

### M5StickC Trigger with Timecode

Start/Stop recording using the builtin button with live timecode display.

![timecode-trigger](docs/timecode.jpg)

### M5StickC Settings Display

Display live camera settings on the builtin screen.

![settings](docs/settings.jpg)

# Donation

If this project helped you and you want support more cool projects like it:

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=ZDU59MDJ3VXF6&source=url)

# Acknowledgments

- BlackMagic Design's awesome [SDK](https://www.blackmagicdesign.com/developer/product/camera)!
- [nkolban](https://github.com/nkolban/esp32-snippets/tree/master/cpp_utils/tests/BLETests/Arduino) BLE examples
