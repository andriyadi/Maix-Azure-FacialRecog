#pragma once

#include "Arduino.h"
#include "WiFiEsp.h"

#include <ArduinoJson.h>

#include "ArduinoHttpClient.h"
#include "HttpsClient.h"

#undef min 
#undef max
#include <vector>

#define FACE_API_ENABLED 1

struct camera_fb_t {
    uint8_t * buf = NULL;       /*!< Pointer to the pixel data */
    size_t len;                 /*!< Length of the buffer in bytes */
    size_t width;               /*!< Width of the buffer in pixels */
    size_t height;              /*!< Height of the buffer in pixels */
};


class FaceVisionClient {
public:

    struct FaceVisionClientConfig_t {
		const char *host;
        const char *path;
		const char *key;
		bool shouldStoreImage;
	};

    struct FaceVisionDetectionRequest_t {
		FaceVisionDetectionRequest_t() {}
		~FaceVisionDetectionRequest_t() {}

		camera_fb_t *cameraFb = NULL;
        float bestPredictionThreshold = 0.5f;

		bool onlyCustomVision = false;
	};

    struct BoundingBox_t {
		float left, top, width, height;
	};

    struct FaceVisionDetectionModel_t {
		char tagName[64];	// = NULL; --> Somehow I can not use dynamic allocation for this class
		char tagId[40];		// = NULL;
		float probability = 0.0f;
		BoundingBox_t region = {0, 0, 0, 0};

		FaceVisionDetectionModel_t() {}
		~FaceVisionDetectionModel_t() {}
	};

	struct FaceVisionDetectionResult_t {
		std::vector<FaceVisionDetectionModel_t> predictions;
		char *id = NULL;
		float bestPredictionThreshold = 0.5f;

		//Just for convenience
		int16_t bestPredictionIndex = -1;

		FaceVisionDetectionResult_t() {
			predictions.reserve(10);
			id = new char[40]();
		}

		~FaceVisionDetectionResult_t() {
			if (id != NULL) {
				delete[] id;
				id = NULL;
			}
		}

		bool isBestPredictionFound() {
			return (bestPredictionIndex >= 0);
		}

		const FaceVisionClient::FaceVisionDetectionModel_t *getBestPrediction() {
			if (predictions.empty() || !isBestPredictionFound()) {
				return NULL;
			}
			else {
				return &predictions.at(bestPredictionIndex);
			}
		}

		bool getBestPredictionLabel(char *label) {
			if (label == NULL || !isBestPredictionFound()) {
				return false;
			}

			FaceVisionClient::FaceVisionDetectionModel_t bestPred = predictions.at(bestPredictionIndex);
			sprintf(label, "%s (%.2f%s)", bestPred.tagName, (bestPred.probability*100), "%");

			return true;
		}

		void clear() {
			memset(id, 0, 40);
			predictions.clear();
		}
	};

#if FACE_API_ENABLED
	struct FaceAPIAttributeEmotion_t {
		float anger = 0.0f;
		float contempt = 0.0f;
		float disgust = 0.0f;
		float fear = 0.0f;
		float happiness = 0.0f;
		float neutral = 0.0f;
		float sadness = 0.0f;
		float surprise = 0.0f;

		// Just for convenience
		char topEmotionType[16];
		float topEmotionScore = 0.0f;
	};

	struct FaceAPIAttributeFacialHair_t {
		float moustache = 0.0f;
		float beard = 0.0f;
		float sideburns = 0.0f;
	};

	struct FaceAPIAttributeHair_t {
		float bald = 0.0f;
		bool invisible = false;
		char hairColorLabel[10];
		float hairColorConfidence = 0.0f;
	};

	struct FaceAPIAttributes_t {
		float smile;
		uint8_t age;
		char gender[10];
		char glasses[20];
		FaceAPIAttributeFacialHair_t facialHair;
		FaceAPIAttributeEmotion_t emotion;
		FaceAPIAttributeHair_t hair;
	};

	struct FaceAPIDetectionResult_t {
		char *faceId = NULL;
		char *recognitionModel = NULL;
		BoundingBox_t faceRectangle = {0, 0, 0, 0};
		FaceAPIAttributes_t faceAttributes;

		//Just for convenience
		bool faceFound = false;

		FaceAPIDetectionResult_t() {
			faceId = new char[40]();
			recognitionModel = new char[40]();
		}

		~FaceAPIDetectionResult_t() {
			if (faceId != NULL) {
				delete[] faceId;
				faceId = NULL;
			}
			if (recognitionModel != NULL) {
				delete[] recognitionModel;
				recognitionModel = NULL;
			}
		}
	};
#endif

    FaceVisionClient(const FaceVisionClientConfig_t &config);
    ~FaceVisionClient();

	bool detect(FaceVisionDetectionRequest_t *req, FaceAPIDetectionResult_t *faceResult = NULL, FaceVisionDetectionResult_t *visionResult = NULL);
	bool detectWithFaceAPI(FaceVisionDetectionRequest_t *req, FaceAPIDetectionResult_t *faceResult = NULL, FaceVisionDetectionResult_t *visionResult = NULL);

//    bool detect(FaceVisionDetectionRequest_t *req, FaceVisionDetectionResult_t *predResult = NULL);

private:
    FaceVisionClientConfig_t clientConfig_;
    WiFiEspClient *wifiClient_ = NULL;
	HttpClient *httpClient_ = NULL;
};