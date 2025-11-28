/**
 * @file src/rtsp.h
 * @brief Declarations for RTSP streaming.
 */
#pragma once

// standard includes
#include <atomic>
#include <memory>
#include <list>

// local includes
#include "crypto.h"
#include "thread_safe.h"

#ifdef _WIN32
  #include <windows.h>
#endif

// Resolve circular dependencies
namespace stream {
  struct session_t;
}

namespace rtsp_stream {
  constexpr auto RTSP_SETUP_PORT = 21;  ///< RTSP setup port offset from base port

  /**
   * @brief Launch session structure containing client connection parameters.
   */
  struct launch_session_t {
    uint32_t id;  ///< Session identifier

    crypto::aes_t gcm_key;  ///< GCM encryption key
    crypto::aes_t iv;  ///< Initialization vector

    std::string av_ping_payload;  ///< AV ping payload for session identification
    uint32_t control_connect_data;  ///< Control connection data

    std::string device_name;  ///< Client device name
    std::string unique_id;  ///< Client unique identifier
    crypto::PERM perm;  ///< Client permissions

    bool input_only;  ///< Whether this is an input-only session
    bool host_audio;  ///< Whether to enable host audio capture
    bool auto_bitrate_enabled = false;  ///< Client-side auto bitrate checkbox state
    int auto_bitrate_min_kbps = 0;  ///< Client-requested minimum bitrate (0 = not set)
    int auto_bitrate_max_kbps = 0;  ///< Client-requested maximum bitrate (0 = not set)
    int width;  ///< Video width in pixels
    int height;  ///< Video height in pixels
    int fps;  ///< Frames per second
    int gcmap;  ///< Game controller mapping
    int surround_info;  ///< Surround sound information
    std::string surround_params;  ///< Surround sound parameters
    bool enable_hdr;  ///< Whether to enable HDR
    bool enable_sops;  ///< Whether to enable SOPS (Secure Operations)
    bool virtual_display;  ///< Whether to use virtual display
    uint32_t scale_factor;  ///< Display scale factor

    std::optional<crypto::cipher::gcm_t> rtsp_cipher;  ///< RTSP encryption cipher
    std::string rtsp_url_scheme;  ///< RTSP URL scheme (rtsp:// or rtsps://)
    uint32_t rtsp_iv_counter;  ///< RTSP IV counter for encryption

    std::list<crypto::command_entry_t> client_do_cmds;  ///< Commands to execute on client connect
    std::list<crypto::command_entry_t> client_undo_cmds;  ///< Commands to execute on client disconnect

  #ifdef _WIN32
    GUID display_guid{};  ///< Windows display GUID
  #endif
  };

  /**
   * @brief Raise/activate a launch session.
   * @param launch_session Shared pointer to the launch session.
   */
  void launch_session_raise(std::shared_ptr<launch_session_t> launch_session);

  /**
   * @brief Clear state for the specified launch session.
   * @param launch_session_id The ID of the session to clear.
   */
  void launch_session_clear(uint32_t launch_session_id);

  /**
   * @brief Get the number of active sessions.
   * @return Count of active sessions.
   */
  int session_count();

  /**
   * @brief Find a session by UUID.
   * @param uuid The session UUID.
   * @return Shared pointer to session, or nullptr if not found.
   */
  std::shared_ptr<stream::session_t>
  find_session(const std::string_view& uuid);

  /**
   * @brief Get all active session UUIDs.
   * @return List of session UUID strings.
   */
  std::list<std::string>
  get_all_session_uuids();

  /**
   * @brief Terminates all running streaming sessions.
   */
  void terminate_sessions();

  /**
   * @brief Runs the RTSP server loop.
   */
  void start();
}  // namespace rtsp_stream
