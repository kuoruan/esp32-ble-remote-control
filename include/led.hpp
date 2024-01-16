#include <string>

class Led {
   public:
    void init(int pin);
    void loop();
    void on();
    void off();
    void toggle();

   private:
    int _pin;
    bool _isOn = false;
};
