/**
 * @file src/input.h
 * @brief Declarations for gamepad, keyboard, and mouse input handling.
 */
#pragma once

// standard includes
#include <functional>

// local includes
#include "platform/common.h"
#include "thread_safe.h"
#include "crypto.h"

namespace input {
  struct input_t;  ///< Forward declaration of input structure

  /**
   * @brief Print input device information.
   * @param input Pointer to input device structure.
   */
  void print(void *input);

  /**
   * @brief Reset input device state.
   * @param input Shared pointer to input device.
   */
  void reset(std::shared_ptr<input_t> &input);

  /**
   * @brief Pass through input data to the system.
   * @param input Shared pointer to input device.
   * @param input_data The input data to process.
   * @param permission The permissions for this input operation.
   */
  void passthrough(std::shared_ptr<input_t> &input, std::vector<std::uint8_t> &&input_data, const crypto::PERM& permission);

  /**
   * @brief Initialize the input subsystem.
   * @return Unique pointer to deinitialization object.
   */
  [[nodiscard]] std::unique_ptr<platf::deinit_t> init();

  /**
   * @brief Probe for available gamepad devices.
   * @return `true` if gamepads were found, `false` otherwise.
   */
  bool probe_gamepads();

  /**
   * @brief Allocate a new input device.
   * @param mail Mailbox for communication.
   * @return Shared pointer to input device.
   */
  std::shared_ptr<input_t> alloc(safe::mail_t mail);

  /**
   * @brief Touch port configuration structure.
   */
  struct touch_port_t: public platf::touch_port_t {
    int env_width;  ///< Environment width
    int env_height;  ///< Environment height
    float client_offsetX;  ///< Client X coordinate offset
    float client_offsetY;  ///< Client Y coordinate offset
    float scalar_inv;  ///< Inverse scalar for coordinate transformation

    /**
     * @brief Check if touch port is valid.
     * @return `true` if all dimensions are non-zero, `false` otherwise.
     */
    explicit operator bool() const {
      return width != 0 && height != 0 && env_width != 0 && env_height != 0;
    }
  };

  /**
   * @brief Scale the ellipse axes according to the provided size.
   * @param val The major and minor axis pair.
   * @param rotation The rotation value from the touch/pen event.
   * @param scalar The scalar cartesian coordinate pair.
   * @return The major and minor axis pair.
   */
  std::pair<float, float> scale_client_contact_area(const std::pair<float, float> &val, uint16_t rotation, const std::pair<float, float> &scalar);
}  // namespace input
