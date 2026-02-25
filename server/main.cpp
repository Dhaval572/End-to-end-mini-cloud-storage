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
        std::cout << "Server listening on http://localhost:" << PORT << std::endl;
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

        // Register endpoint
        // Register endpoint - FIXED
        // Register endpoint with EXTREME debugging
        m_server.Post("/register", [this](const httplib::Request& req, httplib::Response& res)
        {
            std::cout << "\n========== SERVER DEBUG ==========" << std::endl;
            std::cout << "=== REGISTER ENDPOINT HIT ===" << std::endl;
            std::cout << "Request method: " << req.method << std::endl;
            std::cout << "Request path: " << req.path << std::endl;
            std::cout << "Request body: '" << req.body << "'" << std::endl;
            std::cout << "Body length: " << req.body.length() << std::endl;
            
            // Print all headers
            std::cout << "Headers:" << std::endl;
            for (const auto& header : req.headers) {
                std::cout << "  " << header.first << ": " << header.second << std::endl;
            }
            
            // Print all params
            std::cout << "Params from get_param_value:" << std::endl;
            for (const auto& param : req.params) {
                std::cout << "  " << param.first << " = '" << param.second << "'" << std::endl;
            }
            
            std::string username = req.get_param_value("username");
            std::string password = req.get_param_value("password");
            
            std::cout << "Extracted username via get_param_value: '" << username << "'" << std::endl;
            std::cout << "Extracted password via get_param_value: '" << password << "' (length: " << password.length() << ")" << std::endl;
            
            // If get_param_value failed, try manual parsing
            if (username.empty() && !req.body.empty()) {
                std::cout << "Attempting manual parse from body..." << std::endl;
                
                std::string body = req.body;
                size_t pos = 0;
                while (pos < body.length()) {
                    size_t amp_pos = body.find('&', pos);
                    std::string pair = body.substr(pos, amp_pos - pos);
                    std::cout << "  Found pair: '" << pair << "'" << std::endl;
                    
                    size_t eq_pos = pair.find('=');
                    if (eq_pos != std::string::npos) {
                        std::string key = pair.substr(0, eq_pos);
                        std::string value = pair.substr(eq_pos + 1);
                        std::cout << "    Key: '" << key << "', Value: '" << value << "'" << std::endl;
                        
                        if (key == "username") username = value;
                        if (key == "password") password = value;
                    }
                    
                    if (amp_pos == std::string::npos) break;
                    pos = amp_pos + 1;
                }
                
                std::cout << "Manual parse result:" << std::endl;
                std::cout << "  Username: '" << username << "'" << std::endl;
                std::cout << "  Password: '" << password << "'" << std::endl;
            }
            
            if (username.empty() || password.empty())
            {
                std::cout << "ERROR: Username or password is empty!" << std::endl;
                res.status = 400;
                res.set_content("Username and password required", "text/plain");
                return;
            }
            
            // Check if user already exists
            std::cout << "Checking if user exists: " << username << std::endl;
            if (m_user_manager.UserExists(username)) {
                std::cout << "User already exists!" << std::endl;
                res.status = 400;
                res.set_content("Username already exists", "text/plain");
                return;
            }
            
            std::cout << "Calling UserManager::RegisterUser..." << std::endl;
            bool register_result = m_user_manager.RegisterUser(username, password);
            std::cout << "RegisterUser returned: " << (register_result ? "true" : "false") << std::endl;
            
            if (register_result) 
            {
                // Create user storage directory
                try {
                    std::string user_dir = m_storage_path + "/" + username;
                    std::cout << "Creating directory: " << user_dir << std::endl;
                    fs::create_directories(user_dir);
                    std::cout << "Directory created successfully" << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Exception creating directory: " << e.what() << std::endl;
                }
                
                std::cout << "Registration successful!" << std::endl;
                res.set_content("Registration successful", "text/plain");
            } 
            else 
            {
                std::cout << "Registration failed!" << std::endl;
                res.status = 400;
                res.set_content("Registration failed", "text/plain");
            }
            
            std::cout << "========== END DEBUG ==========\n" << std::endl;
        });

        // Login endpoint - FIXED
        m_server.Post("/login", [this](const httplib::Request& req, httplib::Response& res)
        {
            std::cout << "\n=== Login Request ===" << std::endl;
            std::cout << "Body: '" << req.body << "'" << std::endl;

            std::string username, password;

            // Get from params (should work)
            username = req.get_param_value("username");
            password = req.get_param_value("password");

            std::cout << "From params - Username: '" << username << "', Password length: " << password.length() << std::endl;

            // If params empty, try manual parsing
            if (username.empty() && !req.body.empty()) 
            {
                std::cout << "Params empty, parsing body manually..." << std::endl;

                std::string body = req.body;
                size_t pos = 0;
                while (pos < body.length()) 
                {
                    size_t amp_pos = body.find('&', pos);
                    std::string pair = body.substr(pos, amp_pos - pos);

                    size_t eq_pos = pair.find('=');
                    if (eq_pos != std::string::npos) 
                    {
                        std::string key = pair.substr(0, eq_pos);
                        std::string value = pair.substr(eq_pos + 1);

                        if (key == "username") username = value;
                        if (key == "password") password = value;
                    }

                    if (amp_pos == std::string::npos) break;
                    pos = amp_pos + 1;
                }

                std::cout << "Manual parse - Username: '" 
                          << username 
                          << "', Password: '" 
                          << password 
                          << "'" 
                          << std::endl;
            }

            if (username.empty() || password.empty())
            {
                std::cout << "ERROR: Empty username or password" << std::endl;
                res.status = 400;
                res.set_content("Username and password required", "text/plain");
                return;
            }

            std::cout << "Attempting login for: " << username << std::endl;

            if (m_user_manager.AuthenticateUser(username, password)) 
            {
                std::cout << "Login successful: " << username << std::endl;
                res.set_content("Login successful", "text/plain");
            } 
            else 
            {
                std::cout << "Login failed: Invalid credentials" << std::endl;
                res.status = 401;
                res.set_content("Invalid credentials", "text/plain");
            }
        });

        // Upload endpoint
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

            // Encrypt and save
            std::vector<unsigned char> data(file_content.begin(), file_content.end());
            std::vector<unsigned char> encrypted_data = m_encryption.Encrypt(data, username);
            
            std::ofstream ofs(file_path, std::ios::binary);
            if (ofs.is_open()) 
            {
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

            std::ifstream ifs(file_path, std::ios::binary);
            std::vector<unsigned char> encrypted_data
            (
                (std::istreambuf_iterator<char>(ifs)),
                std::istreambuf_iterator<char>()
            );
            
            std::vector<unsigned char> decrypted_data = m_encryption.Decrypt(encrypted_data, username);
            
            res.set_content
            (
                reinterpret_cast<const char*>(decrypted_data.data()), 
                decrypted_data.size(), 
                "application/octet-stream"
            );
            
            std::cout << "File downloaded: " << username << "/" << filename << std::endl;
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