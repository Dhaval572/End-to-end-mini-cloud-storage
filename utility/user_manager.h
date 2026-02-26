#pragma once
#include <string>
#include <map>
#include <array>
#include <sodium.h>

class UserManager
{
private:
    struct t_UserData
    {
        std::string password_hash;
        size_t storage_used;
    };
    
    std::map<std::string, t_UserData> m_users;
    size_t m_storage_limit;
    std::string m_data_file;

    void LoadUsers();
    void SaveUsers();

public:
    UserManager();
    ~UserManager();

    bool RegisterUser(const std::string& username, const std::string& password);
    bool AuthenticateUser(const std::string& username, const std::string& password);
    bool UserExists(const std::string& username);
    size_t GetStorageUsed(const std::string& username);
    size_t GetStorageLimit() const { return m_storage_limit; }
    void UpdateStorageUsage(const std::string& username, size_t bytes_added);
    std::array<std::string, 5> GetAllUsers();
};