// Native Linux gamepad input for the Wayland backend.
#ifndef FLUTTER_ELINUX_GAMEPAD_INPUT_H_
#define FLUTTER_ELINUX_GAMEPAD_INPUT_H_

#include <array>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>

class GamepadInput {
 public:
  using Callback = std::function<void(const char*, bool)>;
  ~GamepadInput();
  void Poll(bool active, const Callback& callback);

 private:
  struct Axis {
    int center = 0;
    int radius = 0;
  };
  struct Device {
    int fd = -1;
    std::array<bool, 9> keys{};
    std::array<bool, 4> dpad{};
    std::array<bool, 4> hat{};
    std::array<bool, 4> stick{};
    Axis x;
    Axis y;
  };

  void Scan();
  void Handle(Device& device, unsigned short type, unsigned short code,
              int value);
  void Publish(bool active, const Callback& callback);

  std::unordered_map<std::string, Device> devices_;
  std::array<bool, 9> published_{};
  std::chrono::steady_clock::time_point next_scan_{};
};

#endif  // FLUTTER_ELINUX_GAMEPAD_INPUT_H_
