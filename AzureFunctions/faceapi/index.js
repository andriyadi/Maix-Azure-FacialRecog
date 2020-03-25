'use strict';
const request = require('request');

module.exports = function (context, req) {
    context.log('JavaScript HTTP trigger function processed a request.');

    if (!req.body) {
        context.res = {
            status: 400,
            body: "Post a image file in the request body"
        };

        context.done();
        return;
    }

    // To dump to Blob Storage, to check we have received image in request body
    // context.bindings.outputBlob = req.body;
    
    // Replace <Subscription Key> with your valid subscription key of Face API.
    // Get key here: https://azure.microsoft.com/try/cognitive-services/?api=face-api
    // Or follow instruction here: https://azure.microsoft.com/try/cognitive-services/?api=face-api
    const subscriptionKey = '<Subscription Key>';

    // You must use the same location in your REST call as you used to get your
    // subscription keys. For example, if you got your subscription keys from
    // westus, replace "westcentralus" in the URL below with "westus".
    const uriBase = 'https://southeastasia.api.cognitive.microsoft.com/face/v1.0/detect';
    
    // Request parameters.
    const params = {
        'returnFaceId': 'true',
        'returnFaceLandmarks': 'false',
        'returnFaceAttributes': 'age,gender,smile,facialHair,glasses,emotion'  
    };
    // add this in `returnFaceAttributes` if you like: ',hair,headPose,makeup,occlusion,accessories,blur,exposure,noise'

    const options = {
        uri: uriBase,
        qs: params,
        body: req.body,
        headers: {
            'Content-Type': 'application/octet-stream',
            'Ocp-Apim-Subscription-Key' : subscriptionKey
        }
    };

    request.post(options, (error, response, theBody) => {
        if (error) {
            context.log(error);
            context.res = {
                status: 500, 
                body: error
            };
        }
        else {
            let jsonResponse = JSON.stringify(JSON.parse(theBody), null, '  ');
            context.log(jsonResponse);

            context.res = {
                status: response.statusCode, 
                headers: {
                    'content-type': response.headers['content-type'],
                    'content-length': response.headers['content-length']
                },
                body: theBody
            };
        }

        context.done();
    });
};
