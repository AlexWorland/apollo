/**
 * @file src/stream.h
 * @brief Declarations for the streaming protocols.
 */
#pragma once

// standard includes
#include <atomic>
#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// lib includes
#include <boost/asio.hpp>

extern "C" {
  // clang-format off
#include <moonlight-common-c/src/Limelight-internal.h>
  // clang-format on
}

// local includes
#include "audio.h"
#include "crypto.h"
#include "input.h"
#include "network.h"
#include "platform/common.h"
#include "sync.h"
#include "thread_safe.h"
#include "utility.h"
#include "video.h"

namespace stream {
  constexpr auto VIDEO_STREAM_PORT = 9;
  constexpr auto CONTROL_PORT = 10;
  constexpr auto AUDIO_STREAM_PORT = 11;

  // Forward declarations
  class control_server_t;
  struct session_t;
  
  namespace session {
    enum class state_e : int;
  }
}

// Forward declaration for rtsp_stream namespace
namespace rtsp_stream {
  struct launch_session_t;
}

namespace stream {

  /**
   * @brief Socket type enumeration.
   */
  enum class socket_e : int {
    video,  ///< Video socket
    audio  ///< Audio socket
  };

  namespace asio = boost::asio;
  using asio::ip::udp;
  using av_session_id_t = std::variant<asio::ip::address, std::string>;  ///< Session identifier: IP address or SS-Ping-Payload from RTSP handshake
  using message_queue_t = std::shared_ptr<safe::queue_t<std::pair<udp::endpoint, std::string>>>;  ///< Message queue type
  using message_queue_queue_t = std::shared_ptr<safe::queue_t<std::tuple<socket_e, av_session_id_t, message_queue_t>>>;  ///< Queue of message queues

  /**
   * @brief Control server for managing streaming sessions.
   */
  class control_server_t {
  public:
    int bind(net::af_e address_family, std::uint16_t port);

    /**
     * @brief Get session associated with peer address.
     * @details If no session is found, try to find an unclaimed session (marked by port 0).
     *          If none are found, return nullptr.
     * @param peer The ENet peer.
     * @param connect_data The connection data.
     * @return Pointer to session, or nullptr if not found.
     */
    session_t *get_session(const net::peer_t peer, uint32_t connect_data);

    /**
     * @brief Iterate over control messages and process them.
     * @note Circular dependency: iterate refers to session, session refers to broadcast_ctx_t,
     *       broadcast_ctx_t refers to control_server_t. Therefore, iterate is implemented
     *       further down in the source file.
     * @param timeout The timeout for waiting for messages.
     */
    void iterate(std::chrono::milliseconds timeout);

    /**
     * @brief Call the handler for a given control stream message.
     * @param type The message type.
     * @param session The session the message was received on.
     * @param payload The payload of the message.
     * @param reinjected `true` if this message is being reprocessed after decryption.
     */
    void call(std::uint16_t type, session_t *session, const std::string_view &payload, bool reinjected);

    /**
     * @brief Map a message type to a callback handler.
     * @param type The message type.
     * @param cb The callback function to handle the message.
     */
    void map(uint16_t type, std::function<void(session_t *, const std::string_view &)> cb);

    /**
     * @brief Send a payload to a peer.
     * @param payload The data to send.
     * @param peer The destination peer.
     * @return 0 on success, non-zero on error.
     */
    int send(const std::string_view &payload, net::peer_t peer);

    /**
     * @brief Flush pending messages.
     */
    void flush();

    std::unordered_map<std::uint16_t, std::function<void(session_t *, const std::string_view &)>> _map_type_cb;  ///< Message type to callback mapping
    sync_util::sync_t<std::vector<session_t *>> _sessions;  ///< All active sessions (including those waiting for peer connection)
    sync_util::sync_t<std::map<net::peer_t, session_t *>> _peer_to_session;  ///< ENet peer to session mapping
    ENetAddress _addr;  ///< ENet address
    net::host_t _host;  ///< ENet host
  };

  /**
   * @brief Broadcast context for managing streaming threads and sockets.
   */
  struct broadcast_ctx_t {
    message_queue_queue_t message_queue_queue;  ///< Queue of message queues

    std::thread recv_thread;  ///< Receive thread
    std::thread video_thread;  ///< Video processing thread
    std::thread audio_thread;  ///< Audio processing thread
    std::thread control_thread;  ///< Control message processing thread

    asio::io_context io_context;  ///< ASIO I/O context

    udp::socket video_sock {io_context};  ///< Video UDP socket
    udp::socket audio_sock {io_context};  ///< Audio UDP socket

    control_server_t control_server;  ///< Control server instance
  };

  /**
   * @brief Streaming configuration structure.
   */
  struct config_t {
    audio::config_t audio;  ///< Audio configuration
    video::config_t monitor;  ///< Video/monitor configuration

    int packetsize;  ///< Packet size in bytes
    int minRequiredFecPackets;  ///< Minimum required FEC packets
    int mlFeatureFlags;  ///< Moonlight feature flags
    int controlProtocolType;  ///< Control protocol type
    int audioQosType;  ///< Audio QoS type
    int videoQosType;  ///< Video QoS type

    uint32_t encryptionFlagsEnabled;  ///< Enabled encryption flags

    std::optional<int> gcmap;  ///< Game controller mapping (optional)
  };

#pragma pack(push, 1)
  /**
   * @brief Audio FEC packet structure (packed for network transmission).
   */
  struct audio_fec_packet_t {
    RTP_PACKET rtp;  ///< RTP packet header
    AUDIO_FEC_HEADER fecHeader;  ///< FEC header
  };
#pragma pack(pop)

  /**
   * @brief Streaming session structure.
   */
  struct session_t {
    config_t config;

    safe::mail_t mail;

    std::shared_ptr<input::input_t> input;

    std::thread audioThread;
    std::thread videoThread;

    std::chrono::steady_clock::time_point pingTimeout;

    safe::shared_t<broadcast_ctx_t>::ptr_t broadcast_ref;

    boost::asio::ip::address localAddress;

    struct {
      std::string ping_payload;  ///< Ping payload for session identification

      int lowseq;  ///< Lowest sequence number received
      boost::asio::ip::udp::endpoint peer;  ///< Video peer endpoint

      std::optional<crypto::cipher::gcm_t> cipher;  ///< GCM cipher for encryption
      std::uint64_t gcm_iv_counter;  ///< GCM IV counter

      safe::mail_raw_t::event_t<bool> idr_events;  ///< IDR frame request events
      safe::mail_raw_t::event_t<std::pair<int64_t, int64_t>> invalidate_ref_frames_events;  ///< Reference frame invalidation events

      std::unique_ptr<platf::deinit_t> qos;  ///< QoS handler
    } video;

    struct {
      crypto::cipher::cbc_t cipher;  ///< CBC cipher for audio encryption
      std::string ping_payload;  ///< Ping payload for session identification

      std::uint16_t sequenceNumber;  ///< Audio sequence number
      std::uint32_t avRiKeyId;  ///< Remote input key ID (big-endian first bytes of launch_session->iv)
      std::uint32_t timestamp;  ///< Audio timestamp
      boost::asio::ip::udp::endpoint peer;  ///< Audio peer endpoint

      util::buffer_t<char> shards;  ///< FEC shard buffer
      util::buffer_t<uint8_t *> shards_p;  ///< FEC shard pointer buffer

      audio_fec_packet_t fec_packet;  ///< FEC packet structure
      std::unique_ptr<platf::deinit_t> qos;  ///< QoS handler
    } audio;

    struct {
      crypto::cipher::gcm_t cipher;  ///< GCM cipher for control encryption
      crypto::aes_t legacy_input_enc_iv;  ///< Legacy input encryption IV (only for clients without full control stream encryption)
      crypto::aes_t incoming_iv;  ///< Incoming IV for control stream
      crypto::aes_t outgoing_iv;  ///< Outgoing IV for control stream

      std::uint32_t connect_data;  ///< Connection data (for new clients with ML_FF_SESSION_ID_V1)
      std::string expected_peer_address;  ///< Expected peer address (only for legacy clients without ML_FF_SESSION_ID_V1)

      net::peer_t peer;  ///< ENet peer
      std::uint32_t seq;  ///< Control sequence number

      platf::feedback_queue_t feedback_queue;  ///< Gamepad feedback queue
      safe::mail_raw_t::event_t<video::hdr_info_t> hdr_queue;  ///< HDR information queue
    } control;

    std::uint32_t launch_session_id;  ///< Associated launch session ID
    std::string device_name;  ///< Client device name
    std::string device_uuid;  ///< Client device UUID
    crypto::PERM permission;  ///< Client permissions

    std::list<crypto::command_entry_t> do_cmds;  ///< Commands to execute on session start
    std::list<crypto::command_entry_t> undo_cmds;  ///< Commands to execute on session end

    safe::mail_raw_t::event_t<bool> shutdown_event;  ///< Shutdown event
    safe::signal_t controlEnd;  ///< Control stream end signal

    std::atomic<session::state_e> state;  ///< Current session state

    bool auto_bitrate_enabled = false;  ///< Enable auto bitrate for this session (set to true ONLY when client checkbox is checked; when false, use existing static bitrate flow)
    int auto_bitrate_min_kbps = 0;  ///< Client-requested minimum bitrate (0 = not set, use server config default)
    int auto_bitrate_max_kbps = 0;  ///< Client-requested maximum bitrate (0 = not set, use configured bitrate)
    bool auto_bitrate_v2_active = false;  ///< True once V2 telemetry is received for this session
    
    int bitrate_stats_send_counter = 0;  ///< Counter for periodic stats sending
    int last_sent_connection_status = -1;  ///< Track last sent status to detect changes
  };

  /**
   * @brief Session management functions.
   */
  namespace session {
    /**
     * @brief Session state enumeration.
     */
    enum class state_e : int {
      STOPPED,  ///< The session is stopped
      STOPPING,  ///< The session is stopping
      STARTING,  ///< The session is starting
      RUNNING,  ///< The session is running
    };

    /**
     * @brief Allocate a new streaming session.
     * @param config The streaming configuration.
     * @param launch_session The launch session parameters.
     * @return Shared pointer to the new session.
     */
    std::shared_ptr<session_t> alloc(config_t &config, rtsp_stream::launch_session_t &launch_session);

    /**
     * @brief Get the UUID of a session.
     * @param session The session.
     * @return Session UUID string.
     */
    std::string uuid(const session_t& session);

    /**
     * @brief Check if session UUID matches.
     * @param session The session.
     * @param uuid The UUID to match.
     * @return `true` if UUIDs match, `false` otherwise.
     */
    bool uuid_match(const session_t& session, const std::string_view& uuid);

    /**
     * @brief Update device information for a session.
     * @param session The session.
     * @param name The new device name.
     * @param newPerm The new permissions.
     * @return `true` on success, `false` on failure.
     */
    bool update_device_info(session_t& session, const std::string& name, const crypto::PERM& newPerm);

    /**
     * @brief Start a streaming session.
     * @param session The session to start.
     * @param addr_string The address string to connect to.
     * @return 0 on success, non-zero on error.
     */
    int start(session_t &session, const std::string &addr_string);

    /**
     * @brief Stop a streaming session.
     * @param session The session to stop.
     */
    void stop(session_t &session);

    /**
     * @brief Gracefully stop a streaming session.
     * @param session The session to stop.
     */
    void graceful_stop(session_t& session);

    /**
     * @brief Join session threads (wait for completion).
     * @param session The session.
     */
    void join(session_t &session);

    /**
     * @brief Get the current state of a session.
     * @param session The session.
     * @return Current session state.
     */
    state_e state(session_t &session);

    /**
     * @brief Send a control message to the session.
     * @param session The session.
     * @param payload The message payload.
     * @return `true` on success, `false` on failure.
     */
    inline bool send(session_t& session, const std::string_view &payload);
  }  // namespace session
}  // namespace stream
