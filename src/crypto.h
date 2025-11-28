/**
 * @file src/crypto.h
 * @brief Declarations for cryptography functions.
 */
#pragma once

// standard includes
#include <array>

// lib includes
#include <list>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <nlohmann/json.hpp>

// local includes
#include "utility.h"

namespace crypto {
  /**
   * @brief Credentials structure containing X.509 certificate and private key.
   */
  struct creds_t {
    std::string x509;  ///< X.509 certificate in PEM format
    std::string pkey;  ///< Private key in PEM format
  };

  /**
   * @brief Destroy an EVP_MD_CTX context.
   * @param ctx The message digest context to destroy.
   */
  void md_ctx_destroy(EVP_MD_CTX *);

  using sha256_t = std::array<std::uint8_t, SHA256_DIGEST_LENGTH>;  ///< SHA-256 hash type

  using aes_t = std::vector<std::uint8_t>;  ///< AES key type
  using x509_t = util::safe_ptr<X509, X509_free>;  ///< X.509 certificate type
  using x509_store_t = util::safe_ptr<X509_STORE, X509_STORE_free>;  ///< X.509 certificate store type
  using x509_store_ctx_t = util::safe_ptr<X509_STORE_CTX, X509_STORE_CTX_free>;  ///< X.509 certificate store context type
  using cipher_ctx_t = util::safe_ptr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free>;  ///< Cipher context type
  using md_ctx_t = util::safe_ptr<EVP_MD_CTX, md_ctx_destroy>;  ///< Message digest context type
  using bio_t = util::safe_ptr<BIO, BIO_free_all>;  ///< BIO (Basic I/O) type
  using pkey_t = util::safe_ptr<EVP_PKEY, EVP_PKEY_free>;  ///< Public/private key type
  using pkey_ctx_t = util::safe_ptr<EVP_PKEY_CTX, EVP_PKEY_CTX_free>;  ///< Public/private key context type
  using bignum_t = util::safe_ptr<BIGNUM, BN_free>;  ///< Big number type

  /**
   * @brief The permissions of a client.
   */
  enum class PERM: uint32_t {
    _reserved        = 1,

    _input           = _reserved << 8,   // Input permission group
    input_controller = _input << 0,      // Allow controller input
    input_touch      = _input << 1,      // Allow touch input
    input_pen        = _input << 2,      // Allow pen input
    input_mouse      = _input << 3,      // Allow mouse input
    input_kbd        = _input << 4,      // Allow keyboard input
    _all_inputs      = input_controller | input_touch | input_pen | input_mouse | input_kbd,

    _operation       = _input << 8,      // Operation permission group
    clipboard_set    = _operation << 0,  // Allow set clipboard from client
    clipboard_read   = _operation << 1,  // Allow read clipboard from host
    file_upload      = _operation << 2,  // Allow upload files to host
    file_dwnload     = _operation << 3,  // Allow download files from host
    server_cmd       = _operation << 4,  // Allow execute server cmd
    _all_opeiations  = clipboard_set | clipboard_read | file_upload | file_dwnload | server_cmd,

    _action          = _operation << 8,  // Action permission group
    list             = _action << 0,     // Allow list apps
    view             = _action << 1,     // Allow view streams
    launch           = _action << 2,     // Allow launch apps
    _allow_view      = view | launch,    // If no view permission is granted, disconnect the device upon permission update
    _all_actions     = list | view | launch,

    _default         = view | list,      // Default permissions for new clients
    _no              = 0,                // No permissions are granted
    _all             = _all_inputs | _all_opeiations | _all_actions, // All current permissions
  };

  /**
   * @brief Bitwise AND operator for PERM enum.
   * @param x First permission value.
   * @param y Second permission value.
   * @return Bitwise AND of the two permission values.
   */
  inline constexpr PERM
  operator&(PERM x, PERM y) {
    return static_cast<PERM>(static_cast<uint32_t>(x) & static_cast<uint32_t>(y));
  }

  /**
   * @brief Logical NOT operator for PERM enum.
   * @param p Permission value to check.
   * @return `true` if permission is zero (no permissions), `false` otherwise.
   */
  inline constexpr bool
  operator!(PERM p) {
    return static_cast<uint32_t>(p) == 0;
  }

  /**
   * @brief Command entry structure for pre/post commands.
   */
  struct command_entry_t {
    std::string cmd;  ///< Command to execute
    bool elevated;  ///< Whether to run the command with elevated privileges

    /**
     * @brief Serialize command entry to JSON.
     * @param entry The command entry to serialize.
     * @return JSON representation of the command entry.
     */
    static inline nlohmann::json serialize(const command_entry_t& entry) {
      nlohmann::json node;
      node["cmd"] = entry.cmd;
      node["elevated"] = entry.elevated;
      return node;
    }
  };

  /**
   * @brief Named certificate structure for client authentication.
   */
  struct named_cert_t {
    std::string name;  ///< Client device name
    std::string uuid;  ///< Client unique identifier
    std::string cert;  ///< Client certificate in PEM format
    std::string display_mode;  ///< Display mode configuration
    std::list<command_entry_t> do_cmds;  ///< Commands to execute when client connects
    std::list<command_entry_t> undo_cmds;  ///< Commands to execute when client disconnects
    PERM perm;  ///< Client permissions
    bool enable_legacy_ordering;  ///< Enable legacy command ordering
    bool allow_client_commands;  ///< Allow client to send commands
    bool always_use_virtual_display;  ///< Always use virtual display for this client
  };

  using p_named_cert_t = std::shared_ptr<named_cert_t>;  ///< Shared pointer to named certificate

  /**
   * @brief Hashes the given plaintext using SHA-256.
   * @param plaintext
   * @return The SHA-256 hash of the plaintext.
   */
  sha256_t hash(const std::string_view &plaintext);

  /**
   * @brief Generate an AES key from salt and PIN.
   * @param salt The salt value (16 bytes).
   * @param pin The PIN string.
   * @return Generated AES key.
   */
  aes_t gen_aes_key(const std::array<uint8_t, 16> &salt, const std::string_view &pin);

  /**
   * @brief Parse an X.509 certificate from PEM string.
   * @param x The PEM-encoded certificate string.
   * @return X.509 certificate object.
   */
  x509_t x509(const std::string_view &x);

  /**
   * @brief Parse a private key from PEM string.
   * @param k The PEM-encoded private key string.
   * @return Private key object.
   */
  pkey_t pkey(const std::string_view &k);

  /**
   * @brief Convert X.509 certificate to PEM string.
   * @param x509 The X.509 certificate object.
   * @return PEM-encoded certificate string.
   */
  std::string pem(x509_t &x509);

  /**
   * @brief Convert private key to PEM string.
   * @param pkey The private key object.
   * @return PEM-encoded private key string.
   */
  std::string pem(pkey_t &pkey);

  /**
   * @brief Sign data using SHA-256 and private key.
   * @param pkey The private key to use for signing.
   * @param data The data to sign.
   * @return Signature bytes.
   */
  std::vector<uint8_t> sign256(const pkey_t &pkey, const std::string_view &data);

  /**
   * @brief Verify a signature using SHA-256 and X.509 certificate.
   * @param x509 The X.509 certificate containing the public key.
   * @param data The original data that was signed.
   * @param signature The signature to verify.
   * @return `true` if signature is valid, `false` otherwise.
   */
  bool verify256(const x509_t &x509, const std::string_view &data, const std::string_view &signature);

  /**
   * @brief Generate X.509 certificate and private key credentials.
   * @param cn The common name (CN) for the certificate.
   * @param key_bits The key size in bits (e.g., 2048, 4096).
   * @return Credentials structure containing certificate and private key.
   */
  creds_t gen_creds(const std::string_view &cn, std::uint32_t key_bits);

  /**
   * @brief Get the signature from an X.509 certificate.
   * @param x The X.509 certificate.
   * @return Signature as string view.
   */
  std::string_view signature(const x509_t &x);

  /**
   * @brief Generate random bytes.
   * @param bytes Number of bytes to generate.
   * @return Random bytes as string.
   */
  std::string rand(std::size_t bytes);

  /**
   * @brief Generate random string from specified alphabet.
   * @param bytes Number of characters to generate.
   * @param alphabet The alphabet to use for generation (default: alphanumeric + special chars).
   * @return Random string.
   */
  std::string rand_alphabet(std::size_t bytes, const std::string_view &alphabet = std::string_view {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!%&()=-"});

  /**
   * @brief Certificate chain for client certificate verification.
   */
  class cert_chain_t {
  public:
    KITTY_DECL_CONSTR(cert_chain_t)

    /**
     * @brief Add a named certificate to the chain.
     * @param named_cert_p Shared pointer to the named certificate.
     */
    void add(p_named_cert_t& named_cert_p);

    /**
     * @brief Clear all certificates from the chain.
     */
    void clear();

    /**
     * @brief Verify a certificate against the certificate chain.
     * @param cert The certificate to verify.
     * @param named_cert_out Output parameter for the matched named certificate.
     * @return Error message string if verification fails, `nullptr` if successful.
     */
    const char *verify(x509_t::element_type *cert, p_named_cert_t& named_cert_out);

  private:
    std::vector<std::pair<p_named_cert_t, x509_store_t>> _certs;  ///< Certificate store pairs
    x509_store_ctx_t _cert_ctx;  ///< Certificate verification context
  };

  /**
   * @brief Cipher namespace for encryption/decryption operations.
   */
  namespace cipher {
    constexpr std::size_t tag_size = 16;  ///< GCM tag size in bytes

    /**
     * @brief Round size up to PKCS#7 padding boundary (16 bytes).
     * @param size The size to round up.
     * @return Size rounded up to nearest multiple of 16.
     */
    constexpr std::size_t round_to_pkcs7_padded(std::size_t size) {
      return ((size + 15) / 16) * 16;
    }

    /**
     * @brief Base cipher class for encryption/decryption contexts.
     */
    class cipher_t {
    public:
      cipher_ctx_t decrypt_ctx;  ///< Decryption context
      cipher_ctx_t encrypt_ctx;  ///< Encryption context
      aes_t key;  ///< AES key
      bool padding;  ///< Whether to use PKCS#7 padding
    };

    /**
     * @brief ECB (Electronic Codebook) mode cipher.
     */
    class ecb_t: public cipher_t {
    public:
      ecb_t() = default;
      ecb_t(ecb_t &&) noexcept = default;
      ecb_t &operator=(ecb_t &&) noexcept = default;

      /**
       * @brief Construct ECB cipher with key and padding option.
       * @param key The AES key.
       * @param padding Whether to use PKCS#7 padding (default: true).
       */
      ecb_t(const aes_t &key, bool padding = true);

      /**
       * @brief Encrypt plaintext using ECB mode.
       * @param plaintext The plaintext to encrypt.
       * @param cipher Output buffer for ciphertext.
       * @return Number of bytes written, or -1 on error.
       */
      int encrypt(const std::string_view &plaintext, std::vector<std::uint8_t> &cipher);

      /**
       * @brief Decrypt ciphertext using ECB mode.
       * @param cipher The ciphertext to decrypt.
       * @param plaintext Output buffer for plaintext.
       * @return Number of bytes written, or -1 on error.
       */
      int decrypt(const std::string_view &cipher, std::vector<std::uint8_t> &plaintext);
    };

    class gcm_t: public cipher_t {
    public:
      gcm_t() = default;
      gcm_t(gcm_t &&) noexcept = default;
      gcm_t &operator=(gcm_t &&) noexcept = default;

      gcm_t(const crypto::aes_t &key, bool padding = true);

      /**
       * @brief Encrypts the plaintext using AES GCM mode.
       * @param plaintext The plaintext data to be encrypted.
       * @param tag The buffer where the GCM tag will be written.
       * @param ciphertext The buffer where the resulting ciphertext will be written.
       * @param iv The initialization vector to be used for the encryption.
       * @return The total length of the ciphertext and GCM tag. Returns -1 in case of an error.
       */
      int encrypt(const std::string_view &plaintext, std::uint8_t *tag, std::uint8_t *ciphertext, aes_t *iv);

      /**
       * @brief Encrypts the plaintext using AES GCM mode.
       * length of cipher must be at least: round_to_pkcs7_padded(plaintext.size()) + crypto::cipher::tag_size
       * @param plaintext The plaintext data to be encrypted.
       * @param tagged_cipher The buffer where the resulting ciphertext and GCM tag will be written.
       * @param iv The initialization vector to be used for the encryption.
       * @return The total length of the ciphertext and GCM tag written into tagged_cipher. Returns -1 in case of an error.
       */
      int encrypt(const std::string_view &plaintext, std::uint8_t *tagged_cipher, aes_t *iv);

      int decrypt(const std::string_view &cipher, std::vector<std::uint8_t> &plaintext, aes_t *iv);
    };

    /**
     * @brief CBC (Cipher Block Chaining) mode cipher.
     */
    class cbc_t: public cipher_t {
    public:
      cbc_t() = default;
      cbc_t(cbc_t &&) noexcept = default;
      cbc_t &operator=(cbc_t &&) noexcept = default;

      /**
       * @brief Construct CBC cipher with key and padding option.
       * @param key The AES key.
       * @param padding Whether to use PKCS#7 padding (default: true).
       */
      cbc_t(const crypto::aes_t &key, bool padding = true);

      /**
       * @brief Encrypts the plaintext using AES CBC mode.
       * @note Length of cipher must be at least: round_to_pkcs7_padded(plaintext.size())
       * @param plaintext The plaintext data to be encrypted.
       * @param cipher The buffer where the resulting ciphertext will be written.
       * @param iv The initialization vector to be used for the encryption.
       * @return The total length of the ciphertext written into cipher. Returns -1 in case of an error.
       */
      int encrypt(const std::string_view &plaintext, std::uint8_t *cipher, aes_t *iv);
    };
  }  // namespace cipher
}  // namespace crypto
