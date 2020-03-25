//by Andri
#include "HttpClient.h"
#include <WiFiEspClient.h>

class HttpsClient : public HttpClient
{
public:
    HttpsClient(Client& aClient, const char* aServerName, uint16_t aServerPort = kHttpPort): 
    HttpClient(aClient, aServerName, aServerPort) {
    }

    HttpsClient(Client& aClient, const String& aServerName, uint16_t aServerPort = kHttpPort): 
    HttpClient(aClient, aServerName, aServerPort) {
    }

    HttpsClient(Client& aClient, const IPAddress& aServerAddress, uint16_t aServerPort = kHttpPort): 
    HttpClient(aClient, aServerAddress, aServerPort) {
    }

    virtual int connect(IPAddress ip, uint16_t port) { 
      Serial.println("Connect HTTPS");
      return ((WiFiEspClient*)iClient)->connectSSL(ip, port); 
    };

    virtual int connect(const char *host, uint16_t port) { 
      Serial.println("Connect HTTPS");
      return ((WiFiEspClient*)iClient)->connectSSL(host, port); 
    };
};