/**
 * @file src/httpcommon.h
 * @brief Declarations for common HTTP.
 */
#pragma once

// lib includes
#include <curl/curl.h>

// local includes
#include "network.h"
#include "thread_safe.h"
#include "uuid.h"

namespace http {

  /**
   * @brief Initialize the HTTP subsystem.
   * @return 0 on success, non-zero on error.
   */
  int init();

  /**
   * @brief Create credentials from private key and certificate.
   * @param pkey The private key in PEM format.
   * @param cert The certificate in PEM format.
   * @return 0 on success, non-zero on error.
   */
  int create_creds(const std::string &pkey, const std::string &cert);

  /**
   * @brief Save user credentials to file.
   * @param file The file path to save credentials to.
   * @param username The username.
   * @param password The password.
   * @param run_our_mouth Whether to print credentials to stdout (default: false).
   * @return 0 on success, non-zero on error.
   */
  int save_user_creds(
    const std::string &file,
    const std::string &username,
    const std::string &password,
    bool run_our_mouth = false
  );

  /**
   * @brief Reload user credentials from file.
   * @param file The file path to load credentials from.
   * @return 0 on success, non-zero on error.
   */
  int reload_user_creds(const std::string &file);

  /**
   * @brief Download a file from a URL.
   * @param url The URL to download from.
   * @param file The local file path to save to.
   * @param ssl_version The SSL/TLS version to use (default: TLSv1.2).
   * @return `true` on success, `false` on error.
   */
  bool download_file(const std::string &url, const std::string &file, long ssl_version = CURL_SSLVERSION_TLSv1_2);

  /**
   * @brief URL-escape a string.
   * @param url The string to escape.
   * @return URL-escaped string.
   */
  std::string url_escape(const std::string &url);

  /**
   * @brief Extract the host from a URL.
   * @param url The URL string.
   * @return The host portion of the URL.
   */
  std::string url_get_host(const std::string &url);

  extern std::string unique_id;  ///< Unique identifier for this instance
  extern uuid_util::uuid_t uuid;  ///< UUID for this instance
  extern net::net_e origin_web_ui_allowed;  ///< Network origin allowed for Web UI access

}  // namespace http
