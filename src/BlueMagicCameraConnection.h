
#ifndef BLEDevice_h
#include <BLEDevice.h>
#endif

#include <Preferences.h>
#include <functional>

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

#ifndef BLUEMAGIC_MAX_CAMERAS
#if defined(CONFIG_NIMBLE_ENABLED) && defined(CONFIG_BT_NIMBLE_MAX_BONDS)
#define BLUEMAGIC_MAX_CAMERAS CONFIG_BT_NIMBLE_MAX_BONDS
#elif defined(CONFIG_NIMBLE_ENABLED)
#define BLUEMAGIC_MAX_CAMERAS 3
#else
#define BLUEMAGIC_MAX_CAMERAS 8
#endif
#endif

#define BLUEMAGIC_CAMERA_NAME_LENGTH 32
#define BLUEMAGIC_MAX_FOUND_CAMERAS 8

struct BlueMagicCameraRecord
{
  char address[18];
  uint8_t addressType;
  char name[BLUEMAGIC_CAMERA_NAME_LENGTH + 1];
  uint32_t lastUsed;
};

typedef std::function<uint32_t()> PinCodeCallback;

class BlueMagicClientCallbacks;

class BlueMagicCameraConnection
{

public:
  BlueMagicCameraConnection();
  ~BlueMagicCameraConnection();

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
  void setConnectTimeout(uint32_t milliseconds);

private:
  friend class BlueMagicClientCallbacks;

  String _name;
  Preferences *_pref = nullptr;
  bool _ownsPref = false;
  bool _init = false;

  volatile int _connected = CAMERA_DISCONNECTED;

  BLEClient *_client = nullptr;
  BlueMagicClientCallbacks *_clientCallbacks = nullptr;

  BlueMagicCameraRecord _cameras[BLUEMAGIC_MAX_CAMERAS];
  uint8_t _cameraCount = 0;
  uint32_t _useCounter = 0;
  int _connectedIndex = -1;
  uint32_t _connectTimeout = 10000;

  static BLERemoteCharacteristic *_outgoingCameraControl;
  static BLERemoteCharacteristic *_incomingCameraControl;
  static BLERemoteCharacteristic *_timecode;
  static BLERemoteCharacteristic *_cameraStatus;
  static BLERemoteCharacteristic *_deviceName;

  BLEScan *_bleScan = nullptr;

  BlueMagicCameraController *_cameraControl = nullptr;

  bool connectToServer(BLEAddress address);
  void onCameraDisconnected();

  int connected();

  void setController();

  void setState(CONNECTION_STATE state);

  void loadCameras();
  void saveCameras();
  void migrateLegacyPairing();
  BLEAddress cameraBLEAddress(uint8_t index);
  int findCamera(BLEAddress address);
  int lastUsedCamera();
  int addCamera(BLEAddress address, String name);
  void markUsed(uint8_t index);
  void removeBond(uint8_t index);
  int leastRecentlyUsedCamera();
  bool connectToCamera(uint8_t index);
  String readCameraName();
  void runScan(bool active, uint32_t seconds, bool stopAtFirstCamera);
};

#endif
