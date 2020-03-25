#include "FaceVisionClient.hpp"

#define printf Serial.printf

FaceVisionClient::FaceVisionClient(const FaceVisionClientConfig_t &config): clientConfig_(config) {
}

FaceVisionClient::~FaceVisionClient() {
    if (httpClient_ != NULL) {
        httpClient_->stop();
        // delete httpClient_;
        httpClient_ = NULL;
    }
    if (wifiClient_ != NULL) {
        // delete wifiClient_;
        wifiClient_ = NULL;
    }
}

static bool processJsonVision(JsonObject doc, FaceVisionClient::FaceVisionDetectionRequest_t *req, FaceVisionClient::FaceVisionDetectionResult_t *predResult) {
    
    // const uint8_t predMaxRes = 6;
    // const size_t capacity = JSON_ARRAY_SIZE(predMaxRes) + 2*predMaxRes*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5);

    // DynamicJsonDocument doc(capacity);

    // DeserializationError err = deserializeJson(doc, json);
    // if (err) {
    //     printf("deserializeJson() returned %s\n", err.c_str());
    //     return false;
    // }

    JsonArray predictions = doc["predictions"].as<JsonArray>();
    if (predictions.isNull() || predictions.size() == 0) {
        printf("No predictions\n");
        return false;
    }

    float maxProb = 0; int maxProbIdx = -1, idx = 0;
    for(auto prediction: predictions) {
        float probability = prediction["probability"]; // 0.7644105
        const char* tagId = prediction["tagId"]; // "b2b34875-82af-44d9-93c4-ae61fbfabfb5"
        const char* tagName = prediction["tagName"]; // "Andri"
        if (!tagName) {
			continue;
		}

        FaceVisionClient::FaceVisionDetectionModel_t predModel;

        JsonObject boundingBox = prediction["boundingBox"];
        if (!boundingBox.isNull()) {
            float left = boundingBox["left"].as<float>() * req->cameraFb->width; // 0.24105148
            float top = boundingBox["top"].as<float>() * req->cameraFb->height; // 0.183790118
            float width = boundingBox["width"].as<float>() * req->cameraFb->width; // 0.344858944
            float height = boundingBox["height"].as<float>() * req->cameraFb->height; // 0.6609372

            printf("Bounding box: (%.2f, %.2f, %.2f, %.2f)\n", left, top, width, height);

            predModel.region.left = left;
			predModel.region.top = top;
			predModel.region.width = width;
			predModel.region.height = height;
        }
        else {
            continue;
        }

        strncpy(predModel.tagId, tagId, strlen(tagId) + 1);
//		predModel.tagId[strlen(tagId->valuestring)] = '\0';
		strncpy(predModel.tagName, tagName, strlen(tagName) + 1);
//		predModel.tagName[strlen(tagName->valuestring)] = '\0';

		predModel.probability = probability;

        //Add to predictions vector
		predResult->predictions.push_back(predModel);

		//To deal with best prediction
		if (probability > maxProb) {
			maxProb = probability;
			maxProbIdx = idx;
		}

		printf("Prediction => idx: %d, id: %s, tag: %s. prob: %.2f%s\n", idx, predModel.tagId, predModel.tagName, (predModel.probability*100), "%");
		idx++;
    }

    if (maxProbIdx < 0) {
		predResult->bestPredictionIndex = -1;
		return true; //Return OK
	}
    
    auto bestPrediction = predictions[maxProbIdx];
	float bestProbability = bestPrediction["probability"];
	const char* bestTagName = bestPrediction["tagName"];
	
	printf("Best prediction => idx: %d, tag: %s. prob: %.2f%s\n", maxProbIdx, bestTagName, (bestProbability*100), "%");

	if (req->bestPredictionThreshold > 0.0f && bestProbability < req->bestPredictionThreshold) {
		printf("Best prediction is NOT enough!\n");
		return true; //Return OK
	}

//	if (result->bestPredictionLabel != NULL) {
//		sprintf(result->bestPredictionLabel, "%s (%.2f%s)", bestTagName->valuestring, (bestProbF*100), "%");
//	}

	predResult->bestPredictionIndex = maxProbIdx;
	predResult->bestPredictionThreshold = req->bestPredictionThreshold;

    return true;
}

static bool processJsonFace(JsonArray doc, FaceVisionClient::FaceVisionDetectionRequest_t *req, FaceVisionClient::FaceAPIDetectionResult_t *result) {

    if (doc.isNull() || doc.size() == 0) {
        printf("No predictions\n");
        return false;
    }

    JsonObject faceObject = doc[0].as<JsonObject>();
    if (!faceObject.containsKey("faceRectangle")) {
        printf("No face\n");
        return false;
    }

    result->faceFound = true;

    JsonObject faceRectangle = faceObject["faceRectangle"];
	if (!faceRectangle.isNull()) {
		
        int leftInt = faceRectangle["left"].as<int>();
		int topInt = faceRectangle["top"].as<int>();
		int widthInt = faceRectangle["width"].as<int>();
		int heightInt = faceRectangle["height"].as<int>();

		printf("Bounding box: (%d, %d, %d, %d)\n", leftInt, topInt, widthInt, heightInt);

		result->faceRectangle.left = leftInt;
		result->faceRectangle.top = topInt;
		result->faceRectangle.width = widthInt;
		result->faceRectangle.height = heightInt;
	}

    //To deal with face attribute
	JsonObject faceAttributes = faceObject["faceAttributes"];
    if (!faceAttributes.isNull()) {
        FaceVisionClient::FaceAPIAttributes_t attrObj = {};

		if (faceAttributes.containsKey("age")) {
			attrObj.age = faceAttributes["age"].as<int>();
		}

        if (faceAttributes.containsKey("smile")) {
			attrObj.smile = faceAttributes["smile"].as<float>();
		}

		if (faceAttributes.containsKey("gender")) {
            const char *genderStr = faceAttributes["gender"].as<char*>();
			strncpy(attrObj.gender, genderStr, strlen(genderStr) + 1);
		}

		if (faceAttributes.containsKey("glasses")) {
            const char *glassesStr = faceAttributes["glasses"].as<char*>();
			strncpy(attrObj.glasses, glassesStr, strlen(glassesStr) + 1);
		}

        if (faceAttributes.containsKey("facialHair")) {
			JsonObject facialHair = faceAttributes["facialHair"].as<JsonObject>();
            attrObj.facialHair.beard = facialHair["beard"].as<float>();
            attrObj.facialHair.moustache = facialHair["moustache"].as<float>();
            attrObj.facialHair.sideburns = facialHair["sideburns"].as<float>();
		}

        if (faceAttributes.containsKey("hair")) {
			JsonObject hair = faceAttributes["hair"].as<JsonObject>();
            attrObj.hair.bald = hair["bald"].as<float>();
            attrObj.hair.invisible = hair["invisible"].as<bool>();
           
            float maxHairScore = 0; int maxHairIdx = -1; uint8_t idx = 0;
			for(auto hairColor: hair["hairColor"].as<JsonArray>()) {
                if (hairColor["confidence"].as<float>() > maxHairScore) {
                    maxHairScore = hairColor["confidence"].as<float>();
                    maxHairIdx = idx;
                }
                idx++;
            }
            if (maxHairIdx > -1) {
                attrObj.hair.hairColorConfidence = hair["hairColor"][maxHairIdx]["confidence"].as<float>();
                const char *hl = hair["hairColor"][maxHairIdx]["color"].as<char*>();
                strncpy(attrObj.hair.hairColorLabel, hl, strlen(hl) + 1);
            }
		}

        if (faceAttributes.containsKey("emotion")) {
			JsonObject emotion = faceAttributes["emotion"].as<JsonObject>();

            float maxScore = 0;
			char maxEmot[16];

			if (emotion.containsKey("anger")) {
                attrObj.emotion.anger = emotion["anger"].as<float>();
                if (attrObj.emotion.anger > maxScore) {
				    maxScore = attrObj.emotion.anger;
				    strncpy(maxEmot, "anger", 6);
                }
			}
            if (emotion.containsKey("contempt")) {
                attrObj.emotion.contempt = emotion["contempt"].as<float>();
                if (attrObj.emotion.contempt > maxScore) {
				    maxScore = attrObj.emotion.contempt;
				    strncpy(maxEmot, "contempt", 9);
                }
			}
            if (emotion.containsKey("disgust")) {
                attrObj.emotion.disgust = emotion["disgust"].as<float>();
                if (attrObj.emotion.disgust > maxScore) {
				    maxScore = attrObj.emotion.disgust;
				    strncpy(maxEmot, "disgust", 8);
                }
			}
			if (emotion.containsKey("fear")) {
                attrObj.emotion.fear = emotion["fear"].as<float>();
                if (attrObj.emotion.fear > maxScore) {
				    maxScore = attrObj.emotion.fear;
				    strncpy(maxEmot, "fear", 5);
                }
			}
            if (emotion.containsKey("happiness")) {
                attrObj.emotion.happiness = emotion["happiness"].as<float>();
                if (attrObj.emotion.happiness > maxScore) {
				    maxScore = attrObj.emotion.happiness;
				    strncpy(maxEmot, "happiness", 10);
                }
			}
			if (emotion.containsKey("neutral")) {
                attrObj.emotion.neutral = emotion["neutral"].as<float>();
                if (attrObj.emotion.neutral > maxScore) {
				    maxScore = attrObj.emotion.neutral;
				    strncpy(maxEmot, "neutral", 8);
                }
			}
            if (emotion.containsKey("sadness")) {
                attrObj.emotion.sadness = emotion["sadness"].as<float>();
                if (attrObj.emotion.sadness > maxScore) {
				    maxScore = attrObj.emotion.sadness;
				    strncpy(maxEmot, "sadness", 8);
                }
			}
			if (emotion.containsKey("surprise")) {
                attrObj.emotion.surprise = emotion["surprise"].as<float>();
                if (attrObj.emotion.surprise > maxScore) {
				    maxScore = attrObj.emotion.surprise;
				    strncpy(maxEmot, "surprise", 9);
                }
			}

            attrObj.emotion.topEmotionScore = maxScore;
			strncpy(attrObj.emotion.topEmotionType, maxEmot, strlen(maxEmot) + 1);
		}

        result->faceAttributes = attrObj;    
    }

    printf("Prediction => id: %s, emotion: %s. score: %.2f%s\n", result->faceId, result->faceAttributes.emotion.topEmotionType, (result->faceAttributes.emotion.topEmotionScore*100), "%");

    return true;
}

static bool processJson(const char *json, FaceVisionClient::FaceVisionDetectionRequest_t *req, FaceVisionClient::FaceVisionDetectionResult_t *visionResult = NULL, FaceVisionClient::FaceAPIDetectionResult_t *faceResult = NULL) {
    
    const uint8_t visionPredMaxRes = 10;
    size_t capacity = (JSON_ARRAY_SIZE(visionPredMaxRes) + 2*visionPredMaxRes*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5));
    if (!req->onlyCustomVision) {
        capacity += (JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(6) + 6*JSON_OBJECT_SIZE(2) + 3*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(7) + JSON_OBJECT_SIZE(8));
    }

    DynamicJsonDocument doc(capacity);

    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        printf("deserializeJson() returned %s\n", err.c_str());
        return false;
    }

    bool res = true;
    if (req->onlyCustomVision) {
        res = processJsonVision(doc.as<JsonObject>(), req, visionResult);
    }
    else {
        if (doc.containsKey("vision")) {
            res = processJsonVision(doc["vision"], req, visionResult);
        }
        else {
            if (doc.is<JsonObject>()) {
                processJsonVision(doc.as<JsonObject>(), req, visionResult);
            }
        }

        if (doc.containsKey("face")) {
            res |= processJsonFace(doc["face"].as<JsonArray>(), req, faceResult);
        }
        else {
            if (doc.is<JsonArray>()) {
                printf("Process JSON from face array\n");
                res |= processJsonFace(doc.as<JsonArray>(), req, faceResult);
            }
        }
    }

    return res;
}

bool FaceVisionClient::detect(FaceVisionDetectionRequest_t *req, FaceAPIDetectionResult_t *faceResult, FaceVisionDetectionResult_t *visionResult) {
    if (WiFi.status() != WL_CONNECTED) {
        printf("NOT CONNECTED to WiFI!\n");
        return false;
    }

    if (wifiClient_ == NULL) {
        wifiClient_ = new WiFiEspClient();
        // wifiClient_->setTimeout(30*1000);
    }


    /*   
    //To use built-in WifiEspClient

    if (!wifiClient_->connected()) {
        printf("Starting connection to server...\n");
        // if (!wifiClient_->connect(clientConfig_.host, 80)) {            
        //     return false;
        // }
        if (!wifiClient_->connectSSL("southeastasia.api.cognitive.microsoft.com", 443)) {            
            return false;
        }
    }

    printf("Connected to server\n");
    size_t written = 0;

    // written = wifiClient_->println("POST /api/http_uploader?code=qxb1GQ1IUJ2CPna3CZcqQflJaHEN5AvrRSYjdsni5pa6Rcp6oRMnqQ== HTTP/1.1");
    // if (written <= 0) {
    //     return false;
    // }
    // printf("Sent POST\n");
    // wifiClient_->print("HOST: ");
    // wifiClient_->println(clientConfig_.host);
    // wifiClient_->println("User-Agent: Mozilla/5.0");
    // wifiClient_->println("Content-Type: application/octet-stream");
    // wifiClient_->print("Content-Length: "); wifiClient_->println(req->cameraFb->len);
    // // wifiClient_->println(2);
    // wifiClient_->println("Connection: close");
    // wifiClient_->println();


    written = wifiClient_->println("POST /face/v1.0/detect HTTP/1.1");
    if (written <= 0) {
        return false;
    }
    printf("Sent POST\n");
    wifiClient_->print("HOST: ");
    wifiClient_->println(clientConfig_.host);
    wifiClient_->println("User-Agent: Mozilla/5.0");
    wifiClient_->println("Ocp-Apim-Subscription-Key: d65aab0fe2844a68b9c50b3ae6c0280c");
    wifiClient_->println("Content-Type: application/octet-stream");
    wifiClient_->print("Content-Length: "); wifiClient_->println((int)req->cameraFb->len);
    wifiClient_->println("Connection: close");
    wifiClient_->println();


    //POST
    delay(50);
    printf("Sending content: %ld\n", req->cameraFb->len);
    // Serial.write(fbIn->buf, fbIn->len);
    // char testdata[] = {0x00, 0x01};
    // wifiClient_->write(testdata, 2);   

    // written = wifiClient_->write(fbIn->buf, fbIn->len);
    // if (written <= 0) {
    //     printf("Failed to send content\n");
    //     return false;
    // }

    uint8_t writebuf[1024*2];
    int remain = req->cameraFb->len;
    while (remain)
    {
      int toCpy = remain > sizeof(writebuf) ? sizeof(writebuf) : remain;
      memcpy(writebuf, req->cameraFb->buf, toCpy);
      req->cameraFb->buf += toCpy;
      remain -= toCpy;
      wifiClient_->write(writebuf, toCpy);
    }
    
    printf("Waiting for response...\n");

    while(wifiClient_->connected()) {
        while (wifiClient_->available()) {
            char c = wifiClient_->read();
            Serial.write(c);
        }
        // delay(10);
    }  
    */

    printf("Starting HTTP client. Path:%s\n", clientConfig_.path);
    if (httpClient_ == NULL) {
        httpClient_ = new HttpClient(*wifiClient_, clientConfig_.host, 80);
    }
    else {
        httpClient_->connect(clientConfig_.host, 80);
    }

    //Make sure it stopped
    httpClient_->stop();

    // httpClient_->connectionKeepAlive();
    httpClient_->beginRequest();
    int connRes = httpClient_->post(clientConfig_.path);
    if (connRes != HTTP_SUCCESS) {
        // httpClient_->stop();
        // httpClient_ = NULL;
        // wifiClient_ = NULL;
        return false;
    }

    httpClient_->sendHeader("Content-Type", "application/octet-stream");
    httpClient_->sendHeader("Content-Length", (int)req->cameraFb->len);
    
    // httpClient_->sendHeader("User-Agent", "Mozilla/5.0");
    // httpClient_->endRequest();
    // httpClient_->println();
    // delay(1000);
    httpClient_->beginBody();
    
    printf("Sending image with size: %ld\n", req->cameraFb->len);
    if (req->cameraFb->buf != NULL) {
        // httpClient_->write(fbIn->buf, fbIn->len);

        uint8_t writebuf[1024*2];
        int remain = req->cameraFb->len;
        while (remain)
        {
            int toCpy = remain > sizeof(writebuf) ? sizeof(writebuf) : remain;
            memcpy(writebuf, req->cameraFb->buf, toCpy);
            req->cameraFb->buf += toCpy;
            remain -= toCpy;
            httpClient_->write(writebuf, toCpy);
        }
    }

    // httpClient_->endRequest();
    delay(100);
    printf("Waiting for response...\n");

    // httpClient_->post("/api/http_uploader?code=qxb1GQ1IUJ2CPna3CZcqQflJaHEN5AvrRSYjdsni5pa6Rcp6oRMnqQ==", "application/octet-stream", 2, testdata);

    if (httpClient_->connected()) {  
        // If connected to the server,     
        // read the status code and body of the response
        int statusCode = httpClient_->responseStatusCode();
        printf("Status code: %d\n", statusCode);
        if (statusCode != 200) {
            printf("Wrong status code\n");
            httpClient_->stop();
            return false;
        }
        String response = httpClient_->responseBody(); 
        printf("Response: %s\n", response.c_str());

        //Process JSON
        if (processJson(response.c_str(), req, visionResult, faceResult)) {
            printf("JSON parsing SUCCESS\n");
        }
        else {
            printf("JSON parsing FAILED!\n");
            return false;
        }
    }
    else {
        printf("Lost connection\n");
        httpClient_->stop();
        // httpClient_ = NULL;
        // wifiClient_ = NULL;
        return false;
    }

    httpClient_->stop();

   
    return true;
}


bool FaceVisionClient::detectWithFaceAPI(FaceVisionDetectionRequest_t *req, FaceAPIDetectionResult_t *faceResult, FaceVisionDetectionResult_t *visionResult) {
    if (WiFi.status() != WL_CONNECTED) {
        printf("NOT CONNECTED to WiFI!\n");
        return false;
    }

    if (wifiClient_ == NULL) {
        wifiClient_ = new WiFiEspClient();
        // wifiClient_->setTimeout(30*1000);
    }
    
    printf("Starting HTTPS client. Path:%s\n", clientConfig_.path);
    if (httpClient_ == NULL) {
        httpClient_ = new HttpsClient(*wifiClient_, clientConfig_.host, 443);
    }
    else {
        httpClient_->connect(clientConfig_.host, 443);
    }

    //Make sure it stopped
    httpClient_->stop();

    // httpClient_->connectionKeepAlive();
    httpClient_->beginRequest();

    // Params
    int connRes = httpClient_->post(clientConfig_.path);
    if (connRes != HTTP_SUCCESS) {
        // httpClient_->stop();
        // httpClient_ = NULL;
        // wifiClient_ = NULL;
        return false;
    }

    httpClient_->sendHeader("Content-Type", "application/octet-stream");
    httpClient_->sendHeader("Content-Length", (int)req->cameraFb->len);
    httpClient_->sendHeader("User-Agent", "Mozilla/5.0");
    httpClient_->sendHeader("Ocp-Apim-Subscription-Key", clientConfig_.key);
    // httpClient_->endRequest();
    // httpClient_->println();
    // delay(1000);
    httpClient_->beginBody();
    
    printf("Sending image with size: %ld\n", req->cameraFb->len);
    if (req->cameraFb->buf != NULL) {
        // httpClient_->write(fbIn->buf, fbIn->len);

        uint8_t writebuf[1024*2];
        int remain = req->cameraFb->len;
        while (remain)
        {
            int toCpy = remain > sizeof(writebuf) ? sizeof(writebuf) : remain;
            memcpy(writebuf, req->cameraFb->buf, toCpy);
            req->cameraFb->buf += toCpy;
            remain -= toCpy;
            httpClient_->write(writebuf, toCpy);
        }
    }

    // httpClient_->endRequest();
    delay(100);
    printf("Waiting for response...\n");

    // httpClient_->post("/api/http_uploader?code=qxb1GQ1IUJ2CPna3CZcqQflJaHEN5AvrRSYjdsni5pa6Rcp6oRMnqQ==", "application/octet-stream", 2, testdata);

    if (httpClient_->connected()) {  
        // If connected to the server,     
        // read the status code and body of the response
        int statusCode = httpClient_->responseStatusCode();
        printf("Status code: %d\n", statusCode);
        if (statusCode != 200) {
            printf("Wrong status code\n");
            httpClient_->stop();
            return false;
        }
        String response = httpClient_->responseBody(); 
        printf("Response: %s\n", response.c_str());

        //Process JSON
        if (processJson(response.c_str(), req, visionResult, faceResult)) {
            printf("JSON parsing SUCCESS\n");
        }
        else {
            printf("JSON parsing FAILED!\n");
            return false;
        }
    }
    else {
        printf("Lost connection\n");
        httpClient_->stop();
        // httpClient_ = NULL;
        // wifiClient_ = NULL;
        return false;
    }

    httpClient_->stop();    
   
    return true;
}