#include "flutter/shell/platform/linux_embedded/window/gamepad_input.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <utility>

namespace {
// Keep these names platform-independent for Flutter's BasicMessageChannel.
constexpr const char* kActions[] = {
    "up", "right", "down", "left", "confirm", "back", "menu", "l1", "r1"};
constexpr size_t kBitCount = sizeof(unsigned long) * 8;

bool HasKey(const std::array<unsigned long, KEY_MAX / kBitCount + 1>& bits,
            int code) {
  return (bits[code / kBitCount] & (1UL << (code % kBitCount))) != 0;
}

}  // namespace

GamepadInput::~GamepadInput() {
  for (auto& [_, device] : devices_) close(device.fd);
}

void GamepadInput::Scan() {
  DIR* directory = opendir("/dev/input");
  if (!directory) return;
  while (dirent* entry = readdir(directory)) {
    const std::string name(entry->d_name);
    if (name.compare(0, 5, "event") != 0) continue;
    const std::string path = "/dev/input/" + name;
    if (devices_.count(path)) continue;
    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;
    std::array<unsigned long, KEY_MAX / kBitCount + 1> keys{};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys.data()) < 0 ||
        !HasKey(keys, BTN_SOUTH) || !HasKey(keys, BTN_EAST)) {
      close(fd);
      continue;
    }
    Device device;
    device.fd = fd;
    input_absinfo info{};
    if (ioctl(fd, EVIOCGABS(ABS_X), &info) == 0) {
      device.x = {(info.minimum + info.maximum) / 2,
                  (info.maximum - info.minimum) / 2};
    }
    if (ioctl(fd, EVIOCGABS(ABS_Y), &info) == 0) {
      device.y = {(info.minimum + info.maximum) / 2,
                  (info.maximum - info.minimum) / 2};
    }
    devices_.emplace(path, std::move(device));
  }
  closedir(directory);
}

void GamepadInput::Handle(Device& device, unsigned short type,
                          unsigned short code, int value) {
  if (type == EV_KEY && value != 2) {
    const bool down = value != 0;
    switch (code) {
      case BTN_DPAD_UP: device.dpad[0] = down; break;
      case BTN_DPAD_RIGHT: device.dpad[1] = down; break;
      case BTN_DPAD_DOWN: device.dpad[2] = down; break;
      case BTN_DPAD_LEFT: device.dpad[3] = down; break;
      case BTN_SOUTH: device.keys[4] = down; break;
      case BTN_EAST: device.keys[5] = down; break;
      case BTN_START: device.keys[6] = down; break;
      case BTN_TL: device.keys[7] = down; break;
      case BTN_TR: device.keys[8] = down; break;
    }
  } else if (type == EV_ABS) {
    switch (code) {
      case ABS_HAT0X:
        device.hat[3] = value < 0;
        device.hat[1] = value > 0;
        break;
      case ABS_HAT0Y:
        device.hat[0] = value < 0;
        device.hat[2] = value > 0;
        break;
      case ABS_X:
        if (device.x.radius > 0) {
          const double normalized = static_cast<double>(value - device.x.center) /
                                    device.x.radius;
          device.stick[3] = normalized < -0.55;
          device.stick[1] = normalized > 0.55;
        }
        break;
      case ABS_Y:
        if (device.y.radius > 0) {
          const double normalized = static_cast<double>(value - device.y.center) /
                                    device.y.radius;
          device.stick[0] = normalized < -0.55;
          device.stick[2] = normalized > 0.55;
        }
        break;
    }
  }
}

void GamepadInput::Publish(bool active, const Callback& callback) {
  for (size_t i = 0; i < published_.size(); ++i) {
    bool held = false;
    if (active) {
      for (const auto& [_, device] : devices_) {
        held |= device.keys[i];
        if (i < 4) held |= device.dpad[i] || device.hat[i] || device.stick[i];
      }
    }
    if (published_[i] != held) {
      published_[i] = held;
      callback(kActions[i], held);
    }
  }
}

void GamepadInput::Poll(bool active, const Callback& callback) {
  const auto now = std::chrono::steady_clock::now();
  if (now >= next_scan_) {
    Scan();
    next_scan_ = now + std::chrono::seconds(2);
  }
  for (auto it = devices_.begin(); it != devices_.end();) {
    bool disconnected = false;
    input_event event{};
    ssize_t bytes = 0;
    while ((bytes = read(it->second.fd, &event, sizeof(event))) ==
           sizeof(event)) {
      Handle(it->second, event.type, event.code, event.value);
      Publish(active, callback);
    }
    if (bytes == 0 || (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
      disconnected = true;
    }
    if (disconnected) {
      close(it->second.fd);
      it = devices_.erase(it);
    } else {
      ++it;
    }
  }
  Publish(active, callback);
}
