#include "user_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>

UserManager::UserManager() : m_storage_limit(100 * 1024 * 1024), m_data_file("users.txt")
{
    sodium_init();
    LoadUsers();
}

UserManager::~UserManager()
{
    SaveUsers();
}

void UserManager::LoadUsers()
{
    std::ifstream file(m_data_file);
    if (!file.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string username;
        std::string password_hash;
        size_t storage_used;

        if (std::getline(iss, username, ',') &&
            std::getline(iss, password_hash, ',') &&
            (iss >> storage_used))
        {
            m_users[username] = {password_hash, storage_used};
        }
    }
}

void UserManager::SaveUsers()
{
    std::ofstream file(m_data_file);
    for (const auto& [username, user_data] : m_users)
    {
        file << username << "," 
             << user_data.password_hash << "," 
             << user_data.storage_used << "\n";
    }
}

bool UserManager::RegisterUser(const std::string& username, const std::string& password)
{
    if (m_users.find(username) != m_users.end())
    {
        return false;
    }

    char hash[crypto_pwhash_STRBYTES];
    if 
    (
        crypto_pwhash_str
        (
            hash,
            password.c_str(),
            password.length(),
            crypto_pwhash_OPSLIMIT_MODERATE,
            crypto_pwhash_MEMLIMIT_MODERATE
        ) != 0
    )
    {
        return false;
    }

    m_users[username] = {hash, 0};
    SaveUsers();
    return true;
}

bool UserManager::AuthenticateUser(const std::string& username, const std::string& password)
{
    auto it = m_users.find(username);
    if (it == m_users.end())
    {
        return false;
    }

    return crypto_pwhash_str_verify
    (
        it->second.password_hash.c_str(),
        password.c_str(),
        password.length()
    ) == 0;
}

bool UserManager::UserExists(const std::string& username)
{
    return m_users.contains(username);
}

size_t UserManager::GetStorageUsed(const std::string& username)
{
    auto it = m_users.find(username);
    if (it == m_users.end())
    {
        return 0;
    }
    return it->second.storage_used;
}

void UserManager::UpdateStorageUsage(const std::string& username, size_t bytes_added)
{
    auto it = m_users.find(username);
    if (it != m_users.end())
    {
        it->second.storage_used += bytes_added;
        SaveUsers();
    }
}

std::vector<std::string> UserManager::GetAllUsers()
{
    std::vector<std::string> users;
    for (const auto& [username, _] : m_users)
    {
        users.push_back(username);
    }
    return users;
}