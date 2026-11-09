#include "BlueMagicCameraConnection.h"

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_store.h>
#endif

BLERemoteCharacteristic *BlueMagicCameraConnection::_cameraStatus;
BLERemoteCharacteristic *BlueMagicCameraConnection::_deviceName;
BLERemoteCharacteristic *BlueMagicCameraConnection::_timecode;
BLERemoteCharacteristic *BlueMagicCameraConnection::_outgoingCameraControl;
BLERemoteCharacteristic *BlueMagicCameraConnection::_incomingCameraControl;

static BLEUUID OutgoingCameraControl("5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB");
static BLEUUID IncomingCameraControl("B864E140-76A0-416A-BF30-5876504537D9");
static BLEUUID Timecode("6D8F2110-86F1-41BF-9AFB-451D87E976C8");
static BLEUUID CameraStatus("7FE8691D-95DC-4FC5-8ABD-CA74339B51B9");
static BLEUUID DeviceName("FFAC0C52-C9FB-41A0-B063-CC76282EB89C");

static BLEUUID ServiceId("00001800-0000-1000-8000-00805f9b34fb");
static BLEUUID BmdCameraService("291D567A-6D75-11E6-8B77-86F30CA893D3");

static void cameraStatusNotify(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  if (length < 1)
  {
    return;
  }
  BlueMagicState *blu = BlueMagicState::getInstance();
  blu->statusNotify(true, pData, length);
  blu->setCameraStatus(pData[0]);
}

static void timeCodeNotify(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  if (length < 12)
  {
    return;
  }
  BlueMagicState *blu = BlueMagicState::getInstance();
  blu->timecodeNotify(true, pData, length);
  // timecode
  uint8_t H, M, S, f;
  H = (pData[11] / 16 * 10) + (pData[11] % 16);
  M = (pData[10] / 16 * 10) + (pData[10] % 16);
  S = (pData[9] / 16 * 10) + (pData[9] % 16);
  f = (pData[8] / 16 * 10) + (pData[8] % 16);
  blu->setTimecode(H, M, S, f);
}

static void controlNotify(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  BlueMagicState *blu = BlueMagicState::getInstance();
  blu->settingsNotify(true, pData, length);
  bool changed = false;

  if (length < 10 || pData[0] != 255)
  {
    blu->setChanged(changed);
    return;
  }

  // recording
  if (length == 13 && pData[1] == 9 && pData[4] == 10 && pData[5] == 1)
  {
    changed = true;
    int8_t transportMode = pData[8];
    blu->setTransportMode(transportMode);
  }

  //codec
  if (pData[4] == 10 && pData[5] == 0)
  {
    changed = true;
    int8_t codec = pData[8];
    int8_t quality = pData[9];
    blu->setCodec(codec);
    blu->setQuality(quality);
  }

  //resolution + framerate
  if (length >= 17 && pData[4] == 1 && pData[5] == 9)
  {

    changed = true;
    int16_t frL = pData[8];
    int16_t frH = pData[9] << 8;
    int16_t frameRate = frL + frH;

    int16_t sfrL = pData[10];
    int16_t sfrH = pData[11] << 8;
    int16_t sensorRate = sfrL + sfrH;

    int16_t wL = pData[12];
    int16_t wH = pData[13] << 8;
    int16_t width = wL + wH;

    int16_t hL = pData[14];
    int16_t hH = pData[15] << 8;
    int16_t height = hL + hH;

    int8_t flags = pData[16];

    blu->setFrameRate(frameRate);
    blu->setSensorFrameRate(sensorRate);
    blu->setFrameWidth(width);
    blu->setFrameHeight(height);
    blu->setFormatFlags(flags);
  }

  // white balance
  if (length >= 12 && pData[4] == 1 && pData[5] == 2)
  {
    changed = true;
    int16_t wbL = pData[8];
    int16_t wbH = pData[9] << 8;
    int16_t whiteBalance = wbL + wbH;

    int16_t tintL = pData[10];
    int16_t tintH = pData[11] << 8;
    int16_t tint = tintL + tintH;

    blu->setWhiteBalance(whiteBalance);
    blu->setTint(tint);
  }

  // zoom
  if (pData[4] == 0 && pData[5] == 7)
  {
    changed = true;
    int16_t zL = pData[8];
    int16_t zH = pData[9] << 8;
    int16_t zoom = zL + zH;
    blu->setZoom(zoom);
  }

  // aperture
  if (pData[4] == 0 && pData[5] == 2)
  {
    changed = true;
    uint16_t low = pData[8];
    uint16_t high = pData[9] << 8;
    float aperture = sqrt(pow(2, (float(low + high) / 2048.0)));
    blu->setAperture(aperture);
  }

  // iso
  if (pData[4] == 1 && pData[5] == 14)
  {
    changed = true;
    uint16_t low = pData[8];
    uint16_t high = pData[9] << 8;
    int32_t iso = low + high;
    blu->setIso(iso);
  }

  // shutter
  if (pData[4] == 1 && pData[5] == 11)
  {
    changed = true;
    uint16_t low = pData[8];
    uint16_t high = pData[9] << 8;
    int32_t shutter = low + high;
    blu->setShutter(shutter);
  }

  blu->setChanged(changed);
}

struct FoundCamera
{
  BLEAddress address;
  char name[BLUEMAGIC_CAMERA_NAME_LENGTH + 1];
  int rssi;
  bool blackmagicService;
};

static FoundCamera foundCameras[BLUEMAGIC_MAX_FOUND_CAMERAS];
static uint8_t foundCount = 0;
static bool stopAtFirstCamera = false;
static PinCodeCallback pinCodeCallback = nullptr;
static BlueMagicCameraConnection *activeConnection = nullptr;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    BLEAddress address = advertisedDevice.getAddress();
    for (uint8_t i = 0; i < foundCount; i++)
    {
      if (foundCameras[i].address == address)
      {
        update(foundCameras[i], advertisedDevice);
        return;
      }
    }

    if (!advertisedDevice.haveServiceUUID())
    {
      return;
    }

    bool blackmagic = advertisedDevice.isAdvertisingService(BmdCameraService);
    if (!blackmagic && !advertisedDevice.isAdvertisingService(ServiceId))
    {
      return;
    }

    int slot = -1;
    if (foundCount < BLUEMAGIC_MAX_FOUND_CAMERAS)
    {
      slot = foundCount++;
    }
    else if (blackmagic)
    {
      for (uint8_t i = 0; i < foundCount && slot < 0; i++)
      {
        if (!foundCameras[i].blackmagicService)
        {
          slot = i;
        }
      }
    }
    if (slot < 0)
    {
      return;
    }

    FoundCamera &camera = foundCameras[slot];
    camera.address = address;
    camera.name[0] = '\0';
    camera.rssi = -127;
    camera.blackmagicService = blackmagic;
    update(camera, advertisedDevice);

    if (blackmagic && stopAtFirstCamera)
    {
      advertisedDevice.getScan()->stop();
    }
  }

  void update(FoundCamera &camera, BLEAdvertisedDevice &advertisedDevice)
  {
    if (advertisedDevice.haveRSSI())
    {
      camera.rssi = advertisedDevice.getRSSI();
    }
    if (advertisedDevice.haveName() && advertisedDevice.getName().length() > 0)
    {
      strncpy(camera.name, advertisedDevice.getName().c_str(), BLUEMAGIC_CAMERA_NAME_LENGTH);
      camera.name[BLUEMAGIC_CAMERA_NAME_LENGTH] = '\0';
    }
  }
};

static MyAdvertisedDeviceCallbacks advertisedDeviceCallbacks;

class BlueMagicClientCallbacks : public BLEClientCallbacks
{
public:
  BlueMagicClientCallbacks(BlueMagicCameraConnection *connection) : _connection(connection) {}

  void onConnect(BLEClient *client) {}

  void onDisconnect(BLEClient *client)
  {
    if (client == _connection->_client)
    {
      _connection->onCameraDisconnected();
    }
  }

private:
  BlueMagicCameraConnection *_connection;
};

class MySecurity : public BLESecurityCallbacks
{
  uint32_t onPassKeyRequest()
  {
    if (pinCodeCallback)
    {
      return pinCodeCallback();
    }

    // code snippet from jeppo7745 https://www.instructables.com/id/Magic-Button-4k-the-20USD-BMPCC4k-Remote/
    Serial.println("---> PLEASE ENTER 6 DIGIT PIN (end with ENTER) : ");
    int pinCode = 0;
    char ch;
    do
    {
      while (!Serial.available())
      {
        vTaskDelay(1);
      }
      ch = Serial.read();
      if (ch >= '0' && ch <= '9')
      {
        pinCode = pinCode * 10 + (ch - '0');
        Serial.print(ch);
      }
    } while ((ch != '\n'));
    return pinCode;
  }

  void onPassKeyNotify(uint32_t pass_key)
  {
    pass_key + 1;
  }

  bool onConfirmPIN(uint32_t pin)
  {
    return true;
  }

  bool onSecurityRequest()
  {
    return true;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t result)
  {
    if (activeConnection != nullptr)
    {
      bool keyMissing = !result.success && (result.fail_reason == ESP_AUTH_SMP_ENC_FAIL || result.fail_reason == 0x06);
      activeConnection->onAuthenticationResult(result.success, result.fail_reason, keyMissing);
    }
  }
#else
  void onAuthenticationComplete(ble_gap_conn_desc *desc, int status)
  {
    if (activeConnection != nullptr)
    {
      bool keyMissing = status == BLE_HS_ERR_HCI_BASE + BLE_ERR_PINKEY_MISSING;
      activeConnection->onAuthenticationResult(status == 0, status, keyMissing);
    }
  }
#endif
};

BlueMagicCameraConnection::BlueMagicCameraConnection()
{
}

BlueMagicCameraConnection::~BlueMagicCameraConnection()
{
  delete _client;
  delete _clientCallbacks;
  delete _cameraControl;
  if (_ownsPref)
  {
    delete _pref;
  }
  _init = false;
  BLEDevice::deinit(true);
}

void BlueMagicCameraConnection::begin()
{
  begin("BlueMagic32");
}

void BlueMagicCameraConnection::begin(String name, Preferences &pref)
{
  _pref = &pref;
  begin(name);
}

void BlueMagicCameraConnection::begin(String name)
{

  if (_init)
  {
    return;
  }

  _name = name;
  setState(CAMERA_DISCONNECTED);

  if (_pref == nullptr)
  {
    _pref = new Preferences();
    _ownsPref = true;
  }

  activeConnection = this;
  loadCameras();
  migrateLegacyPairing();

  BLEDevice::init(_name);
  BLEDevice::setPower(ESP_PWR_LVL_P9);
#if defined(CONFIG_BLUEDROID_ENABLED)
  BLESecurity::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
#endif
  BLEDevice::setSecurityCallbacks(new MySecurity());

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  pSecurity->setCapability(ESP_IO_CAP_IN);
  pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  _init = true;
}

bool BlueMagicCameraConnection::scan(bool active, int duration)
{
  if (_cameraCount > 0)
  {
    return false;
  }
  runScan(active, duration, true);
  return true;
}

int BlueMagicCameraConnection::scanForCameras(uint32_t seconds)
{
  runScan(true, seconds, false);
  return foundCount;
}

void BlueMagicCameraConnection::runScan(bool active, uint32_t seconds, bool stopAtFirst)
{
  foundCount = 0;
  stopAtFirstCamera = stopAtFirst;
  _bleScan = BLEDevice::getScan();
  _bleScan->clearResults();
  _bleScan->setAdvertisedDeviceCallbacks(&advertisedDeviceCallbacks);
  _bleScan->setActiveScan(active);
  _bleScan->start(seconds);
  _bleScan->clearResults();

  bool anyBlackmagic = false;
  for (uint8_t i = 0; i < foundCount; i++)
  {
    anyBlackmagic |= foundCameras[i].blackmagicService;
  }
  if (anyBlackmagic)
  {
    uint8_t kept = 0;
    for (uint8_t i = 0; i < foundCount; i++)
    {
      if (foundCameras[i].blackmagicService)
      {
        foundCameras[kept++] = foundCameras[i];
      }
    }
    foundCount = kept;
  }

  for (uint8_t i = 1; i < foundCount; i++)
  {
    FoundCamera camera = foundCameras[i];
    int j = i - 1;
    while (j >= 0 && foundCameras[j].rssi < camera.rssi)
    {
      foundCameras[j + 1] = foundCameras[j];
      j--;
    }
    foundCameras[j + 1] = camera;
  }
}

uint8_t BlueMagicCameraConnection::foundCameraCount()
{
  return foundCount;
}

String BlueMagicCameraConnection::foundCameraName(uint8_t index)
{
  if (index >= foundCount)
  {
    return "";
  }
  if (foundCameras[index].name[0] != '\0')
  {
    return String(foundCameras[index].name);
  }
  return foundCameras[index].address.toString();
}

String BlueMagicCameraConnection::foundCameraAddress(uint8_t index)
{
  return index < foundCount ? foundCameras[index].address.toString() : String("");
}

int BlueMagicCameraConnection::foundCameraRssi(uint8_t index)
{
  return index < foundCount ? foundCameras[index].rssi : 0;
}

bool BlueMagicCameraConnection::foundCameraIsStored(uint8_t index)
{
  return index < foundCount && findCamera(foundCameras[index].address) >= 0;
}

int BlueMagicCameraConnection::connected()
{
  return _connected;
}

bool BlueMagicCameraConnection::available()
{
  return connected() == CAMERA_CONNECTED && (_cameraControl != nullptr);
}

int BlueMagicCameraConnection::connectedIndex()
{
  return available() ? _connectedIndex : -1;
}

void BlueMagicCameraConnection::setPinCodeCallback(PinCodeCallback callback)
{
  pinCodeCallback = callback;
}

void BlueMagicCameraConnection::setConnectTimeout(uint32_t milliseconds)
{
  _connectTimeout = milliseconds;
}

void BlueMagicCameraConnection::setMaxAuthFailures(uint8_t failures)
{
  _maxAuthFailures = failures;
}

void BlueMagicCameraConnection::onAuthenticationResult(bool success, int reason, bool keyMissing)
{
  int index = _pendingIndex;
  if (index < 0 || index >= _cameraCount)
  {
    return;
  }
  if (success)
  {
    _authFailures[index] = 0;
    return;
  }
  Serial.printf("Authentication with camera %d failed, reason %d (0x%x)%s\n", index, reason, reason,
                keyMissing ? ", camera no longer knows this device" : "");
  if (keyMissing && _authFailures[index] < 255)
  {
    _authFailures[index] = _authFailures[index] + 1;
  }
}

void BlueMagicCameraConnection::onCameraDisconnected()
{
  setState(CAMERA_DISCONNECTED);
  _connectedIndex = -1;
  _outgoingCameraControl = nullptr;
  _incomingCameraControl = nullptr;
  _timecode = nullptr;
  _cameraStatus = nullptr;
  _deviceName = nullptr;
  if (_cameraControl != nullptr)
  {
    _cameraControl->_cameraControl = nullptr;
  }
}

bool BlueMagicCameraConnection::connectToServer(BLEAddress address)
{
  if (_client != nullptr)
  {
    delete _client;
  }
  if (_clientCallbacks == nullptr)
  {
    _clientCallbacks = new BlueMagicClientCallbacks(this);
  }
  _client = BLEDevice::createClient();
  _client->setClientCallbacks(_clientCallbacks);
  setState(CAMERA_CONNECTING);
  if (!_client->connect(address, 0xFF, _connectTimeout))
  {
    Serial.println("Failed to connect to camera");
    setState(CAMERA_DISCONNECTED);
    return false;
  }

  BLERemoteService *pRemoteService = _client->getService(BmdCameraService);
  if (pRemoteService == nullptr)
  {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(BmdCameraService.toString().c_str());
    _client->disconnect();
    setState(CAMERA_DISCONNECTED);
    return false;
  }

  _deviceName = pRemoteService->getCharacteristic(DeviceName);
  if (_deviceName != nullptr)
  {
    _deviceName->writeValue(_name.c_str(), _name.length());
  }

  _outgoingCameraControl = pRemoteService->getCharacteristic(OutgoingCameraControl);
  if (_outgoingCameraControl == nullptr)
  {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(OutgoingCameraControl.toString().c_str());
    _client->disconnect();
    setState(CAMERA_DISCONNECTED);
    return false;
  }

  _incomingCameraControl = pRemoteService->getCharacteristic(IncomingCameraControl);
  if (_incomingCameraControl == nullptr)
  {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(IncomingCameraControl.toString().c_str());
    _client->disconnect();
    setState(CAMERA_DISCONNECTED);
    return false;
  }
  _incomingCameraControl->registerForNotify(controlNotify, false);

  _timecode = pRemoteService->getCharacteristic(Timecode);
  if (_timecode == nullptr)
  {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(Timecode.toString().c_str());
    _client->disconnect();
    setState(CAMERA_DISCONNECTED);
    return false;
  }
  _timecode->registerForNotify(timeCodeNotify);

  _cameraStatus = pRemoteService->getCharacteristic(CameraStatus);
  if (_cameraStatus == nullptr)
  {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(CameraStatus.toString().c_str());
    _client->disconnect();
    setState(CAMERA_DISCONNECTED);
    return false;
  }
  _cameraStatus->registerForNotify(cameraStatusNotify);

  setState(CAMERA_CONNECTED);
  setController();
  return true;
}

void BlueMagicCameraConnection::setController()
{
  if (_cameraControl == nullptr)
  {
    _cameraControl = new BlueMagicCameraController(_outgoingCameraControl);
  }
  else
  {
    _cameraControl->_cameraControl = _outgoingCameraControl;
  }
}

void BlueMagicCameraConnection::setState(CONNECTION_STATE state)
{
  _connected = state;
}

void BlueMagicCameraConnection::loadCameras()
{
  _cameraCount = 0;
  _useCounter = 0;
  _pref->begin(_name.c_str(), false);
  uint8_t count = _pref->getUChar("camcount", 0);
  if (_pref->getUChar("camversion", 0) == 1 && count <= BLUEMAGIC_MAX_CAMERAS &&
      _pref->getBytesLength("cameras") == count * sizeof(BlueMagicCameraRecord))
  {
    _pref->getBytes("cameras", _cameras, count * sizeof(BlueMagicCameraRecord));
    _cameraCount = count;
    _useCounter = _pref->getULong("camuse", 0);
  }
  _pref->end();
}

void BlueMagicCameraConnection::saveCameras()
{
  _pref->begin(_name.c_str(), false);
  _pref->putUChar("camversion", 1);
  _pref->putUChar("camcount", _cameraCount);
  _pref->putULong("camuse", _useCounter);
  if (_cameraCount > 0)
  {
    _pref->putBytes("cameras", _cameras, _cameraCount * sizeof(BlueMagicCameraRecord));
  }
  else
  {
    _pref->remove("cameras");
  }
  _pref->end();
}

void BlueMagicCameraConnection::migrateLegacyPairing()
{
  _pref->begin(_name.c_str(), false);
  bool hasLegacy = _pref->isKey("cameraAddress") || _pref->isKey("authenticated");
  String address = _pref->getString("cameraAddress", "");
  bool authenticated = _pref->getBool("authenticated", false);
  if (hasLegacy)
  {
    _pref->remove("cameraAddress");
    _pref->remove("authenticated");
  }
  _pref->end();

  if (hasLegacy && authenticated && address.length() == 17 && _cameraCount == 0)
  {
    addCamera(BLEAddress(address), "");
  }
}

BLEAddress BlueMagicCameraConnection::cameraBLEAddress(uint8_t index)
{
  return BLEAddress(String(_cameras[index].address), _cameras[index].addressType);
}

int BlueMagicCameraConnection::findCamera(BLEAddress address)
{
  String text = address.toString();
  for (uint8_t i = 0; i < _cameraCount; i++)
  {
    if (text.equalsIgnoreCase(_cameras[i].address))
    {
      return i;
    }
  }
  return -1;
}

int BlueMagicCameraConnection::lastUsedCamera()
{
  int result = -1;
  for (uint8_t i = 0; i < _cameraCount; i++)
  {
    if (result < 0 || _cameras[i].lastUsed > _cameras[result].lastUsed)
    {
      result = i;
    }
  }
  return result;
}

int BlueMagicCameraConnection::addCamera(BLEAddress address, String name)
{
  int index = findCamera(address);
  if (index < 0)
  {
    if (_cameraCount >= BLUEMAGIC_MAX_CAMERAS)
    {
      return -1;
    }
    index = _cameraCount++;
    BlueMagicCameraRecord &record = _cameras[index];
    memset(&record, 0, sizeof(record));
    strncpy(record.address, address.toString().c_str(), sizeof(record.address) - 1);
    record.addressType = address.getType();
    strncpy(record.name, name.c_str(), BLUEMAGIC_CAMERA_NAME_LENGTH);
  }
  markUsed(index);
  return index;
}

void BlueMagicCameraConnection::markUsed(uint8_t index)
{
  _cameras[index].lastUsed = ++_useCounter;
  saveCameras();
}

BlueMagicCameraController *BlueMagicCameraConnection::connect()
{
  if (available())
  {
    return _cameraControl;
  }

  if (_cameraCount == 0)
  {
    scan(false, 5);
    return foundCount > 0 ? pair(0) : nullptr;
  }

  int last = lastUsedCamera();
  if (connectToCamera(last))
  {
    return _cameraControl;
  }

  if (_cameraCount > 1)
  {
    runScan(false, 3, false);
    for (uint8_t i = 0; i < foundCount; i++)
    {
      int stored = findCamera(foundCameras[i].address);
      if (stored >= 0 && stored != last && connectToCamera(stored))
      {
        return _cameraControl;
      }
    }
  }

  return nullptr;
}

BlueMagicCameraController *BlueMagicCameraConnection::connect(uint8_t index)
{
  if (index >= _cameraCount)
  {
    return nullptr;
  }
  if (available() && _connectedIndex == index)
  {
    return _cameraControl;
  }
  return connectToCamera(index) ? _cameraControl : nullptr;
}

void BlueMagicCameraConnection::disconnect()
{
  if (_client != nullptr && _client->isConnected())
  {
    _client->disconnect();
    unsigned long start = millis();
    while (_client->isConnected() && millis() - start < 2000)
    {
      delay(10);
    }
  }
  onCameraDisconnected();
}

BlueMagicCameraController *BlueMagicCameraConnection::pair(uint8_t foundIndex)
{
  if (foundIndex >= foundCount)
  {
    return nullptr;
  }
  FoundCamera camera = foundCameras[foundIndex];

  int stored = findCamera(camera.address);
  if (stored >= 0)
  {
    return connectToCamera(stored) ? _cameraControl : nullptr;
  }

  if (connected() != CAMERA_DISCONNECTED)
  {
    disconnect();
  }

  if (_cameraCount >= BLUEMAGIC_MAX_CAMERAS)
  {
    int oldest = leastRecentlyUsedCamera();
    Serial.print("Camera list full, forgetting: ");
    Serial.println(cameraName(oldest));
    forgetCamera(oldest);
  }

  BlueMagicState::getInstance()->reset();
  _pendingIndex = -1;
  if (!connectToServer(camera.address))
  {
    return nullptr;
  }

  String name = String(camera.name);
  if (name.length() == 0)
  {
    name = readCameraName();
  }
  _connectedIndex = addCamera(camera.address, name);
  if (_connectedIndex >= 0)
  {
    _authFailures[_connectedIndex] = 0;
  }
  _pendingIndex = _connectedIndex;
  return _cameraControl;
}

bool BlueMagicCameraConnection::connectToCamera(uint8_t index)
{
  if (index >= _cameraCount)
  {
    return false;
  }
  if (connected() != CAMERA_DISCONNECTED)
  {
    disconnect();
  }
  if (_maxAuthFailures > 0 && _authFailures[index] >= _maxAuthFailures)
  {
    Serial.print("Pairing again with camera: ");
    Serial.println(cameraName(index));
    removeBond(index);
    _authFailures[index] = 0;
  }
  BlueMagicState::getInstance()->reset();
  _pendingIndex = index;
  if (!connectToServer(cameraBLEAddress(index)))
  {
    return false;
  }
  _connectedIndex = index;
  markUsed(index);
  return true;
}

String BlueMagicCameraConnection::readCameraName()
{
  if (_client == nullptr || !_client->isConnected())
  {
    return "";
  }
  BLERemoteService *service = _client->getService(BLEUUID((uint16_t)0x1800));
  if (service == nullptr)
  {
    return "";
  }
  BLERemoteCharacteristic *characteristic = service->getCharacteristic(BLEUUID((uint16_t)0x2A00));
  if (characteristic == nullptr || !characteristic->canRead())
  {
    return "";
  }
  String name = characteristic->readValue();
  name.trim();
  return name;
}

int BlueMagicCameraConnection::leastRecentlyUsedCamera()
{
  int result = -1;
  for (uint8_t i = 0; i < _cameraCount; i++)
  {
    if (i != _connectedIndex && (result < 0 || _cameras[i].lastUsed < _cameras[result].lastUsed))
    {
      result = i;
    }
  }
  return result < 0 ? 0 : result;
}

uint8_t BlueMagicCameraConnection::cameraCount()
{
  return _cameraCount;
}

String BlueMagicCameraConnection::cameraName(uint8_t index)
{
  if (index >= _cameraCount)
  {
    return "";
  }
  if (_cameras[index].name[0] != '\0')
  {
    return String(_cameras[index].name);
  }
  return String(_cameras[index].address);
}

String BlueMagicCameraConnection::cameraAddress(uint8_t index)
{
  return index < _cameraCount ? String(_cameras[index].address) : String("");
}

bool BlueMagicCameraConnection::renameCamera(uint8_t index, String name)
{
  if (index >= _cameraCount)
  {
    return false;
  }
  memset(_cameras[index].name, 0, sizeof(_cameras[index].name));
  strncpy(_cameras[index].name, name.c_str(), BLUEMAGIC_CAMERA_NAME_LENGTH);
  saveCameras();
  return true;
}

void BlueMagicCameraConnection::forgetCamera(uint8_t index)
{
  if (index >= _cameraCount)
  {
    return;
  }
  if (_connectedIndex == index)
  {
    disconnect();
  }
  removeBond(index);

  for (uint8_t i = index; i + 1 < _cameraCount; i++)
  {
    _cameras[i] = _cameras[i + 1];
    _authFailures[i] = _authFailures[i + 1];
  }
  _cameraCount--;
  _authFailures[_cameraCount] = 0;
  _pendingIndex = -1;
  if (_connectedIndex > index)
  {
    _connectedIndex--;
  }
  saveCameras();
}

void BlueMagicCameraConnection::removeBond(uint8_t index)
{
  BLEAddress address = cameraBLEAddress(index);
#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_remove_bond_device(address.getNative());
#else
  ble_addr_t peer;
  peer.type = _cameras[index].addressType;
  memcpy(peer.val, address.getNative(), sizeof(peer.val));
  ble_store_util_delete_peer(&peer);
#endif
}

void BlueMagicCameraConnection::clearPairing()
{
  if (connected() != CAMERA_DISCONNECTED)
  {
    disconnect();
  }
#if defined(CONFIG_BLUEDROID_ENABLED)
  int dev_num = esp_ble_get_bond_device_num();

  esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
  esp_ble_get_bond_device_list(&dev_num, dev_list);
  for (int i = 0; i < dev_num; i++)
  {
    BLEAddress baddr = BLEAddress(dev_list[i].bd_addr);
    Serial.println(baddr.toString().c_str());
    esp_ble_remove_bond_device(dev_list[i].bd_addr);
  }

  free(dev_list);
#else
  ble_store_clear();
#endif
  _cameraCount = 0;
  for (uint8_t i = 0; i < BLUEMAGIC_MAX_CAMERAS; i++)
  {
    _authFailures[i] = 0;
  }
  _pendingIndex = -1;
  saveCameras();
}
