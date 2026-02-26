#include <cstddef>
#include <memory>
#include <locale>
#include <clocale>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <httplib.h>
#include "utility/tinyfiledialogs.h"
#include "ftxui/dom/elements.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"

namespace fs = std::filesystem;

class TaskQueue
{
private:
    std::queue<std::function<void()>> tasks;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> running{true};

public:
    TaskQueue()
    {
        worker = std::thread([this] 
        {
            while (running)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    cv.wait(lock, [this] { return !tasks.empty() || !running; });
                    if (!running && tasks.empty()) return;
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }

    ~TaskQueue()
    {
        running = false;
        cv.notify_one();
        if (worker.joinable()) worker.join();
    }

    void post(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            tasks.push(std::move(task));
        }
        cv.notify_one();
    }
};

class Client
{
private:
    std::unique_ptr<httplib::Client> http_client;
    std::string current_user;
    std::string server_url;
    std::vector<std::string> file_list;
    std::vector<std::string> cached_file_list;
    std::string status_message;
    std::atomic<bool> server_connected;
    std::chrono::steady_clock::time_point last_heartbeat;
    std::chrono::steady_clock::time_point last_file_refresh;
    bool showing_main_menu;
    std::string selected_file_path;
    std::string upload_status;
    std::thread monitor_thread;
    std::atomic<bool> monitoring_active;
    std::mutex screen_mutex;
    ftxui::ScreenInteractive* current_screen;
    TaskQueue task_queue;
    std::atomic<bool> is_loading_files{false};
    std::atomic<bool> is_uploading{false};
    std::atomic<int> loading_progress{0};
    std::chrono::steady_clock::time_point upload_start_time;

public:
    Client(const std::string& server_address = "localhost", int port = 8080)
        : http_client(nullptr)
        , server_url(server_address)
        , status_message("Ready")
        , server_connected(false)
        , last_file_refresh(std::chrono::steady_clock::now())
        , showing_main_menu(false)
        , monitoring_active(false) 
        , current_screen(nullptr)
    {
        if (server_address.find("http://") == 0 || server_address.find("https://") == 0)
        {
            server_url = server_address;
        }
        else
        {
            server_url = "http://" + server_address + ":" + std::to_string(port);
        }

        http_client = std::make_unique<httplib::Client>(server_url.c_str());
        http_client->set_connection_timeout(2);
        http_client->set_read_timeout(5);
        http_client->set_write_timeout(5);
        http_client->set_keep_alive(true);

        CheckConnection();
    }

    ~Client()
    {
        StopMonitoring();
    }

    void StartMonitoring(ftxui::ScreenInteractive* screen)
    {
        current_screen = screen;
        monitoring_active = true;
        monitor_thread = std::thread(&Client::MonitorConnection, this);
    }

    void StopMonitoring()
    {
        monitoring_active = false;
        if (monitor_thread.joinable())
        {
            monitor_thread.join();
        }
    }

    void MonitorConnection()
    {
        while (monitoring_active)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));

            bool was_connected = server_connected;
            CheckConnection();

            if (was_connected != server_connected && current_screen)
            {
                std::lock_guard<std::mutex> lock(screen_mutex);

                if (server_connected)
                {
                    status_message = "Server connected";
                    if (showing_main_menu && !current_user.empty())
                    {
                        RefreshFileListAsync();
                    }
                }
                else
                {
                    status_message = "Server disconnected";
                }

                current_screen->PostEvent(ftxui::Event::Custom);
            }
        }
    }

    void CheckConnection()
    {
        auto res = http_client->Get("/");
        server_connected = (res && res->status == 200);

        if (server_connected)
        {
            last_heartbeat = std::chrono::steady_clock::now();
        }
    }

    bool Login(const std::string& username, const std::string& password)
    {
        if (!server_connected)
        {
            status_message = "Server disconnected";
            return false;
        }

        httplib::Params params;
        params.emplace("username", username);
        params.emplace("password", password);

        auto response = http_client->Post("/login", params);

        if (response && response->status == 200)
        {
            current_user = username;
            status_message = "Login successful";
            last_heartbeat = std::chrono::steady_clock::now();
            return true;
        }

        status_message = response ? "Login failed: " + response->body : "Server not responding";
        return false;
    }

    bool Register(const std::string& username, const std::string& password)
    {
        if (!server_connected)
        {
            status_message = "Server disconnected";
            return false;
        }

        httplib::Params params;
        params.emplace("username", username);
        params.emplace("password", password);

        auto response = http_client->Post("/register", params);

        if (response && response->status == 200)
        {
            status_message = "Registration successful";
            last_heartbeat = std::chrono::steady_clock::now();
            return true;
        }

        status_message = response ? "Registration failed: " + response->body : "Server not responding";
        return false;
    }

    bool UploadFile(const std::string& filepath)
    {
        if (!server_connected)
        {
            status_message = "Server disconnected";
            return false;
        }

        fs::path path(filepath);

        if (!fs::exists(path))
        {
            status_message = "File does not exist";
            return false;
        }

        if (!fs::is_regular_file(path))
        {
            status_message = "Not a regular file";
            return false;
        }

        std::ifstream file(path, std::ios::binary);

        if (!file)
        {
            status_message = "Cannot open file";
            return false;
        }

        std::string filename = path.filename().string();
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        if (content.empty())
        {
            status_message = "File is empty";
            return false;
        }

        httplib::MultipartFormDataItems items = 
        {
            {"username", current_user, "", ""},
            {"filename", filename, "", ""},
            {"file", content, filename, "application/octet-stream"}
        };

        auto response = http_client->Post("/upload", items);

        if (response && response->status == 200)
        {
            status_message = "Upload successful: " + filename;
            last_heartbeat = std::chrono::steady_clock::now();
            RefreshFileList();
            return true;
        }

        status_message = response ? "Upload failed: " + response->body : "Server not responding";
        return false;
    }

    void RefreshFileListAsync()
    {
        if (is_loading_files || !server_connected || current_user.empty()) return;

        is_loading_files = true;
        status_message = "Loading files...";
        if (current_screen) current_screen->PostEvent(ftxui::Event::Custom);

        task_queue.post([this]()
        {
            std::string url = "/files?username=" + current_user;
            auto response = http_client->Get(url.c_str());

            std::vector<std::string> new_files;

            if (response && response->status == 200)
            {
                std::string content = response->body;
                size_t pos = 0;
                while ((pos = content.find('\n')) != std::string::npos)
                {
                    std::string file = content.substr(0, pos);
                    if (!file.empty() && file != "No files found")
                    {
                        new_files.push_back(file);
                    }
                    content.erase(0, pos + 1);
                }

                {
                    std::lock_guard<std::mutex> lock(screen_mutex);
                    cached_file_list = new_files;
                    file_list = new_files;
                    last_file_refresh = std::chrono::steady_clock::now();
                    status_message = "Found " + std::to_string(new_files.size()) + " files";
                }
            }
            else
            {
                std::lock_guard<std::mutex> lock(screen_mutex);
                status_message = "Failed to load files";
                file_list = cached_file_list;
            }

            is_loading_files = false;
            if (current_screen) current_screen->PostEvent(ftxui::Event::Custom);
        });
    }

    void UploadFileAsync(const std::string& filepath)
    {
        if (is_uploading || !server_connected || current_user.empty()) return;

        is_uploading = true;
        upload_start_time = std::chrono::steady_clock::now();
        upload_status = "Starting upload...";

        task_queue.post([this, filepath]()
        {
            fs::path path(filepath);
            bool success = false;
            std::string filename = path.filename().string();

            if (fs::exists(path) && fs::is_regular_file(path))
            {
                std::ifstream file(path, std::ios::binary);
                if (file)
                {
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    file.close();

                    if (!content.empty())
                    {
                        httplib::MultipartFormDataItems items = 
                        {
                            {"username", current_user, "", ""},
                            {"filename", filename, "", ""},
                            {"file", content, filename, "application/octet-stream"}
                        };

                        auto response = http_client->Post("/upload", items);
                        success = (response && response->status == 200);
                    }
                }
            }

            std::lock_guard<std::mutex> lock(screen_mutex);
            if (success)
            {
                upload_status = "Upload successful!";
                selected_file_path = "";
                RefreshFileListAsync();
            }
            else
            {
                upload_status = "Upload failed";
            }

            is_uploading = false;
            if (current_screen) current_screen->PostEvent(ftxui::Event::Custom);
        });
    }

    void DownloadFileAsync(const std::string& filename)
    {
        task_queue.post([this, filename]()
        {
            std::string url = "/download?username=" + current_user + "&filename=" + filename;
            auto response = http_client->Get(url.c_str());

            bool success = false;

            if (response && response->status == 200)
            {
                fs::path save_path = fs::current_path() / filename;
                std::ofstream file(save_path, std::ios::binary);

                if (file)
                {
                    file.write(response->body.c_str(), response->body.size());
                    file.close();
                    success = true;
                }
            }

            std::lock_guard<std::mutex> lock(screen_mutex);
            if (success)
            {
                status_message = "Downloaded: " + filename;
            }
            else
            {
                status_message = "Download failed";
            }

            if (current_screen) current_screen->PostEvent(ftxui::Event::Custom);
        });
    }

    std::string OpenFileDialog()
    {
        const char* filters[] = { "*" };
        const char* result = tinyfd_openFileDialog(
            "Select File to Upload",
            "",
            1,
            filters,
            "All Files",
            0
        );

        if (result != NULL)
        {
            return std::string(result);
        }

        return "";
    }

    ftxui::Element RenderSpinner(int frame)
    {
        using namespace ftxui;
        static const std::vector<std::string> frames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
        return text(frames[frame % frames.size()]) | color(Color::Yellow);
    }

    void Run()
    {
        using namespace ftxui;
        auto screen = ScreenInteractive::TerminalOutput();

        StartMonitoring(&screen);

        std::string username, password;
        bool login_success = false;
        std::string login_status = "";

        auto username_input = Input(&username, "Username");
        auto password_input = Input(&password, "Password");

        auto login_button = Button("Login", [&]
        {
            if (!server_connected)
            {
                login_status = "Server disconnected";
                return;
            }

            login_status = "Logging in...";

            task_queue.post([&]()
            {
                if (Login(username, password))
                {
                    showing_main_menu = true;
                    login_success = true;
                    screen.ExitLoopClosure()();
                }
                else
                {
                    login_status = status_message;
                    screen.PostEvent(Event::Custom);
                }
            });
        });

        auto register_button = Button("Register", [&]
        {
            if (!server_connected)
            {
                login_status = "Server disconnected";
                return;
            }

            login_status = "Registering...";

            task_queue.post([&]()
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
            });
        });

        auto quit_button = Button("Quit", [&]
        {
            StopMonitoring();
            screen.ExitLoopClosure()();
        });

        auto reconnect_button = Button("Reconnect", [&]
        {
            login_status = "Reconnecting...";

            task_queue.post([&]()
            {
                http_client = std::make_unique<httplib::Client>(server_url.c_str());
                http_client->set_connection_timeout(2);
                http_client->set_read_timeout(5);
                http_client->set_write_timeout(5);
                http_client->set_keep_alive(true);

                CheckConnection();
                login_status = server_connected ? "Connected to server" 
                                                : "Cannot connect to server";
                screen.PostEvent(Event::Custom);
            });
        });

        auto login_components = Container::Vertical
        ({
            username_input,
            password_input,
            Container::Horizontal({login_button, register_button, quit_button}),
            reconnect_button
        });

        auto login_renderer = Renderer(login_components, [&]
        {
            std::string conn_status = server_connected ? "● Connected" : "○ Disconnected";
            Color conn_color = server_connected ? Color::Green : Color::Red;

            return vbox
            ({
                text("E2Eye Login") | bold | center,
                separator(),
                hbox(
                    text("Server: " + server_url) | dim,
                    filler(),
                    text(conn_status) | color(conn_color) | bold
                ),
                separator(),
                text("Username:"),
                username_input->Render(),
                text("Password:"),
                password_input->Render(),
                separator(),
                hbox(login_button->Render(), register_button->Render(), quit_button->Render()),
                text("") | size(HEIGHT, EQUAL, 1),
                text(login_status) | color(Color::Yellow),
                text("") | size(HEIGHT, EQUAL, 1),
                reconnect_button->Render(),
                text("Press Ctrl+C to quit") | dim
            }) | border;
        });

        screen.Loop(login_renderer);

        if (login_success)
        {
            ShowMainMenu(screen);
        }
        else
        {
            StopMonitoring();
        }
    }

    void ShowMainMenu(ftxui::ScreenInteractive& screen)
    {
        using namespace ftxui;

        int selected_tab = 0;
        int previous_tab = 0;
        int selected_file = 0;
        int spinner_frame = 0;
        std::vector<std::string> tab_titles = {"Files", "Upload", "Storage"};

        auto tab_menu = Menu(&tab_titles, &selected_tab);

        auto download_btn = Button("Download", [&]
        {
            if (!server_connected)
            {
                status_message = "Server disconnected";
                return;
            }

            if (cached_file_list.empty() || selected_file >= (int)cached_file_list.size())
            {
                status_message = "No file selected";
                return;
            }

            std::string filename = cached_file_list[selected_file];
            size_t space = filename.find(' ');
            if (space != std::string::npos)
            {
                filename = filename.substr(0, space);
            }

            status_message = "Downloading " + filename + "...";
            DownloadFileAsync(filename);
        });

        auto refresh_btn = Button("Refresh", [&]
        {
            if (server_connected)
            {
                RefreshFileListAsync();
            }
            else
            {
                status_message = "Server disconnected";
            }
        });

        auto files_logout_btn = Button("Logout", [&]
        {
            showing_main_menu = false;
            current_user = "";
            screen.ExitLoopClosure()();
        });

        auto files_buttons = Container::Horizontal({
            download_btn,
            refresh_btn,
            files_logout_btn
        });

        auto files_tab = Renderer(files_buttons, [&]
        {
            if (!server_connected)
            {
                return vbox
                ({
                    text("Connection Lost") | bold | center | color(Color::Red),
                    separator(),
                    text("Server is not responding") | center,
                    text("") | size(HEIGHT, EQUAL, 1),
                    hbox({
                        files_logout_btn->Render()
                    }) | center
                }) | border;
            }

            const auto& display_list = is_loading_files ? cached_file_list : file_list;

            Elements file_elements;

            if (is_loading_files)
            {
                file_elements.push_back
                (
                    hbox
                    (
                        RenderSpinner(spinner_frame++),
                        text(" Loading files...") | color(Color::Yellow)
                    )
                );
            }

            for (size_t i = 0; i < display_list.size(); ++i)
            {
                if ((int)i == selected_file)
                {
                    file_elements.push_back
                    (
                        text("-> " + display_list[i]) | bold | color(Color::Blue)
                    );
                }
                else
                {
                    file_elements.push_back(text("   " + display_list[i]));
                }
            }

            if (display_list.empty() && !is_loading_files)
            {
                file_elements.push_back(text("   No files found"));
            }

            return vbox({
                text("Your Files - " + current_user) | bold | center,
                separator(),
                vbox(file_elements) | frame | size(HEIGHT, LESS_THAN, 15),
                separator(),
                hbox({
                    download_btn->Render(),
                    text("  "),
                    refresh_btn->Render(),
                    text("  "),
                    files_logout_btn->Render()
                }),
                text("") | size(HEIGHT, EQUAL, 1),
                text(status_message) | color(Color::Blue)
            }) | border;
        });

        auto browse_btn = Button("Browse", [&]
        {
            std::string result = OpenFileDialog();

            if (!result.empty())
            {
                selected_file_path = result;
                upload_status = "File selected: " + fs::path(result).filename().string();
            }
            else
            {
                upload_status = "No file selected";
            }
        });

        auto upload_btn = Button("Upload", [&]
        {
            if (selected_file_path.empty())
            {
                upload_status = "No file selected";
                return;
            }

            if (!server_connected)
            {
                upload_status = "Server disconnected";
                return;
            }

            if (current_user.empty())
            {
                upload_status = "Not logged in";
                return;
            }

            UploadFileAsync(selected_file_path);
        });

        auto upload_back_btn = Button("Back", [&] { selected_tab = 0; });

        auto upload_logout_btn = Button("Logout", [&]
        {
            showing_main_menu = false;
            current_user = "";
            screen.ExitLoopClosure()();
        });

        auto upload_buttons = Container::Horizontal({
            browse_btn,
            upload_btn,
            upload_back_btn,
            upload_logout_btn
        });

        auto upload_tab = Renderer(upload_buttons, [&]
        {
            if (!server_connected)
            {
                return vbox({
                    text("Connection Lost") | bold | center | color(Color::Red),
                    separator(),
                    text("Server is not responding") | center,
                    text("") | size(HEIGHT, EQUAL, 1),
                    hbox({
                        upload_back_btn->Render(),
                        text("  "),
                        upload_logout_btn->Render()
                    }) | center
                }) | border;
            }

            return vbox({
                text("Upload File") | bold | center,
                separator(),
                text("Current user: " + current_user) | color(Color::Blue),
                separator(),
                text("Selected file:"),
                text("  " + (selected_file_path.empty() ? "(none)" : selected_file_path)) |
                    color(selected_file_path.empty() ? Color::GrayDark : Color::Green),
                separator(),
                is_uploading ?
                    hbox(
                        RenderSpinner(spinner_frame++),
                        text(" Uploading...") | color(Color::Yellow)
                    ) :
                    hbox({
                        browse_btn->Render(),
                        text("  "),
                        upload_btn->Render(),
                        text("  "),
                        upload_back_btn->Render(),
                        text("  "),
                        upload_logout_btn->Render()
                    }),
                text("") | size(HEIGHT, EQUAL, 1),
                text(upload_status) | color(upload_status.find("success") != std::string::npos ? Color::Green :
                                           upload_status.find("fail") != std::string::npos ? Color::Red : Color::GrayDark)
            }) | border;
        });

        auto storage_refresh_btn = Button("Refresh", [&]
        {
            status_message = "Storage info refreshed";
        });

        auto storage_back_btn = Button("Back", [&] { selected_tab = 0; });

        auto storage_logout_btn = Button("Logout", [&]
        {
            showing_main_menu = false;
            current_user = "";
            screen.ExitLoopClosure()();
        });

        auto storage_buttons = Container::Horizontal({
            storage_refresh_btn,
            storage_back_btn,
            storage_logout_btn
        });

        auto storage_tab = Renderer(storage_buttons, [&]
        {
            if (!server_connected)
            {
                return vbox({
                    text("Connection Lost") | bold | center | color(Color::Red),
                    separator(),
                    text("Server is not responding") | center,
                    text("") | size(HEIGHT, EQUAL, 1),
                    hbox({
                        storage_back_btn->Render(),
                        text("  "),
                        storage_logout_btn->Render()
                    }) | center
                }) | border;
            }

            std::string storage_info = GetStorageInfo();

            return vbox({
                text("Storage Information") | bold | center,
                separator(),
                text(storage_info),
                text(""),
                hbox({
                    storage_refresh_btn->Render(),
                    text("  "),
                    storage_back_btn->Render(),
                    text("  "),
                    storage_logout_btn->Render()
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

        auto main_layout = Container::Vertical(
        {
            tab_menu,
            tab_container
        });

        auto main_renderer = Renderer(main_layout, [&]
        {
            std::string conn_status = server_connected ? "● Connected" : "○ Disconnected";
            Color conn_color = server_connected ? Color::Green : Color::Red;

            std::string ping_display = "";
            if (server_connected)
            {
                auto now = std::chrono::steady_clock::now();
                auto ping = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_heartbeat).count();
                ping_display = std::to_string(ping) + "ms";
            }

            if (selected_tab == 0 && previous_tab != selected_tab && server_connected)
            {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_file_refresh).count();
                if (elapsed > 30)
                {
                    RefreshFileListAsync();
                }
                else
                {
                    file_list = cached_file_list;
                }
            }
            previous_tab = selected_tab;

            return vbox({
                hbox(
                    text("E2Eye - User: " + current_user) | bold,
                    filler(),
                    text(ping_display) | dim,
                    text("  "),
                    text(conn_status) | color(conn_color) | bold
                ) | center,
                separator(),
                tab_menu->Render() | hcenter,
                separator(),
                tab_container->Render() | flex,
            }) | border;
        });

        auto main_component = main_layout | CatchEvent([&](Event event)
        {
            if (event == Event::Character('q') || event == Event::Character('Q') || event == Event::Escape)
            {
                showing_main_menu = false;
                current_user = "";
                screen.ExitLoopClosure()();
                return true;
            }

            if (selected_tab == 0 && server_connected)
            {
                if (event == Event::ArrowUp && selected_file > 0)
                {
                    selected_file--;
                    return true;
                }
                if (event == Event::ArrowDown && selected_file < (int)file_list.size() - 1)
                {
                    selected_file++;
                    return true;
                }
            }

            return false;
        });

        if (server_connected && !current_user.empty())
        {
            RefreshFileListAsync();
        }

        screen.Loop(main_component);

        if (showing_main_menu == false)
        {
            StopMonitoring();
            Run();
        }
    }

    void RefreshFileList()
    {
        if (!server_connected)
        {
            return;
        }

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

            last_heartbeat = std::chrono::steady_clock::now();
        }
    }

    std::string GetStorageInfo()
    {
        if (!server_connected)
        {
            return "Cannot connect to server";
        }

        std::string url = "/storage?username=" + current_user;
        auto response = http_client->Get(url.c_str());

        if (response && response->status == 200)
        {
            last_heartbeat = std::chrono::steady_clock::now();
            return response->body;
        }

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

    try
    {
        while (true)
        {
            Client client(server_address, port);
            client.Run();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
