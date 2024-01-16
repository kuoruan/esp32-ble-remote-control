#include <string>

class Wifi {
   public:
    void init(const char *ssid, const char *password);
    void loop();

    void start();
    void stop();

   private:
    std::string _ssid;
    std::string _password;
    bool _isStarted = false;
};
