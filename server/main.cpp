#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <httplib.h>
#include "common/user_manager.h"
#include "common/encryption.h"

namespace fs = std::filesystem;

class Server
{
private:
    httplib::Server m_server;
    UserManager m_user_manager;
    Encryption m_encryption;
    std::string m_storage_path;
    static const int PORT = 8080;
    static const int MAX_USERS = 5;
    std::mutex m_user_mutex;

public:
    Server() : m_storage_path("./storage")
    {
        InitializeStorage();
        SetupRoutes();
    }

    void Run()
    {
        std::cout << "======================================" << std::endl;
        std::cout << "E2Eye Server Starting..." << std::endl;
        std::cout << "======================================" << std::endl;
        std::cout << "Server listening on http://localhost:" << PORT << std::endl;
        std::cout << "Storage path: " << fs::absolute(m_storage_path) << std::endl;
        std::cout << "Users file: " << fs::absolute("users.txt") << std::endl;
        std::cout << "Max users allowed: " << MAX_USERS << std::endl;
        std::cout << "======================================" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        std::cout << "======================================" << std::endl;
        
        m_server.listen("0.0.0.0", PORT);
    }

private:
    void InitializeStorage()
    {
        fs::create_directories(m_storage_path);
    }

    void SetupRoutes()
    {
        // Root endpoint
        m_server.Get("/", [](const httplib::Request&, httplib::Response& res)
        {
            std::string message = 
                "E2Eye Server is running!\n"
                "Available endpoints:\n"
                "  POST /register - Register new user (max 5 users)\n"
                "  POST /login - User login\n"
                "  POST /upload - Upload file\n"
                "  GET /files - List files\n"
                "  GET /download - Download file\n"
                "  GET /storage - Get storage info\n"
                "  GET /users - List all users\n";
            
            res.set_content(message, "text/plain");
        });

        // Register endpoint with user limit
        m_server.Post("/register", [this](const httplib::Request& req, httplib::Response& res)
        {
            std::lock_guard<std::mutex> lock(m_user_mutex);
            
            std::cout << "\n=== REGISTER REQUEST ===" << std::endl;
            
            std::string username = req.get_param_value("username");
            std::string password = req.get_param_value("password");
            
            if (username.empty() && !req.body.empty())
            {
                size_t u_pos = req.body.find("username=");
                if (u_pos != std::string::npos)
                {
                    size_t u_end = req.body.find("&", u_pos);
                    username = req.body.substr(u_pos + 9, u_end - (u_pos + 9));
                }
                
                size_t p_pos = req.body.find("password=");
                if (p_pos != std::string::npos)
                {
                    size_t p_end = req.body.find("&", p_pos);
                    password = req.body.substr(p_pos + 9, p_end - (p_pos + 9));
                }
            }
            
            std::cout << "Username: '" << username << "'" << std::endl;
            std::cout << "Password length: " << password.length() << std::endl;
            
            if (username.empty() || password.empty())
            {
                std::cout << "ERROR: Missing credentials" << std::endl;
                res.status = 400;
                res.set_content("Username and password required", "text/plain");
                return;
            }
            
            // Check user limit
            auto all_users = m_user_manager.GetAllUsers();
            if (all_users.size() >= MAX_USERS)
            {
                std::cout << "ERROR: User limit reached (" << MAX_USERS << ")" << std::endl;
                res.status = 403;
                res.set_content("Server has reached maximum user limit (5)", "text/plain");
                return;
            }
            
            if (m_user_manager.UserExists(username))
            {
                std::cout << "ERROR: User already exists: " << username << std::endl;
                res.status = 409;
                res.set_content("Username already exists", "text/plain");
                return;
            }
            
            if (m_user_manager.RegisterUser(username, password))
            {
                try
                {
                    fs::create_directories(m_storage_path + "/" + username);
                    std::cout << "Created directory for: " << username << std::endl;
                }
                catch (const std::exception& e)
                {
                    std::cout << "Warning: " << e.what() << std::endl;
                }
                
                std::cout << "SUCCESS: User registered: " << username << std::endl;
                std::cout << "Total users now: " << (all_users.size() + 1) << "/" << MAX_USERS << std::endl;
                res.status = 200;
                res.set_content("Registration successful", "text/plain");
            }
            else
            {
                std::cout << "ERROR: Registration failed for: " << username << std::endl;
                res.status = 500;
                res.set_content("Registration failed - internal error", "text/plain");
            }
        });

        // Login endpoint
        m_server.Post("/login", [this](const httplib::Request& req, httplib::Response& res)
        {
            std::lock_guard<std::mutex> lock(m_user_mutex);
            
            std::cout << "\n=== LOGIN REQUEST ===" << std::endl;
            
            std::string username = req.get_param_value("username");
            std::string password = req.get_param_value("password");
            
            if (username.empty() && !req.body.empty())
            {
                size_t u_pos = req.body.find("username=");
                if (u_pos != std::string::npos)
                {
                    size_t u_end = req.body.find("&", u_pos);
                    username = req.body.substr(u_pos + 9, u_end - (u_pos + 9));
                }
                
                size_t p_pos = req.body.find("password=");
                if (p_pos != std::string::npos)
                {
                    size_t p_end = req.body.find("&", p_pos);
                    password = req.body.substr(p_pos + 9, p_end - (p_pos + 9));
                }
            }
            
            std::cout << "Username: '" << username << "'" << std::endl;
            std::cout << "Password length: " << password.length() << std::endl;
            
            if (username.empty() || password.empty())
            {
                std::cout << "ERROR: Missing credentials" << std::endl;
                res.status = 400;
                res.set_content("Username and password required", "text/plain");
                return;
            }
            
            if (m_user_manager.AuthenticateUser(username, password))
            {
                std::cout << "SUCCESS: Login successful: " << username << std::endl;
                res.status = 200;
                res.set_content("Login successful", "text/plain");
            }
            else
            {
                std::cout << "ERROR: Invalid credentials for: " << username << std::endl;
                res.status = 401;
                res.set_content("Invalid username or password", "text/plain");
            }
        });

        // Upload endpoint - FIXED for client upload
        m_server.Post("/upload", [this](const httplib::Request& req, httplib::Response& res)
        {
            std::string username = req.get_param_value("username");
            std::string filename = req.get_param_value("filename");
            
            std::cout << "\n=== UPLOAD REQUEST ===" << std::endl;
            std::cout << "Username: " << username << std::endl;
            std::cout << "Filename: " << filename << std::endl;
            
            if (username.empty() || filename.empty())
            {
                std::cout << "ERROR: Missing username or filename" << std::endl;
                res.status = 400;
                res.set_content("Username and filename required", "text/plain");
                return;
            }
            
            if (!m_user_manager.UserExists(username))
            {
                std::cout << "ERROR: User not found: " << username << std::endl;
                res.status = 401;
                res.set_content("User not found", "text/plain");
                return;
            }

            // Get file from request - handle both multipart and form data
            std::string file_content;
            
            // Check if it's multipart form data
            auto file_it = req.files.find("file");
            if (file_it != req.files.end())
            {
                file_content = file_it->second.content;
                std::cout << "Found multipart file, size: " << file_content.size() << " bytes" << std::endl;
            }
            else
            {
                // Try to get from body
                file_content = req.body;
                std::cout << "Using request body, size: " << file_content.size() << " bytes" << std::endl;
            }
            
            if (file_content.empty())
            {
                std::cout << "ERROR: No file content" << std::endl;
                res.status = 400;
                res.set_content("No file content provided", "text/plain");
                return;
            }
            
            std::string file_path = m_storage_path + "/" + username + "/" + filename;
            
            size_t file_size = file_content.length();
            size_t used = m_user_manager.GetStorageUsed(username);
            size_t limit = m_user_manager.GetStorageLimit();
            
            if (used + file_size > limit)
            {
                std::cout << "ERROR: Storage limit exceeded" << std::endl;
                res.status = 400;
                res.set_content("Storage limit exceeded", "text/plain");
                return;
            }

            try
            {
                std::vector<unsigned char> data(file_content.begin(), file_content.end());
                std::vector<unsigned char> encrypted = m_encryption.Encrypt(data, username);
                
                std::ofstream ofs(file_path, std::ios::binary);
                if (!ofs)
                {
                    std::cout << "ERROR: Cannot save file" << std::endl;
                    res.status = 500;
                    res.set_content("Failed to save file", "text/plain");
                    return;
                }
                
                ofs.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
                ofs.close();
                
                m_user_manager.UpdateStorageUsage(username, file_size);
                
                std::cout << "SUCCESS: File uploaded: " << username << "/" << filename 
                         << " (" << file_size << " bytes)" << std::endl;
                std::cout << "User storage: " << (used + file_size) << "/" << limit << " bytes" << std::endl;
                
                res.status = 200;
                res.set_content("File uploaded successfully", "text/plain");
            }
            catch (const std::exception& e)
            {
                std::cerr << "Upload error: " << e.what() << std::endl;
                res.status = 500;
                res.set_content("Upload failed: " + std::string(e.what()), "text/plain");
            }
        });

        // List files endpoint
        m_server.Get("/files", [this](const httplib::Request& req, httplib::Response& res)
        {
            std::string username = req.get_param_value("username");
            
            if (username.empty())
            {
                res.status = 400;
                res.set_content("Username required", "text/plain");
                return;
            }
            
            if (!m_user_manager.UserExists(username))
            {
                res.status = 401;
                res.set_content("User not found", "text/plain");
                return;
            }

            std::string user_path = m_storage_path + "/" + username;
            std::string file_list;
            
            if (fs::exists(user_path))
            {
                for (const auto& entry : fs::directory_iterator(user_path))
                {
                    if (entry.is_regular_file())
                    {
                        auto ftime = fs::last_write_time(entry);
                        auto time_t = std::chrono::system_clock::to_time_t(
                            std::chrono::clock_cast<std::chrono::system_clock>(ftime));
                        
                        char time_str[100];
                        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", 
                                     std::localtime(&time_t));
                        
                        file_list += entry.path().filename().string() + " [" + 
                                    std::to_string(fs::file_size(entry)) + " bytes] [" + 
                                    time_str + "]\n";
                    }
                }
            }
            
            if (file_list.empty())
            {
                file_list = "No files found\n";
            }
            
            res.status = 200;
            res.set_content(file_list, "text/plain");
        });

        // Download endpoint
        m_server.Get("/download", [this](const httplib::Request& req, httplib::Response& res)
        {
            std::string username = req.get_param_value("username");
            std::string filename = req.get_param_value("filename");
            
            if (username.empty() || filename.empty())
            {
                res.status = 400;
                res.set_content("Username and filename required", "text/plain");
                return;
            }
            
            if (!m_user_manager.UserExists(username))
            {
                res.status = 401;
                res.set_content("User not found", "text/plain");
                return;
            }

            std::string file_path = m_storage_path + "/" + username + "/" + filename;
            
            if (!fs::exists(file_path))
            {
                res.status = 404;
                res.set_content("File not found", "text/plain");
                return;
            }

            try
            {
                std::ifstream ifs(file_path, std::ios::binary);
                std::vector<unsigned char> encrypted(
                    (std::istreambuf_iterator<char>(ifs)),
                    std::istreambuf_iterator<char>()
                );
                
                std::vector<unsigned char> decrypted = m_encryption.Decrypt(encrypted, username);
                
                res.set_content(
                    reinterpret_cast<const char*>(decrypted.data()), 
                    decrypted.size(), 
                    "application/octet-stream"
                );
                
                std::cout << "File downloaded: " << username << "/" << filename << std::endl;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Download error: " << e.what() << std::endl;
                res.status = 500;
                res.set_content("Download failed: " + std::string(e.what()), "text/plain");
            }
        });

        // Storage info endpoint
        m_server.Get("/storage", [this](const httplib::Request& req, httplib::Response& res)
        {
            std::string username = req.get_param_value("username");
            
            if (username.empty())
            {
                res.status = 400;
                res.set_content("Username required", "text/plain");
                return;
            }
            
            if (!m_user_manager.UserExists(username))
            {
                res.status = 401;
                res.set_content("User not found", "text/plain");
                return;
            }

            size_t used = m_user_manager.GetStorageUsed(username);
            size_t limit = m_user_manager.GetStorageLimit();
            double percent = (static_cast<double>(used) / limit) * 100;
            
            std::string info = "Storage Information:\n";
            info += "  Used:  " + std::to_string(used) + " bytes\n";
            info += "  Limit: " + std::to_string(limit) + " bytes\n";
            info += "  Usage: " + std::to_string(percent) + "%\n";
            
            res.status = 200;
            res.set_content(info, "text/plain");
        });

        // Users list endpoint
        m_server.Get("/users", [this](const httplib::Request&, httplib::Response& res)
        {
            auto users = m_user_manager.GetAllUsers();
            std::string result = "Registered users (" + std::to_string(users.size()) + "/" + 
                                std::to_string(MAX_USERS) + "):\n";
            result += "========================\n";
            
            for (const auto& user : users)
            {
                size_t storage = m_user_manager.GetStorageUsed(user);
                result += "  " + user + " (" + std::to_string(storage) + " bytes)\n";
            }
            
            res.set_content(result, "text/plain");
        });
    }
};

int main()
{
    try
    {
        Server server;
        server.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}