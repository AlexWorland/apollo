/**
 * @file src/config.h
 * @brief Declarations for the configuration of Sunshine.
 */
#pragma once

// standard includes
#include <bitset>
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// local includes
#include "nvenc/nvenc_config.h"

namespace config {
  // track modified config options
  inline std::unordered_map<std::string, std::string> modified_config_settings;

  struct video_t {
    bool headless_mode;
    bool limit_framerate;
    bool double_refreshrate;
    // ffmpeg params
    int qp;  // higher == more compression and less quality

    int hevc_mode;
    int av1_mode;

    int min_threads;  // Minimum number of threads/slices for CPU encoding

    struct {
      std::string sw_preset;
      std::string sw_tune;
      std::optional<int> svtav1_preset;
    } sw;

    nvenc::nvenc_config nv;
    bool nv_realtime_hags;
    bool nv_opengl_vulkan_on_dxgi;
    bool nv_sunshine_high_power_mode;

    struct {
      int preset;
      int multipass;
      int h264_coder;
      int aq;
      int vbv_percentage_increase;
    } nv_legacy;

    struct {
      std::optional<int> qsv_preset;
      std::optional<int> qsv_cavlc;
      bool qsv_slow_hevc;
    } qsv;

    struct {
      std::optional<int> amd_usage_h264;
      std::optional<int> amd_usage_hevc;
      std::optional<int> amd_usage_av1;
      std::optional<int> amd_rc_h264;
      std::optional<int> amd_rc_hevc;
      std::optional<int> amd_rc_av1;
      std::optional<int> amd_enforce_hrd;
      std::optional<int> amd_quality_h264;
      std::optional<int> amd_quality_hevc;
      std::optional<int> amd_quality_av1;
      std::optional<int> amd_preanalysis;
      std::optional<int> amd_vbaq;
      int amd_coder;
    } amd;

    struct {
      int vt_allow_sw;
      int vt_require_sw;
      int vt_realtime;
      int vt_coder;
    } vt;

    struct {
      bool strict_rc_buffer;
    } vaapi;

    std::string capture;
    std::string encoder;
    std::string adapter_name;
    std::string output_name;

    struct dd_t {
      struct workarounds_t {
        std::chrono::milliseconds hdr_toggle_delay;  ///< Specify whether to apply HDR high-contrast color workaround and what delay to use.
      };

      enum class config_option_e {
        disabled,  ///< Disable the configuration for the device.
        verify_only,  ///< @seealso{display_device::SingleDisplayConfiguration::DevicePreparation}
        ensure_active,  ///< @seealso{display_device::SingleDisplayConfiguration::DevicePreparation}
        ensure_primary,  ///< @seealso{display_device::SingleDisplayConfiguration::DevicePreparation}
        ensure_only_display  ///< @seealso{display_device::SingleDisplayConfiguration::DevicePreparation}
      };

      enum class resolution_option_e {
        disabled,  ///< Do not change resolution.
        automatic,  ///< Change resolution and use the one received from Moonlight.
        manual  ///< Change resolution and use the manually provided one.
      };

      enum class refresh_rate_option_e {
        disabled,  ///< Do not change refresh rate.
        automatic,  ///< Change refresh rate and use the one received from Moonlight.
        manual  ///< Change refresh rate and use the manually provided one.
      };

      enum class hdr_option_e {
        disabled,  ///< Do not change HDR settings.
        automatic  ///< Change HDR settings and use the state requested by Moonlight.
      };

      struct mode_remapping_entry_t {
        std::string requested_resolution;
        std::string requested_fps;
        std::string final_resolution;
        std::string final_refresh_rate;
      };

      struct mode_remapping_t {
        std::vector<mode_remapping_entry_t> mixed;  ///< To be used when `resolution_option` and `refresh_rate_option` is set to `automatic`.
        std::vector<mode_remapping_entry_t> resolution_only;  ///< To be use when only `resolution_option` is set to `automatic`.
        std::vector<mode_remapping_entry_t> refresh_rate_only;  ///< To be use when only `refresh_rate_option` is set to `automatic`.
      };

      config_option_e configuration_option;
      resolution_option_e resolution_option;
      std::string manual_resolution;  ///< Manual resolution in case `resolution_option == resolution_option_e::manual`.
      refresh_rate_option_e refresh_rate_option;
      std::string manual_refresh_rate;  ///< Manual refresh rate in case `refresh_rate_option == refresh_rate_option_e::manual`.
      hdr_option_e hdr_option;
      std::chrono::milliseconds config_revert_delay;  ///< Time to wait until settings are reverted (after stream ends/app exists).
      bool config_revert_on_disconnect;  ///< Specify whether to revert display configuration on client disconnect.
      mode_remapping_t mode_remapping;
      workarounds_t wa;
    } dd;

    int max_bitrate;  // Maximum bitrate, sets ceiling in kbps for bitrate requested from client
    double minimum_fps_target;  ///< Lowest framerate that will be used when streaming. Range 0-1000, 0 = half of client's requested framerate.

    // Auto bitrate adjustment settings (only used when client enables it)
    // Note: Feature is controlled by client checkbox, these are host-side tuning parameters
    int auto_bitrate_min_kbps = 1;  // Minimum bitrate (Kbps)
    int auto_bitrate_max_kbps = 0;    // Maximum bitrate (0 = use client max)
    int auto_bitrate_adjustment_interval_ms = 3000;  // Minimum time between adjustments
    int auto_bitrate_min_adjustment_pct = 5;  // Minimum % delta required to change bitrate
    int auto_bitrate_loss_severe_pct = 10;    // Loss percentage considered severe
    int auto_bitrate_loss_moderate_pct = 5;   // Loss percentage considered moderate
    int auto_bitrate_loss_mild_pct = 1;       // Loss percentage considered mild
    int auto_bitrate_decrease_severe_pct = 25;   // Reduce bitrate by this percent on severe loss
    int auto_bitrate_decrease_moderate_pct = 12; // Reduce bitrate by this percent on moderate loss
    int auto_bitrate_decrease_mild_pct = 5;      // Reduce bitrate by this percent on mild loss
    int auto_bitrate_increase_good_pct = 5;       // Increase bitrate by this percent when stable
    int auto_bitrate_good_stability_ms = 5000;    // Good-network duration required before increases
    int auto_bitrate_increase_min_interval_ms = 3000; // Minimum interval between increases
    int auto_bitrate_poor_status_cap_pct = 25;    // Cap reduction percent when status == POOR

    std::string fallback_mode;
    bool isolated_virtual_display_option;
    bool ignore_encoder_probe_failure;
  };

  /**
   * @brief Audio configuration structure.
   */
  struct audio_t {
    std::string sink;  ///< Audio sink name
    std::string virtual_sink;  ///< Virtual audio sink name
    bool stream;  ///< Whether to stream audio
    bool install_steam_drivers;  ///< Whether to install Steam audio drivers
    bool keep_default;  ///< Whether to keep default audio sink
    bool auto_capture;  ///< Whether to auto-capture audio
  };

  /**
   * @def ENCRYPTION_MODE_NEVER
   * @brief Encryption mode: never use video encryption, even if client supports it.
   */
  constexpr int ENCRYPTION_MODE_NEVER = 0;

  /**
   * @def ENCRYPTION_MODE_OPPORTUNISTIC
   * @brief Encryption mode: use video encryption if available, but stream without it if not supported.
   */
  constexpr int ENCRYPTION_MODE_OPPORTUNISTIC = 1;

  /**
   * @def ENCRYPTION_MODE_MANDATORY
   * @brief Encryption mode: always use video encryption and refuse clients that can't encrypt.
   */
  constexpr int ENCRYPTION_MODE_MANDATORY = 2;

  /**
   * @brief Streaming configuration structure.
   */
  struct stream_t {
    std::chrono::milliseconds ping_timeout;  ///< Ping timeout duration
    std::string file_apps;  ///< Applications configuration file path
    int fec_percentage;  ///< Forward Error Correction percentage
    int lan_encryption_mode;  ///< Video encryption mode for LAN streams (ENCRYPTION_MODE_*)
    int wan_encryption_mode;  ///< Video encryption mode for WAN streams (ENCRYPTION_MODE_*)
  };

  /**
   * @brief nvhttp (GameStream HTTP) configuration structure.
   */
  struct nvhttp_t {
    std::string origin_web_ui_allowed;  ///< Allowed origins for Web UI (pc|lan|wan)
    std::string pkey;  ///< Private key file path
    std::string cert;  ///< Certificate file path
    std::string sunshine_name;  ///< Sunshine server name
    std::string file_state;  ///< State file path
    std::string external_ip;  ///< External IP address
  };

  /**
   * @brief Input configuration structure.
   */
  struct input_t {
    std::unordered_map<int, int> keybindings;  ///< Key binding mappings
    std::chrono::milliseconds back_button_timeout;  ///< Back button timeout duration
    std::chrono::milliseconds key_repeat_delay;  ///< Key repeat delay
    std::chrono::duration<double> key_repeat_period;  ///< Key repeat period
    std::string gamepad;  ///< Gamepad configuration
    bool ds4_back_as_touchpad_click;  ///< Use DS4 back button as touchpad click
    bool motion_as_ds4;  ///< Use motion controls as DS4
    bool touchpad_as_ds4;  ///< Use touchpad as DS4
    bool ds5_inputtino_randomize_mac;  ///< Randomize DS5 MAC address with inputtino
    bool keyboard;  ///< Enable keyboard input
    bool mouse;  ///< Enable mouse input
    bool controller;  ///< Enable controller input
    bool always_send_scancodes;  ///< Always send scancodes
    bool high_resolution_scrolling;  ///< Enable high-resolution scrolling
    bool native_pen_touch;  ///< Enable native pen/touch input
    bool enable_input_only_mode;  ///< Enable input-only mode
    bool forward_rumble;  ///< Forward rumble/haptic feedback
  };

  namespace flag {
    enum flag_e : std::size_t {
      PIN_STDIN = 0,  ///< Read PIN from stdin instead of http
      FRESH_STATE,  ///< Do not load or save state
      FORCE_VIDEO_HEADER_REPLACE,  ///< force replacing headers inside video data
      UPNP,  ///< Try Universal Plug 'n Play
      CONST_PIN,  ///< Use "universal" pin
      FLAG_SIZE  ///< Number of flags
    };
  }  // namespace flag

  /**
   * @brief Pre/post command structure for application lifecycle management.
   */
  struct prep_cmd_t {
    /**
     * @brief Construct prep command with do and undo commands.
     * @param do_cmd Command to execute.
     * @param undo_cmd Command to undo the do command.
     * @param elevated Whether to run with elevated privileges.
     */
    prep_cmd_t(std::string &&do_cmd, std::string &&undo_cmd, bool &&elevated):
        do_cmd(std::move(do_cmd)),
        undo_cmd(std::move(undo_cmd)),
        elevated(std::move(elevated)) {
    }

    /**
     * @brief Construct prep command with only do command.
     * @param do_cmd Command to execute.
     * @param elevated Whether to run with elevated privileges.
     */
    explicit prep_cmd_t(std::string &&do_cmd, bool &&elevated):
        do_cmd(std::move(do_cmd)),
        elevated(std::move(elevated)) {
    }

    std::string do_cmd;  ///< Command to execute
    std::string undo_cmd;  ///< Command to undo (optional)
    bool elevated;  ///< Whether to run with elevated privileges
  };

  /**
   * @brief Server command structure.
   */
  struct server_cmd_t {
    /**
     * @brief Construct server command.
     * @param cmd_name Command name.
     * @param cmd_val Command value.
     * @param elevated Whether to run with elevated privileges.
     */
    server_cmd_t(std::string &&cmd_name, std::string &&cmd_val, bool &&elevated):
        cmd_name(std::move(cmd_name)),
        cmd_val(std::move(cmd_val)),
        elevated(std::move(elevated)) {
    }
    std::string cmd_name;  ///< Command name
    std::string cmd_val;  ///< Command value
    bool elevated;  ///< Whether to run with elevated privileges
  };

  /**
   * @brief Main Sunshine configuration structure.
   */
  /**
   * @brief Main Sunshine configuration structure.
   */
  struct sunshine_t {
    bool hide_tray_controls;  ///< Hide system tray controls
    bool enable_pairing;  ///< Enable device pairing
    bool enable_discovery;  ///< Enable device discovery
    bool envvar_compatibility_mode;  ///< Enable environment variable compatibility mode
    std::string locale;  ///< Locale string
    int min_log_level;  ///< Minimum log level
    std::bitset<flag::FLAG_SIZE> flags;  ///< Configuration flags
    std::string credentials_file;  ///< Credentials file path
    std::string username;  ///< Username for authentication
    std::string password;  ///< Password for authentication
    std::string salt;  ///< Salt for password hashing
    std::string config_file;  ///< Configuration file path

    /**
     * @brief Command structure.
     */
    struct cmd_t {
      std::string name;  ///< Command name
      int argc;  ///< Argument count
      char **argv;  ///< Argument vector
    } cmd;  ///< Command configuration

    std::uint16_t port;  ///< Server port
    std::string address_family;  ///< Address family (IPv4/IPv6)
    std::string log_file;  ///< Log file path
    bool notify_pre_releases;  ///< Notify about pre-releases
    bool legacy_ordering;  ///< Use legacy ordering
    bool system_tray;  ///< Enable system tray
    std::vector<prep_cmd_t> prep_cmds;  ///< Pre-application commands
    std::vector<prep_cmd_t> state_cmds;  ///< State management commands
    std::vector<server_cmd_t> server_cmds;  ///< Server commands
  };

  /**
   * @brief Auto bitrate adjustment settings structure.
   */
  struct auto_bitrate_settings_t {
    int min_kbps;  ///< Minimum bitrate in Kbps
    int max_kbps;  ///< Maximum bitrate in Kbps
    int adjustment_interval_ms;  ///< Minimum time between adjustments in milliseconds
    int min_adjustment_pct;  ///< Minimum percentage change required to adjust
    int loss_severe_pct;  ///< Loss percentage considered severe
    int loss_moderate_pct;  ///< Loss percentage considered moderate
    int loss_mild_pct;  ///< Loss percentage considered mild
    int decrease_severe_pct;  ///< Bitrate decrease percentage for severe loss
    int decrease_moderate_pct;  ///< Bitrate decrease percentage for moderate loss
    int decrease_mild_pct;  ///< Bitrate decrease percentage for mild loss
    int increase_good_pct;  ///< Bitrate increase percentage when network is good
    int good_stability_ms;  ///< Duration of good network required before increases
    int increase_min_interval_ms;  ///< Minimum interval between increases
    int poor_status_cap_pct;  ///< Cap reduction percentage when status is poor
    int max_bitrate_cap;  ///< Maximum bitrate cap
  };

  extern video_t video;
  extern audio_t audio;
  extern stream_t stream;
  extern nvhttp_t nvhttp;
  extern input_t input;
  extern sunshine_t sunshine;

  int parse(int argc, char *argv[]);
  std::unordered_map<std::string, std::string> parse_config(const std::string_view &file_content);
  void apply_config(std::unordered_map<std::string, std::string> &&vars);
  auto_bitrate_settings_t get_auto_bitrate_settings();
}  // namespace config
