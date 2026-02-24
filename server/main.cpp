#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
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
        std::cout << "Local URL: http://localhost:" << PORT << std::endl;
        std::cout << "Storage path: " << fs::absolute(m_storage_path) << std::endl;
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
                "  POST /register - Register new user\n"
                "  POST /login - User login\n"
                "  POST /upload - Upload file\n"
                "  GET /files - List files\n"
                "  GET /download - Download file\n"
                "  GET /storage - Get storage info\n";
            
            res.set_content(message, "text/plain");
        });

        // Register
        m_server.Post("/register", [this](const httplib::Request& req, httplib::Response& res)
        {
            std::string username = req.get_param_value("username");
            std::string password = req.get_param_value("password");
            
            if (username.empty() || password.empty())
            {
                res.status = 400;
                res.set_content("Username and password required", "text/plain");
                return;
            }
            
            if (m_user_manager.RegisterUser(username, password)) 
            {
                fs::create_directories(m_storage_path + "/" + username);
                std::cout << "New user registered: " << username << std::endl;
                res.status = 200;
                res.set_content("User registered successfully", "text/plain");
            } 
            else 
            {
                res.status = 400;
                res.set_content("Registration failed", "text/plain");
            }
        });

        // Login
        m_server.Post("/login", [this](const httplib::Request& req, httplib::Response& res)
        {
            std::string username = req.get_param_value("username");
            std::string password = req.get_param_value("password");
            
            if (username.empty() || password.empty())
            {
                res.status = 400;
                res.set_content("Username and password required", "text/plain");
                return;
            }
            
            if (m_user_manager.AuthenticateUser(username, password)) 
            {
                std::cout << "User logged in: " << username << std::endl;
                res.status = 200;
                res.set_content("Login successful", "text/plain");
            } 
            else 
            {
                res.status = 401;
                res.set_content("Invalid credentials", "text/plain");
            }
        });

        // Upload
        m_server.Post("/upload", [this](const httplib::Request& req, httplib::Response& res)
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

            auto file_it = req.files.find("file");
            if (file_it == req.files.end())
            {
                res.status = 400;
                res.set_content("No file provided", "text/plain");
                return;
            }
            
            const auto& file = file_it->second;
            std::string file_content = file.content;
            std::string file_path = m_storage_path + "/" + username + "/" + filename;
            
            size_t file_size = file_content.length();
            if (m_user_manager.GetStorageUsed(username) + file_size > m_user_manager.GetStorageLimit()) 
            {
                res.status = 400;
                res.set_content("Storage limit exceeded", "text/plain");
                return;
            }

            std::ofstream ofs(file_path, std::ios::binary);
            if (ofs.is_open()) 
            {
                std::vector<unsigned char> encrypted_data = m_encryption.Encrypt(
                    std::vector<unsigned char>(file_content.begin(), file_content.end()), 
                    username
                );
                ofs.write(reinterpret_cast<const char*>(encrypted_data.data()), encrypted_data.size());
                ofs.close();
                
                m_user_manager.UpdateStorageUsage(username, file_size);
                std::cout << "File uploaded: " << username << "/" << filename << std::endl;
                res.status = 200;
                res.set_content("File uploaded successfully", "text/plain");
            } 
            else 
            {
                res.status = 500;
                res.set_content("Failed to save file", "text/plain");
            }
        });

        // List files
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

            std::string user_storage = m_storage_path + "/" + username;
            std::string file_list;
            
            if (fs::exists(user_storage))
            {
                for (const auto& entry : fs::directory_iterator(user_storage)) 
                {
                    if (entry.is_regular_file()) 
                    {
                        file_list += entry.path().filename().string() + "\n";
                    }
                }
            }
            
            res.status = 200;
            res.set_content(file_list, "text/plain");
        });

        // Download
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

            std::ifstream ifs(file_path, std::ios::binary);
            std::vector<unsigned char> encrypted_data(
                (std::istreambuf_iterator<char>(ifs)),
                std::istreambuf_iterator<char>()
            );
            
            std::vector<unsigned char> decrypted_data = m_encryption.Decrypt(encrypted_data, username);
            
            res.set_content(
                reinterpret_cast<const char*>(decrypted_data.data()), 
                decrypted_data.size(), 
                "application/octet-stream"
            );
            
            std::cout << "File downloaded: " << username << "/" << filename << std::endl;
        });

        // Storage info
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
            
            std::string info = "Used: " + std::to_string(used) + " bytes\n";
            info += "Limit: " + std::to_string(limit) + " bytes\n";
            
            res.status = 200;
            res.set_content(info, "text/plain");
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
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}