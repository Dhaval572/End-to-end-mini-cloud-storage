#pragma once
#include <vector>
#include <string>
#include <sodium.h>

class Encryption
{
public:
    Encryption();
    ~Encryption();

    std::vector<unsigned char> Encrypt(
        const std::vector<unsigned char>& data,
        const std::string& key_context
    );

    std::vector<unsigned char> Decrypt(
        const std::vector<unsigned char>& encrypted_data,
        const std::string& key_context
    );

private:
    std::vector<unsigned char> DeriveKey(const std::string& context);
    bool m_initialized;
};