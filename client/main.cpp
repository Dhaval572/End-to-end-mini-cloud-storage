#include <memory>
#include <locale>
#include <clocale>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <chrono>
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
    bool should_exit = false;
    bool network_error = false;

public:
    Client(const std::string& server_address = "localhost", int port = 8080)
        : http_client(nullptr)
        , server_url(server_address)
        , status_message("Ready")
    {
        // Check if it's already a full URL (for ngrok)
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
        http_client->set_connection_timeout(3);
        http_client->set_read_timeout(5);
        http_client->set_write_timeout(5);
        
        // Test connection
        auto res = http_client->Get("/");
        if (res)
        {
            std::cout << "Server connected successfully" << std::endl;
        }
        else
        {
            std::cout << "Warning: Cannot connect to server. Make sure it's running." << std::endl;
            network_error = true;
        }
    }

    ~Client()
    {
        std::cout << "Client shutting down..." << std::endl;
    }

    bool Login(const std::string& username, const std::string& password)
    {
        if (network_error)
        {
            status_message = "Cannot connect to server";
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
        }
        return false;
    }

    bool Register(const std::string& username, const std::string& password)
    {
        if (network_error)
        {
            status_message = "Cannot connect to server";
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
        }
        return false;
    }

    bool UploadFile(const std::string& filepath)
    {
        if (network_error)
        {
            status_message = "Cannot connect to server";
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
        }
        return false;
    }

    bool DownloadFile(const std::string& filename)
    {
        if (network_error)
        {
            status_message = "Cannot connect to server";
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
        
        if (network_error)
        {
            login_status = "Warning: Cannot connect to server";
        }
        
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
            login_status = "Connecting...";
            screen.PostEvent(Event::Custom);
            
            // Run login in background to prevent freezing
            std::thread([&]()
            {
                if (Login(username, password))
                {
                    login_success = true;
                    screen.ExitLoopClosure()();
                }
                else
                {
                    screen.PostEvent(Event::Custom);
                }
            }).detach();
        });
        
        auto register_button = Button("Register", [&]
        {
            login_status = "Connecting...";
            screen.PostEvent(Event::Custom);
            
            std::thread([&]()
            {
                if (Register(username, password))
                {
                    login_status = "Registration successful! Please login.";
                }
                else
                {
                    login_status = status_message;
                }
                screen.PostEvent(Event::Custom);
            }).detach();
        });
        
        auto quit_button = Button("Quit", [&] { screen.ExitLoopClosure()(); });
        
        auto reconnect_button = Button("Reconnect", [&]
        {
            login_status = "Reconnecting...";
            screen.PostEvent(Event::Custom);
            
            std::thread([&]()
            {
                http_client = std::make_unique<httplib::Client>(server_url.c_str());
                http_client->set_connection_timeout(3);
                http_client->set_read_timeout(5);
                http_client->set_write_timeout(5);
                
                auto res = http_client->Get("/");
                if (res)
                {
                    network_error = false;
                    login_status = "Connected to server";
                }
                else
                {
                    network_error = true;
                    login_status = "Still cannot connect";
                }
                screen.PostEvent(Event::Custom);
            }).detach();
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
            return vbox({
                text("E2Eye Login") | bold | center,
                separator(),
                text("Server: " + server_url) | color(network_error ? Color::Red : Color::Green) | dim,
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
        std::vector<std::string> tab_titles = {"Files", "Upload", "Storage"};

        auto tab_menu = Menu(&tab_titles, &selected_tab);

        // Files tab
        auto files_tab = Renderer([&]
        {
            if (selected_tab == 0)
            {
                RefreshFileList();
            }
            
            static int selected = 0;
            static std::string download_status = "";
            
            Elements file_elements;
            for (const auto& file : file_list)
            {
                file_elements.push_back(text("  " + file));
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
                        download_status = "Downloading...";
                        screen.PostEvent(Event::Custom);
                        
                        std::thread([&]()
                        {
                            if (!file_list.empty() && selected < file_list.size())
                            {
                                std::string filename = file_list[selected];
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
                            screen.PostEvent(Event::Custom);
                        }).detach();
                    })->Render(),
                    Button("Refresh", [&]
                    {
                        RefreshFileList();
                        download_status = "List refreshed";
                        screen.PostEvent(Event::Custom);
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
                    Button("Upload File", [&]
                    {
                        if (filepath.empty())
                        {
                            upload_status = "Please enter a file path";
                        }
                        else
                        {
                            upload_status = "Uploading...";
                            screen.PostEvent(Event::Custom);
                            
                            std::thread([&]()
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
                                screen.PostEvent(Event::Custom);
                            }).detach();
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
            std::string storage_info = GetStorageInfo();
            return vbox({
                text("Storage Information") | bold | center,
                separator(),
                text(storage_info),
                text(""),
                hbox({
                    Button("Refresh", [&]
                    {
                        screen.PostEvent(Event::Custom);
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
            if (selected_tab == 0 && previous_tab != selected_tab)
            {
                RefreshFileList();
            }
            previous_tab = selected_tab;

            return vbox({
                text("E2Eye - User: " + current_user) | bold | center,
                separator(),
                tab_menu->Render() | hcenter,
                separator(),
                tab_container->Render() | flex,
            }) | border;
        });

        screen.Loop(main_renderer);
    }

    void RefreshFileList()
    {
        if (network_error) return;
        
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
            network_error = false;
        }
        else
        {
            status_message = "Failed to load files";
            network_error = true;
        }
    }

    std::string GetStorageInfo()
    {
        if (network_error) return "Cannot connect to server";
        
        std::string url = "/storage?username=" + current_user;
        auto response = http_client->Get(url.c_str());
        
        if (response && response->status == 200)
        {
            network_error = false;
            return response->body;
        }
        
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