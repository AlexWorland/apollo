/**
 * @file src/process.h
 * @brief Declarations for the startup and shutdown of the apps started by a streaming Session.
 */
#pragma once

#ifndef __kernel_entry
  #define __kernel_entry
#endif

#ifndef BOOST_PROCESS_VERSION
  #define BOOST_PROCESS_VERSION 1
#endif

// standard includes
#include <optional>
#include <unordered_map>

// lib includes
#include <boost/process/v1/child.hpp>
#include <boost/process/v1/group.hpp>
#include <boost/process/v1/environment.hpp>
#include <boost/process/v1/search_path.hpp>
#include <boost/property_tree/ptree.hpp>
#include <nlohmann/json.hpp>

// local includes
#include "config.h"
#include "platform/common.h"
#include "rtsp.h"
#include "utility.h"

#ifdef _WIN32
  #include "platform/windows/virtual_display.h"
#endif

#define VIRTUAL_DISPLAY_UUID "8902CB19-674A-403D-A587-41B092E900BA"
#define FALLBACK_DESKTOP_UUID "EAAC6159-089A-46A9-9E24-6436885F6610"
#define REMOTE_INPUT_UUID "8CB5C136-DA67-4F99-B4A1-F9CD35005CF4"
#define TERMINATE_APP_UUID "E16CBE1B-295D-4632-9A76-EC4180C857D3"

namespace proc {
  using file_t = util::safe_ptr_v2<FILE, int, fclose>;

#ifdef _WIN32
  extern VDISPLAY::DRIVER_STATUS vDisplayDriverStatus;
#endif

  typedef config::prep_cmd_t cmd_t;  ///< Command type alias

  /**
   * @brief Application context structure.
   * @details Contains configuration for launching and managing applications.
   */
  struct ctx_t {
    std::vector<cmd_t> prep_cmds;  ///< Pre-commands: guaranteed to be executed unless any fail
    std::vector<cmd_t> state_cmds;  ///< State commands for display configuration

    /**
     * @brief Detached commands that run independently of Sunshine.
     * @details Some applications (e.g., Steam) either exit quickly or run indefinitely.
     *          Apps with normal child processes are handled by process grouping (wait_all).
     *          Apps with indirect child processes (e.g., UWP) use auto-detach heuristic.
     *          For background processes that shouldn't be managed, use detached commands.
     */
    std::vector<std::string> detached;

    std::string idx;  ///< Application index
    std::string uuid;  ///< Application UUID
    std::string name;  ///< Application name
    std::string cmd;  ///< Command to run (runs until session ends or command exits)
    std::string working_dir;  ///< Process working directory (required for some games)
    std::string output;  ///< Output handling: empty = append to sunshine output, "null" = discard, filename = append to file
    std::string image_path;  ///< Application image/icon path
    std::string id;  ///< Application ID
    std::string gamepad;  ///< Gamepad configuration
    bool elevated;  ///< Whether to run with elevated privileges
    bool auto_detach;  ///< Auto-detach if process exits quickly
    bool wait_all;  ///< Wait for all child processes
    bool virtual_display;  ///< Use virtual display
    bool virtual_display_primary;  ///< Make virtual display primary
    bool use_app_identity;  ///< Use application identity
    bool per_client_app_identity;  ///< Use per-client application identity
    bool allow_client_commands;  ///< Allow client to send commands
    bool terminate_on_pause;  ///< Terminate application when paused
    int  scale_factor;  ///< Display scale factor
    std::chrono::seconds exit_timeout;  ///< Timeout for graceful exit
  };

  /**
   * @brief Process manager class for launching and managing applications.
   */
  class proc_t {
  public:
    KITTY_DEFAULT_CONSTR_MOVE_THROW(proc_t)

    std::string display_name;  ///< Current display name
    std::string initial_display;  ///< Initial display name
    std::string mode_changed_display;  ///< Display name after mode change
    bool initial_hdr = false;  ///< Initial HDR state
    bool virtual_display = false;  ///< Whether virtual display is active
    bool allow_client_commands = false;  ///< Whether client commands are allowed

    /**
     * @brief Construct process manager with environment and applications.
     * @param env Process environment variables.
     * @param apps List of application contexts.
     */
    proc_t(
      boost::process::v1::environment &&env,
      std::vector<ctx_t> &&apps
    ):
        _env(std::move(env)),
        _apps(std::move(apps)) {
    }

    /**
     * @brief Launch input-only mode (no application).
     */
    void launch_input_only();

    /**
     * @brief Execute an application.
     * @param _app The application context.
     * @param launch_session The launch session parameters.
     * @return Application ID on success, 0 on failure.
     */
    int execute(const ctx_t& _app, std::shared_ptr<rtsp_stream::launch_session_t> launch_session);

    /**
     * @brief Get the currently running application ID.
     * @return Application ID if a process is running, otherwise 0.
     */
    int running();

    ~proc_t();

    /**
     * @brief Get applications list (const).
     * @return Const reference to applications vector.
     */
    const std::vector<ctx_t> &get_apps() const;

    /**
     * @brief Get applications list.
     * @return Reference to applications vector.
     */
    std::vector<ctx_t> &get_apps();

    /**
     * @brief Get application image path.
     * @param app_id The application ID.
     * @return Image path string.
     */
    std::string get_app_image(int app_id);

    /**
     * @brief Get the name of the last run application.
     * @return Application name string.
     */
    std::string get_last_run_app_name();

    /**
     * @brief Get the UUID of the currently running application.
     * @return Application UUID string.
     */
    std::string get_running_app_uuid();

    /**
     * @brief Get the process environment.
     * @return Environment object.
     */
    boost::process::v1::environment get_env();

    /**
     * @brief Resume a paused application.
     */
    void resume();

    /**
     * @brief Pause a running application.
     */
    void pause();

    /**
     * @brief Terminate the running application.
     * @param immediate Whether to terminate immediately (default: false).
     * @param needs_refresh Whether to refresh the display (default: true).
     */
    void terminate(bool immediate = false, bool needs_refresh = true);

  private:
    int _app_id = 0;  ///< Current application ID
    std::string _app_name;  ///< Current application name
    boost::process::v1::environment _env;  ///< Process environment
    std::shared_ptr<rtsp_stream::launch_session_t> _launch_session;  ///< Launch session
    std::shared_ptr<config::input_t> _saved_input_config;  ///< Saved input configuration
    std::vector<ctx_t> _apps;  ///< List of applications
    ctx_t _app;  ///< Current application context
    std::chrono::steady_clock::time_point _app_launch_time;  ///< Application launch time
    bool placebo {};  ///< True if no command associated with _app_id but process still running
    boost::process::v1::child _process;  ///< Child process
    boost::process::v1::group _process_group;  ///< Process group
    file_t _pipe;  ///< Process pipe
    std::vector<cmd_t>::const_iterator _app_prep_it;  ///< Current prep command iterator
    std::vector<cmd_t>::const_iterator _app_prep_begin;  ///< Beginning of prep commands
  };

  /**
   * @brief Find the working directory for a command.
   * @param cmd The command string.
   * @param env The process environment.
   * @return Working directory path.
   */
  boost::filesystem::path
  find_working_directory(const std::string &cmd, const boost::process::v1::environment &env);

  /**
   * @brief Calculate a stable application ID based on name and image data.
   * @param app_name The application name.
   * @param app_image_path The application image path.
   * @param index The application index.
   * @return Tuple of ID without index and ID with index.
   */
  std::tuple<std::string, std::string> calculate_app_id(const std::string &app_name, std::string app_image_path, int index);

  /**
   * @brief Validate and normalize application image path.
   * @param app_image_path The image path to validate.
   * @return Validated image path.
   */
  std::string validate_app_image_path(std::string app_image_path);

  /**
   * @brief Refresh applications from file.
   * @param file_name The applications file name.
   * @param needs_terminate Whether to terminate running apps (default: true).
   */
  void refresh(const std::string &file_name, bool needs_terminate = true);

  /**
   * @brief Migrate applications from old format to new format.
   * @param fileTree_p Pointer to file tree JSON.
   * @param inputTree_p Pointer to input tree JSON.
   */
  void migrate_apps(nlohmann::json* fileTree_p, nlohmann::json* inputTree_p);

  /**
   * @brief Parse applications from file.
   * @param file_name The applications file name.
   * @return Optional process manager instance.
   */
  std::optional<proc::proc_t> parse(const std::string &file_name);

  /**
   * @brief Initialize proc functions
   * @return Unique pointer to `deinit_t` to manage cleanup
   */
  std::unique_ptr<platf::deinit_t> init();

  /**
   * @brief Terminates all child processes in a process group.
   * @param proc The child process itself.
   * @param group The group of all children in the process tree.
   * @param exit_timeout The timeout to wait for the process group to gracefully exit.
   */
  void terminate_process_group(boost::process::v1::child &proc, boost::process::v1::group &group, std::chrono::seconds exit_timeout);

  extern proc_t proc;

  extern int input_only_app_id;
  extern std::string input_only_app_id_str;
  extern int terminate_app_id;
  extern std::string terminate_app_id_str;
}  // namespace proc

#ifdef BOOST_PROCESS_VERSION
  #undef BOOST_PROCESS_VERSION
#endif
