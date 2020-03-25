
#define LABEL_TEXT_SIZE                     2
#define FACE_BOX_LINE_COLOR                 COLOR_GREEN

// Threshold to consider a face detected
#define FACE_DETECTION_PROB_THRESHOLD       0.7

// Wait for certain time since a face detected, to be sure the face is really there, before actually sending request to Face API
#define FACE_DETECTION_DEBOUNCE_MS          1000

#define JPEG_COMPRESSION_RATIO              35  //in percentage

// Use Face API directly or via REST API that's deployed as Azure Functions
#define USING_FACE_API                      1 // 1 -> Use Face API directly

#define CUSTOM_VISION_DEMO                  0 // Leave it 0 for now