#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/screen.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/component/component_options.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <httplib.h>

class Client
{
private:
    httplib::Client http_client;
    std::string current_user;
    std::string server_url;
    std::vector<std::string> file_list;
    std::string status_message;

public:
    Client(const std::string& server_address = "localhost", int port = 8080)
        : server_url("http://" + server_address + ":" + std::to_string(port))
        , http_client(server_url)
        , status_message("Ready")
    {
    }

    void Run()
    {
        using namespace ftxui;
        
        auto screen = ScreenInteractive::TerminalOutput();
        
        // Login screen
        std::string username, password;
        bool login_success = false;
        std::string login_status = "";
        
        // Create input components
        auto username_input = Input(&username, "Username");
        auto password_input = Input(&password, "Password");
        
        // Make password field
        password_input |= CatchEvent([&](Event event) 
        {
            return false;  // Let FTXUI handle normally
        });
        
        // Login button
        auto login_button = Button("Login", [&] 
        {
            if (Login(username, password)) 
            {
                login_success = true;
                screen.ExitLoopClosure()();
            } 
            else
            {
                login_status = "Login failed";
            }
        });
        
        // Register button
        auto register_button = Button("Register", [&] 
        {
            if (Register(username, password)) 
            {
                login_status = "Registration successful! Please login.";
            } 
            else 
            {
                login_status = "Registration failed";
            }
        });
        
        // Login screen layout
        auto login_components = Container::Vertical(
        {
            username_input,
            password_input,
            Container::Horizontal({login_button, register_button})
        });
        
        auto login_renderer = Renderer(login_components, [&] 
        {
            return vbox({
                text("E2Eye Login") | bold | center,
                separator(),
                text("Username:"),
                username_input->Render(),
                text("Password:"),
                password_input->Render(),
                separator(),
                hbox(login_button->Render(), register_button->Render()),
                text(login_status) | color(Color::GrayDark)
            }) | border;
        });
        
        screen.Loop(login_renderer);
        
        if (login_success) 
        {
            ShowMainMenu(screen);
        }
    }

private:
    bool Login(const std::string& username, const std::string& password)
    {
        httplib::Params params;
        params.emplace("username", username);
        params.emplace("password", password);
        
        auto response = http_client.Post("/login", params);
        
        if (response && response->status == 200) 
        {
            current_user = username;
            return true;
        }
        return false;
    }

    bool Register(const std::string& username, const std::string& password)
    {
        httplib::Params params;
        params.emplace("username", username);
        params.emplace("password", password);
        
        auto response = http_client.Post("/register", params);
        
        return response && response->status == 200;
    }

    void ShowMainMenu(ftxui::ScreenInteractive& screen)
    {
        using namespace ftxui;
        
        int selected_tab = 0;
        std::vector<std::string> tab_titles = {"Files", "Upload", "Storage"};
        
        // Tab menu
        auto tab_menu = Menu(&tab_titles, &selected_tab);
        
        // Files tab content
        auto files_tab = Renderer([&] 
        {
            RefreshFileList();
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
                text("Your Files:") | bold,
                separator(),
                vbox(file_elements) | frame | size(HEIGHT, LESS_THAN, 20),
                text("") | size(HEIGHT, EQUAL, 1),
                text(status_message) | color(Color::Blue)
            }) | border;
        });
        
        // Upload tab content
        auto upload_tab = Renderer([&] 
        {
            return vbox({
                text("Upload File:") | bold,
                separator(),
                text("Use command line to upload files:"),
                text(""),
                text("  curl -X POST http://" + server_url + "/upload \\"),
                text("    -F \"username=" + current_user + "\" \\"),
                text("    -F \"filename=yourfile.jpg\" \\"),
                text("    -F \"file=@/path/to/yourfile.jpg\""),
                text(""),
                text("Or use the test script:")
            }) | border;
        });
        
        // Storage tab content
        auto storage_tab = Renderer([&] 
        {
            std::string storage_info = GetStorageInfo();
            return vbox({
                text("Storage Information:") | bold,
                separator(),
                text(storage_info),
                text(""),
                text("Refresh to update")
            }) | border;
        });
        
        // Container for tabs
        auto tab_container = Container::Tab
        (
            {
                files_tab,
                upload_tab,
                storage_tab
            },
            &selected_tab
        );
        
        // Main layout
        auto main_layout = Container::Vertical
        ({
            tab_menu,
            tab_container
        });
        
        auto main_renderer = Renderer(main_layout, [&] 
        {
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
        std::string url = "/files?username=" + current_user;
        auto response = http_client.Get(url.c_str());
        
        file_list.clear();
        if (response && response->status == 200) 
        {
            std::string content = response->body;
            size_t pos = 0;
            while ((pos = content.find('\n')) != std::string::npos) 
            {
                std::string file = content.substr(0, pos);
                if (!file.empty()) 
                {
                    file_list.push_back(file);
                }
                content.erase(0, pos + 1);
            }
            status_message = "Found " + std::to_string(file_list.size()) + " files";
        }
        else
        {
            status_message = "Failed to load files";
        }
    }

    std::string GetStorageInfo()
    {
        std::string url = "/storage?username=" + current_user;
        auto response = http_client.Get(url.c_str());
        
        if (response && response->status == 200) 
        {
            return response->body;
        }
        return "Unable to retrieve storage info";
    }
};

int main(int argc, char* argv[])
{
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
    
    return 0;
}