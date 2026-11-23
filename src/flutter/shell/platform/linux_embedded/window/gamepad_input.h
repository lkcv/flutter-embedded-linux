// Native Linux gamepad input for the Wayland backend.
#ifndef FLUTTER_ELINUX_GAMEPAD_INPUT_H_
#define FLUTTER_ELINUX_GAMEPAD_INPUT_H_

#include <array>
#include <chrono>
#include <functional>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>

class GamepadInput {
 public:
  using Callback = std::function<void(const char*, bool)>;
  ~GamepadInput();
  void Poll(bool active, const Callback& callback);

 private:
  enum class Model { unknown, dualshock4, dualsense };

  struct Axis {
    int center = 0;
    int radius = 0;
  };
  struct Device {
    int fd = -1;
    Model model = Model::unknown;
    std::array<bool, 12> keys{};
    std::array<bool, 4> dpad{};
    std::array<bool, 4> hat{};
    std::array<bool, 4> stick{};
    Axis x;
    Axis y;
  };

  using DeviceMap = std::unordered_map<std::string, Device>;
  static DeviceMap Scan(const std::unordered_set<std::string>& known);
  void Handle(Device& device, unsigned short type, unsigned short code,
              int value);
  void Publish(bool active, const Callback& callback);

  DeviceMap devices_;
  std::future<DeviceMap> scan_future_;
  std::array<bool, 12> published_{};
  Model last_active_ = Model::unknown;
  unsigned connected_ = 0;
  std::chrono::steady_clock::time_point next_scan_{};
};

#endif  // FLUTTER_ELINUX_GAMEPAD_INPUT_H_
