#include <memory>
#include <locale>
#include <clocale>
#include "ftxui/dom/elements.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include <string>
#include <vector>
#include <httplib.h>

class Client
{
private:
    std::unique_ptr<httplib::Client> http_client;
    std::string current_user;
    std::string server_url;
    std::vector<std::string> file_list;
    std::string status_message;

public:
    Client(const std::string& server_address = "localhost", int port = 8080)
        : http_client(nullptr)
        , server_url("http://" + server_address + ":" + std::to_string(port))
        , status_message("Ready")
    {
        std::cout << "DEBUG: Client constructor start" << std::endl;
        std::cout << "DEBUG: server_url = '" << server_url << "'" << std::endl;
        
        http_client = std::make_unique<httplib::Client>(server_url);
        
        std::cout << "DEBUG: http_client created successfully" << std::endl;
        std::cout << "DEBUG: Client constructor end" << std::endl;
    }

    bool Login(const std::string& username, const std::string& password)
    {
        httplib::Params params;
        params.emplace("username", username);
        params.emplace("password", password);
        
        auto response = http_client->Post("/login", params);
        
        if (response && response->status == 200) 
        {
            current_user = username;
            return true;
        }
        return false;
    }

    bool Register(const std::string& username, const std::string& password)
    {
        std::cout << "\n========== CLIENT DEBUG ==========" << std::endl;
        std::cout << "Register function called" << std::endl;
        std::cout << "Username: '" << username << "'" << std::endl;
        std::cout << "Password: '" << password << "' (length: " << password.length() << ")" << std::endl;

        std::string body = "username=" + username + "&password=" + password;
        std::cout << "Request body: '" << body << "'" << std::endl;

        httplib::Headers headers = 
        {
            {"Content-Type", "application/x-www-form-urlencoded"},
            {"Content-Length", std::to_string(body.length())}
        };

        std::cout << "Sending POST request to: " << server_url << "/register" << std::endl;

        auto response = http_client->Post("/register", headers, body, "application/x-www-form-urlencoded");

        if (response) 
        {
            std::cout << "Response received!" << std::endl;
            std::cout << "Status code: " << response->status << std::endl;
            std::cout << "Response body: '" << response->body << "'" << std::endl;
            return response->status == 200;
        } 
        else 
        {
            auto err = response.error();
            std::cout << "CONNECTION ERROR: " << httplib::to_string(err) << std::endl;
            return false;
        }
    }

    void Run()
    {
        using namespace ftxui;
        
        auto screen = ScreenInteractive::TerminalOutput();
        
        // Login screen
        std::string username, password;
        bool login_success = false;
        std::string login_status = "";
        
        auto username_input = Input(&username, "Username");
        auto password_input = Input(&password, "Password");
        
        password_input |= CatchEvent([&](Event event) 
        {
            return false;
        });
        
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
                screen.PostEvent(Event::Custom);
            }
        });
        
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
            screen.PostEvent(Event::Custom);  // Force screen refresh
        });
        
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

    void ShowMainMenu(ftxui::ScreenInteractive& screen)
    {
        using namespace ftxui;

        int selected_tab = 0;
        int previous_tab = 0;  // Track tab changes
        std::vector<std::string> tab_titles = {"Files", "Upload", "Storage"};

        auto tab_menu = Menu(&tab_titles, &selected_tab);

        auto files_tab = Renderer([&] 
        {
            // Refresh when this tab is rendered
            if (selected_tab == 0) 
            {
                RefreshFileList();
            }

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

        auto upload_tab = Renderer([&] 
        {
            return vbox({
                text("Upload File:") | bold,
                separator(),
                text("Use command line to upload files:"),
                text(""),
                text("  curl -X POST " + server_url + "/upload \\"),  // FIXED: removed extra http://
                text("    -F \"username=" + current_user + "\" \\"),
                text("    -F \"filename=yourfile.jpg\" \\"),
                text("    -F \"file=@/path/to/yourfile.jpg\""),
                text(""),
                text("Or use the test script:")
            }) | border;
        });

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
            // Force refresh when switching to Files tab
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
        auto response = http_client->Get(url.c_str());
        
        if (response && response->status == 200) 
        {
            return response->body;
        }
        return "Unable to retrieve storage info";
    }
};

int main(int argc, char* argv[])
{
    // Set locale for FTXUI
    try {
        std::setlocale(LC_ALL, "en_US.UTF-8");
        std::locale::global(std::locale("en_US.UTF-8"));
    }
    catch (const std::exception& e) {
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