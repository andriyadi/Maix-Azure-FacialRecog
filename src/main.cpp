
#include <Arduino.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __cplusplus
extern "C"
{
#endif
  #include "face/gencode_output.h"
  #include "face/region_layer.h"
  #include "jpeg/jpeg.h"
#ifdef __cplusplus
}
#endif

#include "bsp.h"
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <Sipeed_ST7789.h>
#include "camera/MyCamera.hpp"

#include "Constants.h"
#include "Secrets.h"
#include "FaceVisionClient.hpp"


#define printf Serial.printf //I'm too lazy to replace all

enum RecognitionState_e
{
  RECOGNITION_STATE_UNKNOWN = 0,
  RECOGNITION_STATE_REQUESTED = 1,
  RECOGNITION_STATE_RECOGNIZING = 2,
  RECOGNITION_STATE_DONE = 3
};

SPIClass spi_(SPI0); // MUST be SPI0 for Maix series on board LCD
Sipeed_ST7789 lcd(320, 240, spi_);

volatile RecognitionState_e recognitionState = RECOGNITION_STATE_UNKNOWN;
volatile uint32_t lastRecognitionStateChangeMillis = 0;
char recognitionResultString[100];

kpu_task_t task;
static region_layer_t detect_rl;
volatile uint8_t g_ai_done_flag;

FaceVisionClient *cvc = NULL;
int wifiStatus = WL_IDLE_STATUS;

#define KEY_GPIONUM 0
volatile uint8_t g_gpio_key_flag;
volatile uint64_t g_gpio_time = 0;
void irq_key()
{
  g_gpio_key_flag = 1;
  g_gpio_time = sysctl_get_time_us();
}

/**
 * Get camera buffer and compress to JPEG
 */ 
uint8_t jpeg_buffer[1024 * 1024];
static bool camera_snapshot_jpeg(image_t *img_out, int quality) {

  image_t img_src;
  img_src.w = CAMERA_WIDTH;
  img_src.h = CAMERA_HEIGHT;
  img_src.bpp = 2;

  // Get raw bytes from camera
  uint32_t *tmp_buffer = camera_snapshot();
  // reverse_u32pixel((tmp_buffer), img.w * img.h /2);

  img_src.pixels = (uint8_t *)tmp_buffer;

  // image_t img_out = { .w=img_src.w, .h=img_src.h, .bpp=sizeof(buffer), .pixels=buffer };
  img_out->w = img_src.w;
  img_out->h = img_src.h;
  img_out->bpp = sizeof(jpeg_buffer);
  img_out->pixels = jpeg_buffer;

  // jpeg_compress(&img_src, img_out, 80, false);
  bool retVal = jpeg_compress(&img_src, img_out, quality, false);
  
  //dump to serial
  // for(int i = 0; i < img_out.bpp; i++)
  // {
  //   // uarths_putchar(img_out.pixels[i]);
  //   Serial.write(img_out.pixels[i]);
  // }

  printf("JPEG done\n");
  return !retVal;

}

/**
 * Main function to do face processing using Face API. This function runs on a dedicated core
 * 
 */
int recognitionHandler(void *ctx) {

  FaceVisionClient::FaceVisionClientConfig_t cvcConfig = {
      FACE_VISION_API_HOST,
      FACE_VISION_API_PATH,
      FACE_VISION_API_KEY,
      false};

  cvc = new FaceVisionClient(cvcConfig);

  camera_fb_t fb = {};
  FaceVisionClient::FaceVisionDetectionRequest_t req = {};
  req.cameraFb = &fb;
  req.bestPredictionThreshold = 0.6f;

#if CUSTOM_VISION_DEMO
  FaceVisionClient::FaceVisionDetectionResult_t visionResult = {};
#endif  

  FaceVisionClient::FaceAPIDetectionResult_t faceResult = {};

  printf("Waiting for recog request...\n");

  while (1)
  {
    if (g_gpio_key_flag || recognitionState == RECOGNITION_STATE_REQUESTED)
    {
      uint64_t v_time_now = sysctl_get_time_us();
      if (v_time_now - g_gpio_time > 10 * 1000)
      {
        // if(gpiohs_get_pin(KEY_GPIONUM) == 1)/*press */
        // {
        printf("Sending frame\n");
        
        image_t img_out;
        bool jpgOK = camera_snapshot_jpeg(&img_out, JPEG_COMPRESSION_RATIO);
        if (!jpgOK) {
          printf("JPEG failed\n");
          continue;
        }

        fb.buf = img_out.pixels;
        fb.len = img_out.bpp;
        fb.width = img_out.w;
        fb.height = img_out.h;

        recognitionState = RECOGNITION_STATE_RECOGNIZING;
        lastRecognitionStateChangeMillis = millis();

#if CUSTOM_VISION_DEMO
        visionResult.clear();
        if (cvc->detect(&req, NULL, &visionResult))
#else
        #if USING_FACE_API
        if (cvc->detectWithFaceAPI(&req, &faceResult))
        #else
        if (cvc->detect(&req, &faceResult))
        #endif
#endif
        {
          printf("Frame processed\n");

#if !CUSTOM_VISION_DEMO

          printf(">>> Emotion: %s, Age: %d\n", faceResult.faceAttributes.emotion.topEmotionType, faceResult.faceAttributes.age);

          //Check the response payload
          if (faceResult.faceFound)
          {
            char emotionType[24];
            bool smiled = (faceResult.faceAttributes.smile > 0.5f);
            sprintf(emotionType, "%s%s", faceResult.faceAttributes.emotion.topEmotionType, (smiled? ",smile": ""));
            
            sprintf(recognitionResultString, "%s, %d (%s)", 
                    faceResult.faceAttributes.gender, faceResult.faceAttributes.age, emotionType);

          }
          else
          {
            sprintf(recognitionResultString, "%s", "Unknown");
          }

#else
          printf(">>> Tag: %s, Prob: %.2f\n", visionResult.getBestPrediction()->tagName, visionResult.getBestPrediction()->probability);
          if (visionResult.isBestPredictionFound())
          {
            sprintf(recognitionResultString, "%s (%.2f%s)", visionResult.getBestPrediction()->tagName, (visionResult.getBestPrediction()->probability * 100), "%");
          }
          else
          {
            sprintf(recognitionResultString, "%s", "Unknown");
          }
#endif
        }
        else
        {
          sprintf(recognitionResultString, "%s", "Unknown");
        }

        // free(fb.buf); //no need, as static allocation
        // fb.buf = NULL;
        recognitionState = RECOGNITION_STATE_DONE;
        lastRecognitionStateChangeMillis = millis();

        g_gpio_key_flag = 0;
        // }
      }
    }
  }
}

/**
 * Draw box and label on top of detected face
 */  
const uint32_t minTxtTop = 2 + (8 * LABEL_TEXT_SIZE) + 2; //8 = char height
static void drawboxlabel(Sipeed_ST7789 &_lcd, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t _class, float prob) {

  if (x1 >= 320)
    x1 = 319;
  if (x2 >= 320)
    x2 = 319;
  if (y1 >= 240)
    y1 = 239;
  if (y2 >= 240)
    y2 = 239;

  // lcd_draw_rectangle(x1, y1, x2, y2, 2, FACE_BOX_LINE_COLOR);
  _lcd.drawRect(x1, y1, (x2 - x1), (y2 - y1), FACE_BOX_LINE_COLOR);

  const char *msg = (recognitionState == RECOGNITION_STATE_REQUESTED || recognitionState == RECOGNITION_STATE_RECOGNIZING) ? "Recognizing..." : (recognitionState == RECOGNITION_STATE_DONE ? recognitionResultString : "Hello");

  //Initial
  uint32_t w = x2 - x1;
  uint16_t str_w = LABEL_TEXT_SIZE * 6 * strlen(msg);
  uint16_t str_x = x1 + (w - str_w) / 2;

  // bool atTop = y1 > minTxtTop;
  // int str_y = atTop ? (y1 - minTxtTop) : y2 + 5;

  int16_t rs_x, rs_y;
  uint16_t str_h;
  _lcd.getTextBounds(msg, 2, 2, &rs_x, &rs_y, &str_w, &str_h);

  uint16_t real_minTxtTop = 2 + str_h + 2;
  bool atTop = y1 > real_minTxtTop;
  int str_y = atTop ? (y1 - real_minTxtTop) : y2 + 5;
  str_x = x1 + (w - str_w) / 2;

  //Normalize str x
  if (str_x <= 2) {
    str_x = 2;
  }
  else if ((str_x + str_w) >= (CAMERA_WIDTH - 2)) {
    str_x = CAMERA_WIDTH - 2 - str_w; 
  }

  _lcd.setCursor(str_x, str_y);
  _lcd.println(msg);
}

static int ai_done(void *ctx)
{
  g_ai_done_flag = 1;
  return 0;
}

void setup() {
  // put your setup code here, to run once:
  delay(1000);
  Serial.begin(115200);

  

  lcd.begin(15000000, COLOR_RED);
  // lcd.setFont(&FreeSans9pt7b);
  lcd.setTextSize(2);
  lcd.setTextColor(COLOR_WHITE);
  lcd.setCursor((320 - (12 * 18)) / 2, (240 - 16) / 2);
  lcd.println("Connecting to WiFi");

  // initialize ESP module
  Serial.println("WiFi shield init");
  // initialize serial for ESP module
  Serial1.begin(115200);
  WiFi.init(&Serial1);

  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println("WiFi shield not present");
    // don't continue
    while (true)
      ;
  }

  // Attempt to connect to WiFi network
  while (wifiStatus != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(WIFI_SSID_NAME);

    // Connect to WPA/WPA2 network
    wifiStatus = WiFi.begin(WIFI_SSID_NAME, WIFI_SSID_PASS);
  }

  Serial.println("Connected to the network");


  // Register a function to call API, to be run on processor's core 1
  register_core1(recognitionHandler, NULL);
  delay(100);

  // For the next label
  lcd.setTextSize(LABEL_TEXT_SIZE);
  lcd.setTextColor(COLOR_WHITE);

  // To manually trigger recognition using on-board button
  gpiohs_set_drive_mode(KEY_GPIONUM, GPIO_DM_INPUT);
  gpiohs_set_pin_edge(KEY_GPIONUM, GPIO_PE_FALLING);
  gpiohs_set_irq(KEY_GPIONUM, 2, irq_key);
  
  // Init camera
  if (!camera_init())
  {
    printf("Camera init FAILED\n");
    // don't continue
    while (true)
      ;
  }
  else
  {
    printf("Camera init success\n");
  }

  // Init face detect model
  kpu_task_gencode_output_init(&task);
  // task.src= (uint64_t *)camera.getRGB888();
  task.src = (uint64_t *)camera_ai_buffer();
  task.dma_ch = DMAC_CHANNEL5;
  task.callback = ai_done;
  kpu_single_task_init(&task);

  detect_rl.threshold = FACE_DETECTION_PROB_THRESHOLD;
  region_layer_init(&detect_rl, &task);

}

uint32_t lastDetectionMillis = millis();

void loop() {

  // Start to calculate
  kpu_start(&task);
  while (!g_ai_done_flag) //Wait inference done
    ;
  g_ai_done_flag = 0;

  // Start region layer
  run_region_layer(&detect_rl);
  // printf("Box count: %d\n", detect_rl.boxes_number);

  // Get image to display on LCD
  uint32_t *img = camera_lcd_buffer();
  if (img == nullptr || img == 0) {
    printf("Cam snap FAILED\n");
  }
  else {
    
    lcd.drawImage(0, 0, CAMERA_WIDTH, CAMERA_HEIGHT, (uint16_t *)img);

    // There could be more than one box at a time, get the one with highest probability
    int maxIdx = -1;
    float maxProb = 0;
    for (size_t i = 0; i < detect_rl.boxes_number; i++) {
      if (detect_rl.boxes[i].prob > maxProb) {
        maxIdx = i;
        maxProb = detect_rl.boxes[i].prob;
      }
    }

    // auto box_num = ((1)<(detect_rl.boxes_number)?(1):(detect_rl.boxes_number));
    // for (size_t i= 0; i < box_num; i++) {
    //     box_t *b= detect_rl.boxes;
    //     drawboxes(lcd,
    //               (uint32_t)((b[i].x - b[i].w / 2) * CAMERA_WIDTH),
    //               (uint32_t)((b[i].y - b[i].h / 2) * CAMERA_HEIGHT),
    //               (uint32_t)((b[i].x + b[i].w / 2) * CAMERA_WIDTH),
    //               (uint32_t)((b[i].y + b[i].h / 2) * CAMERA_HEIGHT), 0,
    //               detect_rl.boxes[i].prob);
    //     printf("Prob: %.4f\n", detect_rl.boxes[i].prob);
    // }

    if (maxIdx > -1) {

      size_t i = maxIdx;
      drawboxlabel(lcd,
                  (uint32_t)((detect_rl.boxes[i].x - detect_rl.boxes[i].w / 2) * CAMERA_WIDTH),
                  (uint32_t)((detect_rl.boxes[i].y - detect_rl.boxes[i].h / 2) * CAMERA_HEIGHT),
                  (uint32_t)((detect_rl.boxes[i].x + detect_rl.boxes[i].w / 2) * CAMERA_WIDTH),
                  (uint32_t)((detect_rl.boxes[i].y + detect_rl.boxes[i].h / 2) * CAMERA_HEIGHT), 0,
                  detect_rl.boxes[i].prob);

      if (recognitionState == RECOGNITION_STATE_UNKNOWN && lastRecognitionStateChangeMillis == 0) {
        lastRecognitionStateChangeMillis = millis();
      }

      if ((recognitionState == RECOGNITION_STATE_UNKNOWN) && millis() - lastRecognitionStateChangeMillis > FACE_DETECTION_DEBOUNCE_MS) {
        recognitionState = RECOGNITION_STATE_REQUESTED;
        lastRecognitionStateChangeMillis = millis();
      }

      if (recognitionState == RECOGNITION_STATE_DONE) {
        lastRecognitionStateChangeMillis = millis(); //Keep update millis, to prevent debouncing
      }
    }
    else { 
      // If there's no BOX, it may be a time to request recognizing
      bool resetState = false;
      if (recognitionState == RECOGNITION_STATE_RECOGNIZING) { 
        // Never switch to unknown state, if it's previously recognizing
      }
      else if (recognitionState == RECOGNITION_STATE_DONE) {
        if (millis() - lastRecognitionStateChangeMillis > 1000) {
          resetState = true;
        }
      }
      else {
        resetState = true;
      }

      if (resetState) {
        recognitionState = RECOGNITION_STATE_UNKNOWN;
        lastRecognitionStateChangeMillis = 0;
      }
    }
  }
}