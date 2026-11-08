
#ifndef BLEDevice_h
#include <BLEDevice.h>
#endif

#include <Preferences.h>

#ifndef BlueMagicCameraConnection_h

#define BlueMagicCameraConnection_h

#include "Arduino.h"
#include "BlueMagicState.h"
#include "BlueMagicCameraController.h"

enum CONNECTION_STATE
{
  CAMERA_CONNECTED = 1,
  CAMERA_DISCONNECTED = 2,
  CAMERA_CONNECTING = 3
};

class BlueMagicCameraConnection
{

public:
  BlueMagicCameraConnection();
  ~BlueMagicCameraConnection();

  void begin();
  void begin(String name);
  void begin(String name, Preferences &pref);

  bool scan(bool active, int duration);

  BlueMagicCameraController *connect();
  BlueMagicCameraController *connect(uint8_t index);

  void disconnect();

  void clearPairing();

  bool available();

private:
  String _name;
  Preferences *_pref = nullptr;
  bool _init = false;

  bool _authenticated;
  int _connected;

  BLEClient *_client = nullptr;

  BLEAddress _cameraAddress;
  bool _hasCameraAddress = false;

  static BLERemoteCharacteristic *_outgoingCameraControl;
  static BLERemoteCharacteristic *_incomingCameraControl;
  static BLERemoteCharacteristic *_timecode;
  static BLERemoteCharacteristic *_cameraStatus;
  static BLERemoteCharacteristic *_deviceName;

  BLEScan *_bleScan;

  BlueMagicCameraController *_cameraControl = nullptr;

  bool connectToServer(BLEAddress address);

  int connected();
  bool authenticated();

  void setController();

  void setState(CONNECTION_STATE state);
  void setAuthentication(bool authenticated);
  bool getAuthentication();
  void setCameraAddress(BLEAddress address);
  BLEAddress *getCameraAddress();
};

#endif
