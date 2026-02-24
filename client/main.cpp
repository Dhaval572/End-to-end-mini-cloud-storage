#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/screen.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <httplib.h>

class Client
{
private:
    httplib::Client http_client;
    std::string current_user;
    std::string server_url;
    std::vector<std::string> file_list;

public:
    Client(const std::string& server_address = "localhost", int port = 8080)
        : server_url("http://" + server_address + ":" + std::to_string(port))
        , http_client(server_url)
    {
    }

    void Run()
    {
        using namespace ftxui;
        
        auto screen = ScreenInteractive::TerminalOutput();
        
        // Login screen
        std::string username, password;
        bool login_success = false;
        
        auto login_component = Container::Vertical({
            Input(&username, "Username"),
            Input(&password, "Password", true),
            Button("Login", [this, &username, &password, &login_success, &screen]{
                if (Login(username, password)) {
                    login_success = true;
                    screen.Exit();
                }
            }),
            Button("Register", [this, &username, &password, &screen]{
                if (Register(username, password)) {
                    std::cout << "Registration successful!" << std::endl;
                }
            })
        });
        
        screen.Loop(login_component);
        
        if (login_success) {
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
        
        if (response && response->status == 200) {
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
        std::vector<std::string> tabs = {"Files", "Upload", "Storage"};
        
        auto tab_selection = Menu(&tabs, &selected_tab);
        
        auto main_component = Container::Tab
        ({
            // Files tab
            Renderer([this]{
                RefreshFileList();
                using namespace ftxui;
                Elements file_elements;
                for (const auto& file : file_list) 
                {
                    file_elements.push_back(text(file));
                }
                return vbox({
                    text("Your Files:"),
                    separator(),
                    vbox(file_elements)
                });
            }),
            
            // Upload tab
            Renderer
            (
                [this]
                {
                    using namespace ftxui;
                    return vbox
                    (
                        {
                            text("Upload File:"),
                            separator(),
                            text("Use command line for file upload")
                        }
                    );
                }
            ),
            
            // Storage tab
            Renderer
            (
                [this]
                {
                    auto storage_info = GetStorageInfo();
                    using namespace ftxui;
                    return vbox
                    (
                        {
                            text("Storage Information:"),
                            separator(),
                            text(storage_info)
                        }
                    );
                }
            )
        }, &selected_tab);
        
        auto layout = Container::Vertical
        (
            {
                tab_selection,
                main_component
            }
        );
        
        screen.Loop(layout);
    }

    void RefreshFileList()
    {
        httplib::Params params;
        params.emplace("username", current_user);
        
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
        }
    }

    std::string GetStorageInfo()
    {
        httplib::Params params;
        params.emplace("username", current_user);
        
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
    
    Client client(server_address, port);
    client.Run();
    
    return 0;
}