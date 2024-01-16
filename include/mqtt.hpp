#include <functional>
#include <string>

class Mqtt {
   public:
    void init(const char *host, int port, const char *clientId, const char *username,
              const char *password);
    void loop();
    void start();
    void stop();
    void publish(const char *topic, const char *payload);
    void subscribe(const char *topic);
    void unsubscribe(const char *topic);
    void onMessage(std::function<void(const char *topic, const char *payload)> callback);

   private:
    std::string _host;
    int _port;
    std::string _clientId;
    std::string _username;
    std::string _password;
    bool _isStarted = false;
    std::function<void(const char *topic, const char *payload)> _onMessageCallback;
};
