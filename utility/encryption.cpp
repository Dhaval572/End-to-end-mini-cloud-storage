#include "encryption.h"
#include <stdexcept>

Encryption::Encryption()
{
    if (sodium_init() < 0)
    {
        throw std::runtime_error("Failed to initialize libsodium");
    }
    m_initialized = true;
}

Encryption::~Encryption() {}

std::vector<unsigned char> Encryption::DeriveKey(const std::string& context)
{
    std::vector<unsigned char> key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES);
    
    crypto_hash_sha256
    (
        key.data(),
        reinterpret_cast<const unsigned char*>(context.c_str()),
        context.length()
    );
    
    return key;
}

std::vector<unsigned char> Encryption::Encrypt
(
    const std::vector<unsigned char>& data,
    const std::string& key_context
)
{
    std::vector<unsigned char> key = DeriveKey(key_context);
    
    std::vector<unsigned char> nonce(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    randombytes_buf(nonce.data(), nonce.size());

    std::vector<unsigned char> ciphertext
    (
        data.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES
    );

    unsigned long long ciphertext_length;

    crypto_aead_xchacha20poly1305_ietf_encrypt
    (
        ciphertext.data(),
        &ciphertext_length,
        data.data(),
        data.size(),
        nullptr,
        0,
        nullptr,
        nonce.data(),
        key.data()
    );

    std::vector<unsigned char> result;
    result.insert(result.end(), nonce.begin(), nonce.end());
    result.insert
    (
        result.end(), 
        ciphertext.begin(), 
        ciphertext.begin() + ciphertext_length
    );

    sodium_memzero(key.data(), key.size());
    
    return result;
}

std::vector<unsigned char> Encryption::Decrypt
(
    const std::vector<unsigned char>& encrypted_data,
    const std::string& key_context
)
{
    if (encrypted_data.size() < crypto_aead_xchacha20poly1305_ietf_NPUBBYTES)
    {
        return {};
    }

    std::vector<unsigned char> key = DeriveKey(key_context);

    std::vector<unsigned char> nonce
    (
        encrypted_data.begin(),
        encrypted_data.begin() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
    );

    std::vector<unsigned char> actual_ciphertext
    (
        encrypted_data.begin() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES,
        encrypted_data.end()
    );

    std::vector<unsigned char> plaintext(actual_ciphertext.size());
    unsigned long long plaintext_length;

    if 
    (
        crypto_aead_xchacha20poly1305_ietf_decrypt
        (
            plaintext.data(),
            &plaintext_length,
            nullptr,
            actual_ciphertext.data(),
            actual_ciphertext.size(),
            nullptr,
            0,
            nonce.data(),
            key.data()
        ) != 0
    )
    {
        sodium_memzero(key.data(), key.size());
        return {};
    }

    plaintext.resize(plaintext_length);
    sodium_memzero(key.data(), key.size());
    
    return plaintext;
}