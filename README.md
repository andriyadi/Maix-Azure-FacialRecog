# Maix-Azure-FacialRecog
Facial recognition by combining the power of AI at the Edge and in the Cloud. Face detection is done locally, right on K210 Microcontroller on Sipeed Maix dev board, developed with Maixduino framework. Then Azure Cognitive Services - Face API in the Cloud is leveraged for age, gender, emotion recognition.

This project is fully tested with **Sipeed Maix Go** board. With minor changes, you could use other Sipeed Maix boards or other K210-powered boards. Since the board needs to connect to internet, the board must have WiFi module on-board.

## Demo
I made and published the demo video on YouTube long time ago. Just found the time to clean up the code and publish here. 

Click the image to view video.

[![Demo video thumbnail](http://i3.ytimg.com/vi/vvQfYqrUwFg/hqdefault.jpg)](https://www.youtube.com/watch?v=vvQfYqrUwFg)

## Prerequisites
* **Sipeed Maix Go** dev board or compatible one
* [PlatformIO](http://platformio.org/)
* [platform-kendryte210](https://github.com/sipeed/platform-kendryte210). Should be installed automatically when you build the project with PlatformIO
* If you're like me, I'll use VSCode and install PlatformIO extension.

## Getting Started
* Prepare subscription key for Azure Cognitive Services - Face API. Follow [this instruction](https://azure.microsoft.com/en-us/try/cognitive-services/?api=face-api)
* Clone this repo: git clone https://github.com/andriyadi/Maix-Azure-FacialRecog.git
* Create `Secrets.h` file inside `src` folder. Explained below.
* Open the project with your fav IDE, I used VSCode. Then build with PlatformIO

## `Secrets.h` file
Under `src` folder, create a file named `Secrets.h` with the content something like this:
```
#define FACE_VISION_API_HOST    "<FaceAPI-EndPoint-Host>" 
#define FACE_VISION_API_KEY     "<FaceAPI-Key1>"

#define FACE_VISION_FACE_ATTRS  "age,gender,smile,facialHair,glasses,emotion"
#define FACE_VISION_API_PATH    "/face/v1.0/detect?returnFaceId=true&returnFaceLandmarks=false&returnFaceAttributes=" FACE_VISION_FACE_ATTRS

#define WIFI_SSID_NAME "<SSID_NAME>"        // your network SSID (name)
#define WIFI_SSID_PASS "<SSID_PASSWORD>"    // your network password
```

Replace all values with format of `<...>` inside quote.


## Credits
* [ArduinoHttpClient](https://github.com/arduino-libraries/ArduinoHttpClient.git)
* JPEG Encoder. Found it somewhere, but so sorry I honestly forgot. Tell me the link if anyone know.
