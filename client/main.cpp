#include <cstddef>
#include <memory>
#include <locale>
#include <clocale>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <httplib.h>
#include "ftxui/dom/elements.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"

class Client
{
private:
    std::unique_ptr<httplib::Client> http_client;
    std::string current_user;
    std::string server_url;
    std::vector<std::string> file_list;
    std::string status_message;
    bool network_error;
    bool server_connected;
    std::chrono::steady_clock::time_point last_heartbeat;

public:
    Client(const std::string& server_address = "localhost", int port = 8080)
        : http_client(nullptr)
        , server_url(server_address)
        , status_message("Ready")
        , network_error(false)
        , server_connected(false)
    {
        if (server_address.find("http://") == 0 || server_address.find("https://") == 0)
        {
            server_url = server_address;
        }
        else
        {
            server_url = "http://" + server_address + ":" + std::to_string(port);
        }
        
        std::cout << "Connecting to server: " << server_url << std::endl;
        
        http_client = std::make_unique<httplib::Client>(server_url.c_str());
        http_client->set_connection_timeout(2);
        http_client->set_read_timeout(3);
        http_client->set_write_timeout(3);
        
        CheckConnection();
    }

    ~Client()
    {
        std::cout << "Client shutting down..." << std::endl;
    }

    void CheckConnection()
    {
        auto res = http_client->Get("/");
        if (res && res->status == 200)
        {
            server_connected = true;
            network_error = false;
            last_heartbeat = std::chrono::steady_clock::now();
        }
        else
        {
            server_connected = false;
            network_error = true;
        }
    }

    bool IsServerConnected()
    {
        // Check if it's been more than 5 seconds since last heartbeat
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_heartbeat).count();
        
        if (elapsed > 5)
        {
            // Try a quick connection test
            auto res = http_client->Get("/");
            if (res && res->status == 200)
            {
                server_connected = true;
                last_heartbeat = now;
            }
            else
            {
                server_connected = false;
                network_error = true;
            }
        }
        
        return server_connected;
    }

    bool Login(const std::string& username, const std::string& password)
    {
        if (!IsServerConnected())
        {
            status_message = "Server disconnected";
            return false;
        }
        
        httplib::Params params;
        params.emplace("username", username);
        params.emplace("password", password);
        
        auto response = http_client->Post("/login", params);
        
        if (response)
        {
            if (response->status == 200)
            {
                current_user = username;
                status_message = "Login successful";
                last_heartbeat = std::chrono::steady_clock::now();
                return true;
            }
            else
            {
                status_message = "Login failed: " + response->body;
            }
        }
        else
        {
            status_message = "Server not responding";
            network_error = true;
            server_connected = false;
        }
        return false;
    }

    bool Register(const std::string& username, const std::string& password)
    {
        if (!IsServerConnected())
        {
            status_message = "Server disconnected";
            return false;
        }
        
        httplib::Params params;
        params.emplace("username", username);
        params.emplace("password", password);
        
        auto response = http_client->Post("/register", params);
        
        if (response)
        {
            if (response->status == 200)
            {
                status_message = "Registration successful";
                last_heartbeat = std::chrono::steady_clock::now();
                return true;
            }
            else
            {
                status_message = "Registration failed: " + response->body;
            }
        }
        else
        {
            status_message = "Server not responding";
            network_error = true;
            server_connected = false;
        }
        return false;
    }

    bool UploadFile(const std::string& filepath)
    {
        if (!IsServerConnected())
        {
            status_message = "Server disconnected";
            return false;
        }
        
        std::ifstream file(filepath, std::ios::binary);
        if (!file)
        {
            status_message = "Cannot open file: " + filepath;
            return false;
        }
        
        size_t pos = filepath.find_last_of("/\\");
        std::string filename = (pos == std::string::npos) ? filepath : filepath.substr(pos + 1);
        
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        if (content.empty())
        {
            status_message = "File is empty";
            return false;
        }
        
        httplib::MultipartFormDataItems items = {
            {"username", current_user, "", ""},
            {"filename", filename, "", ""},
            {"file", content, filename, "application/octet-stream"}
        };
        
        auto response = http_client->Post("/upload", items);
        
        if (response)
        {
            if (response->status == 200)
            {
                status_message = "Upload successful: " + filename;
                last_heartbeat = std::chrono::steady_clock::now();
                return true;
            }
            else
            {
                status_message = "Upload failed: " + response->body;
            }
        }
        else
        {
            status_message = "Server not responding";
            network_error = true;
            server_connected = false;
        }
        return false;
    }

    bool DownloadFile(const std::string& filename)
    {
        if (!IsServerConnected())
        {
            status_message = "Server disconnected";
            return false;
        }
        
        std::string url = "/download?username=" + current_user + "&filename=" + filename;
        auto response = http_client->Get(url.c_str());
        
        if (response)
        {
            if (response->status == 200)
            {
                std::ofstream file(filename, std::ios::binary);
                if (!file)
                {
                    status_message = "Cannot save file";
                    return false;
                }
                
                file.write(response->body.c_str(), response->body.size());
                file.close();
                
                status_message = "Downloaded: " + filename;
                last_heartbeat = std::chrono::steady_clock::now();
                return true;
            }
            else
            {
                status_message = "Download failed: " + response->body;
            }
        }
        else
        {
            status_message = "Server not responding";
            network_error = true;
            server_connected = false;
        }
        return false;
    }

    void Run()
    {
        using namespace ftxui;
        
        auto screen = ScreenInteractive::TerminalOutput();
        
        std::string username, password;
        bool login_success = false;
        std::string login_status = "";
        
        CheckConnection();
        
        auto username_input = Input(&username, "Username");
        auto password_input = Input(&password, "Password");
        
        password_input |= CatchEvent([&](Event event)
        {
            if (event == Event::Escape)
            {
                screen.ExitLoopClosure()();
                return true;
            }
            return false;
        });
        
        auto login_button = Button("Login", [&]
        {
            if (!IsServerConnected())
            {
                login_status = "Server disconnected";
                return;
            }
            
            if (Login(username, password))
            {
                login_success = true;
                screen.ExitLoopClosure()();
            }
            else
            {
                login_status = status_message;
            }
        });
        
        auto register_button = Button("Register", [&]
        {
            if (!IsServerConnected())
            {
                login_status = "Server disconnected";
                return;
            }
            
            if (Register(username, password))
            {
                login_status = "Registration successful! Please login.";
            }
            else
            {
                login_status = status_message;
            }
        });
        
        auto quit_button = Button("Quit", [&] { screen.ExitLoopClosure()(); });
        
        auto reconnect_button = Button("Reconnect", [&]
        {
            http_client = std::make_unique<httplib::Client>(server_url.c_str());
            http_client->set_connection_timeout(2);
            http_client->set_read_timeout(3);
            http_client->set_write_timeout(3);
            
            CheckConnection();
            
            if (server_connected)
            {
                login_status = "Connected to server";
            }
            else
            {
                login_status = "Cannot connect to server";
            }
        });
        
        auto login_components = Container::Vertical(
        {
            username_input,
            password_input,
            Container::Horizontal({login_button, register_button, quit_button}),
            reconnect_button
        });
        
        auto login_renderer = Renderer(login_components, [&]
        {
            std::string conn_status = server_connected ? "Connected" : "Disconnected";
            Color conn_color = server_connected ? Color::Green : Color::Red;
            
            return vbox({
                text("E2Eye Login") | bold | center,
                separator(),
                hbox(text("Server: "), text(server_url) | dim, text(" [") , text(conn_status) | color(conn_color) | bold, text("]")),
                separator(),
                text("Username:"),
                username_input->Render(),
                text("Password:"),
                password_input->Render(),
                separator(),
                hbox(login_button->Render(), register_button->Render(), quit_button->Render()),
                text("") | size(HEIGHT, EQUAL, 1),
                text(login_status) | color(login_status.find("success") != std::string::npos ? Color::Green : 
                                           login_status.find("fail") != std::string::npos ? Color::Red : Color::GrayDark),
                text("") | size(HEIGHT, EQUAL, 1),
                reconnect_button->Render(),
                text("Press ESC to quit") | dim
            }) | border;
        });
        
        screen.Loop(login_renderer);
        
        if (login_success)
        {
            ShowMainMenu(screen);
        }
    }

    void ShowMainMenu(ftxui::ScreenInteractive& screen)
    {
        using namespace ftxui;
    
        int selected_tab = 0;
        int previous_tab = 0;
        size_t selected_file = 0;
        std::vector<std::string> tab_titles = {"Files", "Upload", "Storage"};
    
        auto tab_menu = Menu(&tab_titles, &selected_tab);
    
        // Create a component for file list with event handling
        auto file_list_container = Container::Vertical({});
        
        // Files tab content as a component
        auto files_tab = Renderer([&]
        {
            if (!IsServerConnected())
            {
                return vbox({
                    text("Connection Lost") | bold | center | color(Color::Red),
                    separator(),
                    text("Server is not responding") | center,
                    text("") | size(HEIGHT, EQUAL, 1),
                    text("Press Logout to return to login screen") | dim | center,
                    hbox({
                        Button("Logout", [&]
                        {
                            current_user = "";
                            screen.ExitLoopClosure()();
                        })->Render()
                    }) | center
                }) | border | size(HEIGHT, EQUAL, 10);
            }
            
            if (selected_tab == 0)
            {
                RefreshFileList();
            }
            
            static std::string download_status = "";
            
            Elements file_elements;
            size_t index = 0;
            for (const auto& file : file_list)
            {
                if (index == selected_file)
                {
                    file_elements.push_back(text("→ " + file) | bold | color(Color::Blue));
                }
                else
                {
                    file_elements.push_back(text("  " + file));
                }
                index++;
            }
            if (file_elements.empty())
            {
                file_elements.push_back(text("  No files found"));
            }
            
            return vbox({
                text("Your Files - " + current_user) | bold | center,
                separator(),
                vbox(file_elements) | frame | size(HEIGHT, LESS_THAN, 15),
                separator(),
                hbox({
                    Button("Download Selected", [&]
                    {
                        if (!IsServerConnected())
                        {
                            download_status = "Server disconnected";
                            return;
                        }
                        
                        if (!file_list.empty() && selected_file < file_list.size())
                        {
                            std::string filename = file_list[selected_file];
                            size_t space = filename.find(' ');
                            if (space != std::string::npos)
                            {
                                filename = filename.substr(0, space);
                            }
                            
                            if (DownloadFile(filename))
                            {
                                download_status = "Downloaded: " + filename;
                            }
                            else
                            {
                                download_status = "Download failed";
                            }
                        }
                        else
                        {
                            download_status = "No file selected";
                        }
                    })->Render(),
                    Button("Refresh", [&]
                    {
                        if (IsServerConnected())
                        {
                            RefreshFileList();
                            download_status = "List refreshed";
                        }
                        else
                        {
                            download_status = "Server disconnected";
                        }
                    })->Render(),
                    Button("Logout", [&]
                    {
                        current_user = "";
                        screen.ExitLoopClosure()();
                    })->Render()
                }),
                text("") | size(HEIGHT, EQUAL, 1),
                text(download_status) | color(download_status.find("Downloaded") != std::string::npos ? Color::Green : Color::GrayDark),
                text("") | size(HEIGHT, EQUAL, 1),
                text(status_message) | color(Color::Blue)
            }) | border;
        });
    
        // Upload tab
        auto upload_tab = Renderer([&]
        {
            if (!IsServerConnected())
            {
                return vbox({
                    text("Connection Lost") | bold | center | color(Color::Red),
                    separator(),
                    text("Server is not responding") | center,
                    text("") | size(HEIGHT, EQUAL, 1),
                    text("Press Back to return to Files tab") | dim | center,
                    hbox({
                        Button("Back", [&]
                        {
                            selected_tab = 0;
                        })->Render()
                    }) | center
                }) | border | size(HEIGHT, EQUAL, 10);
            }
            
            static std::string filepath = "";
            static std::string upload_status = "";
            
            return vbox({
                text("Upload File") | bold | center,
                separator(),
                text("Current user: " + current_user) | color(Color::Blue),
                text("Server: " + server_url) | dim,
                separator(),
                text("File path:"),
                Input(&filepath, "Enter file path")->Render(),
                hbox({
                    Button("Upload", [&]
                    {
                        if (!IsServerConnected())
                        {
                            upload_status = "Server disconnected";
                            return;
                        }
                        
                        if (filepath.empty())
                        {
                            upload_status = "Please enter a file path";
                        }
                        else
                        {
                            if (UploadFile(filepath))
                            {
                                upload_status = "Upload successful!";
                                filepath = "";
                                RefreshFileList();
                            }
                            else
                            {
                                upload_status = "Upload failed";
                            }
                        }
                    })->Render(),
                    Button("Back", [&]
                    {
                        selected_tab = 0;
                    })->Render(),
                    Button("Logout", [&]
                    {
                        current_user = "";
                        screen.ExitLoopClosure()();
                    })->Render()
                }),
                text("") | size(HEIGHT, EQUAL, 1),
                text(upload_status) | color(upload_status.find("success") != std::string::npos ? Color::Green : 
                                           upload_status.find("fail") != std::string::npos ? Color::Red : Color::GrayDark),
                separator(),
                text("Examples:") | dim,
                text("  /home/user/docs/notes.txt") | dim,
                text("  ./photo.jpg") | dim,
                text("  C:\\Users\\name\\file.pdf") | dim
            }) | border;
        });
    
        // Storage tab
        auto storage_tab = Renderer([&]
        {
            if (!IsServerConnected())
            {
                return vbox({
                    text("Connection Lost") | bold | center | color(Color::Red),
                    separator(),
                    text("Server is not responding") | center,
                    text("") | size(HEIGHT, EQUAL, 1),
                    text("Press Back to return to Files tab") | dim | center,
                    hbox({
                        Button("Back", [&]
                        {
                            selected_tab = 0;
                        })->Render()
                    }) | center
                }) | border | size(HEIGHT, EQUAL, 10);
            }
            
            std::string storage_info = GetStorageInfo();
            return vbox({
                text("Storage Information") | bold | center,
                separator(),
                text(storage_info),
                text(""),
                hbox({
                    Button("Refresh", [&]
                    {
                        // Just refresh
                    })->Render(),
                    Button("Logout", [&]
                    {
                        current_user = "";
                        screen.ExitLoopClosure()();
                    })->Render()
                })
            }) | border;
        });
    
        auto tab_container = Container::Tab(
            {
                files_tab,
                upload_tab,
                storage_tab
            },
            &selected_tab
        );
    
        auto main_layout = Container::Vertical({
            tab_menu,
            tab_container
        });
    
        auto main_renderer = Renderer(main_layout, [&]
        {
            std::string conn_status = server_connected ? "● Connected" : "○ Disconnected";
            Color conn_color = server_connected ? Color::Green : Color::Red;
            
            if (selected_tab == 0 && previous_tab != selected_tab)
            {
                RefreshFileList();
            }
            previous_tab = selected_tab;
    
            return vbox({
                hbox(
                    text("E2Eye - User: " + current_user) | bold,
                    filler(),
                    text(conn_status) | color(conn_color) | dim
                ) | center,
                separator(),
                tab_menu->Render() | hcenter,
                separator(),
                tab_container->Render() | flex,
            }) | border;
        });
    
        // Add global key handling for file navigation
        auto main_component = main_layout | CatchEvent([&](Event event)
        {
            if (selected_tab == 0) // Only in Files tab
            {
                if (event == Event::ArrowUp && selected_file > 0)
                {
                    selected_file--;
                    return true;
                }
                if (event == Event::ArrowDown && selected_file < file_list.size() - 1)
                {
                    selected_file++;
                    return true;
                }
            }
            return false;
        });
    
        screen.Loop(main_component);
}

    void RefreshFileList()
    {
        if (!IsServerConnected()) return;
        
        std::string url = "/files?username=" + current_user;
        auto response = http_client->Get(url.c_str());
        
        file_list.clear();
        if (response && response->status == 200)
        {
            std::string content = response->body;
            size_t pos = 0;
            while ((pos = content.find('\n')) != std::string::npos)
            {
                std::string file = content.substr(0, pos);
                if (!file.empty() && file != "No files found")
                {
                    file_list.push_back(file);
                }
                content.erase(0, pos + 1);
            }
            status_message = "Found " + std::to_string(file_list.size()) + " files";
            last_heartbeat = std::chrono::steady_clock::now();
        }
        else
        {
            status_message = "Failed to load files";
            server_connected = false;
            network_error = true;
        }
    }

    std::string GetStorageInfo()
    {
        if (!IsServerConnected()) return "Cannot connect to server";
        
        std::string url = "/storage?username=" + current_user;
        auto response = http_client->Get(url.c_str());
        
        if (response && response->status == 200)
        {
            last_heartbeat = std::chrono::steady_clock::now();
            return response->body;
        }
        
        server_connected = false;
        network_error = true;
        return "Unable to retrieve storage info";
    }
};

int main(int argc, char* argv[])
{
    try
    {
        std::setlocale(LC_ALL, "en_US.UTF-8");
        std::locale::global(std::locale("en_US.UTF-8"));
    }
    catch (const std::exception& e)
    {
        std::cerr << "Warning: Locale setup failed: " << e.what() << std::endl;
    }
    
    std::string server_address = "localhost";
    int port = 8080;
    
    if (argc > 1)
    {
        server_address = argv[1];
    }
    if (argc > 2)
    {
        port = std::stoi(argv[2]);
    }
    
    std::cout << "E2Eye Client" << std::endl;
    std::cout << "============" << std::endl;
    std::cout << "Server address: " << server_address << std::endl;
    if (argc > 2) std::cout << "Port: " << port << std::endl;
    std::cout << "Press ESC to quit at any time" << std::endl;
    std::cout << std::endl;
    
    try
    {
        Client client(server_address, port);
        client.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Client terminated normally" << std::endl;
    return 0;
}