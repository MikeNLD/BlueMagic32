#include <BlueMagic32.h>

// Serial monitor commands (115200 baud, newline line ending):
//   s      scan for cameras
//   l      list stored cameras
//   p<n>   pair found camera n
//   c<n>   connect to stored camera n
//   f<n>   forget stored camera n
//   r      toggle recording
//   d      disconnect
//   x      forget all cameras

uint32_t readPin(const String &cameraName, const String &cameraAddress)
{
  Serial.printf("Enter the 6 digit PIN shown on %s (%s): ", cameraName.c_str(), cameraAddress.c_str());
  uint32_t pin = 0;
  char ch = 0;
  while (ch != '\n')
  {
    while (!Serial.available())
    {
      delay(1);
    }
    ch = Serial.read();
    if (ch >= '0' && ch <= '9')
    {
      pin = pin * 10 + (ch - '0');
      Serial.print(ch);
    }
  }
  Serial.println();
  return pin;
}

void listStored()
{
  Serial.printf("%d stored camera(s):\n", BMDConnection.cameraCount());
  for (int i = 0; i < BMDConnection.cameraCount(); i++)
  {
    Serial.printf("  %d: %s (%s)%s\n", i, BMDConnection.cameraName(i).c_str(), BMDConnection.cameraAddress(i).c_str(),
                  BMDConnection.connectedIndex() == i ? " - connected" : "");
  }
}

void scan()
{
  Serial.println("Scanning...");
  int found = BMDConnection.scanForCameras(5);
  Serial.printf("%d camera(s) found:\n", found);
  for (int i = 0; i < found; i++)
  {
    Serial.printf("  %d: %s (%s) %d dBm%s\n", i, BMDConnection.foundCameraName(i).c_str(), BMDConnection.foundCameraAddress(i).c_str(),
                  BMDConnection.foundCameraRssi(i), BMDConnection.foundCameraIsStored(i) ? " - stored" : "");
  }
}

void report(BlueMagicCameraController *control)
{
  if (control != nullptr)
  {
    BMDControl = control;
    Serial.printf("Connected to camera %d: %s\n", BMDConnection.connectedIndex(), BMDConnection.cameraName(BMDConnection.connectedIndex()).c_str());
  }
  else
  {
    Serial.println("Connection failed");
  }
}

void setup()
{
  Serial.begin(115200);
  BMDConnection.setPinCodeCallback(readPin);
  BMDConnection.begin("BlueMagic32");
  listStored();
  Serial.println("Commands: s, l, p<n>, c<n>, f<n>, r, d, x");
}

void loop()
{
  if (!Serial.available())
  {
    return;
  }

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0)
  {
    return;
  }
  char command = line.charAt(0);
  int index = line.substring(1).toInt();

  switch (command)
  {
  case 's':
    scan();
    break;
  case 'l':
    listStored();
    break;
  case 'p':
    report(BMDConnection.pair(index));
    break;
  case 'c':
    report(BMDConnection.connect(index));
    break;
  case 'f':
    BMDConnection.forgetCamera(index);
    listStored();
    break;
  case 'r':
    if (BMDConnection.available())
    {
      BMDControl->toggleRecording();
    }
    else
    {
      Serial.println("Not connected");
    }
    break;
  case 'd':
    BMDConnection.disconnect();
    Serial.println("Disconnected");
    break;
  case 'x':
    BMDConnection.clearPairing();
    listStored();
    break;
  default:
    Serial.println("Commands: s, l, p<n>, c<n>, f<n>, r, d, x");
  }
}
