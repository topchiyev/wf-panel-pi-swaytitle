#include "swaytitle.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <vector>

extern "C" {
    WayfireWidget *create() { return new WayfireSwayTitle; }
    void destroy(WayfireWidget *w) { delete w; }

    static int default_max_chars = 20;
    static constexpr conf_table_t conf_table[2] = {
        {CONF_TYPE_INT, "swaytitle_max_chars", N_("Maximum visible title characters"), &default_max_chars},
        {CONF_TYPE_NONE, NULL, NULL, NULL}
    };
    const conf_table_t *config_params(void) { return conf_table; }
    const char *display_name(void) { return N_("SwayTitle"); }
    const char *package_name(void) { return GETTEXT_PACKAGE; }
}

namespace
{
constexpr char SWAY_IPC_MAGIC[] = "i3-ipc";
constexpr std::uint32_t IPC_GET_TREE = 4;
constexpr std::uint32_t IPC_SUBSCRIBE = 2;

bool read_exact(int fd, void *buffer, std::size_t size)
{
    auto *data = static_cast<char*>(buffer);
    std::size_t total_read = 0;

    while (total_read < size)
    {
        const auto bytes_read = read(fd, data + total_read, size - total_read);

        if (bytes_read == 0)
        {
            return false;
        }

        if (bytes_read < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return false;
        }

        total_read += static_cast<std::size_t>(bytes_read);
    }

    return true;
}

bool write_exact(int fd, const void *buffer, std::size_t size)
{
    const auto *data = static_cast<const char*>(buffer);
    std::size_t total_written = 0;

    while (total_written < size)
    {
        const auto bytes_written = write(fd, data + total_written, size - total_written);

        if (bytes_written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return false;
        }

        total_written += static_cast<std::size_t>(bytes_written);
    }

    return true;
}

int connect_to_sway_socket(bool with_timeout)
{
    const char *socket_path = std::getenv("SWAYSOCK");
    if ((socket_path == nullptr) || (*socket_path == '\0'))
    {
        return -1;
    }

    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    if (with_timeout)
    {
        timeval timeout {1, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    }

    sockaddr_un address {};
    address.sun_family = AF_UNIX;
    std::snprintf(address.sun_path, sizeof(address.sun_path), "%s", socket_path);

    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

bool send_ipc_message(int fd, std::uint32_t type, const std::string& payload)
{
    std::array<char, 14> header {};
    std::memcpy(header.data(), SWAY_IPC_MAGIC, 6);

    const std::uint32_t length = payload.size();
    std::memcpy(header.data() + 6, &length, sizeof(length));
    std::memcpy(header.data() + 10, &type, sizeof(type));

    return write_exact(fd, header.data(), header.size()) &&
        write_exact(fd, payload.data(), payload.size());
}

bool read_ipc_message(int fd, std::uint32_t& type, std::string& payload)
{
    std::array<char, 14> header {};
    if (!read_exact(fd, header.data(), header.size()))
    {
        return false;
    }

    if (std::memcmp(header.data(), SWAY_IPC_MAGIC, 6) != 0)
    {
        return false;
    }

    std::uint32_t length = 0;
    std::memcpy(&length, header.data() + 6, sizeof(length));
    std::memcpy(&type, header.data() + 10, sizeof(type));

    payload.assign(length, '\0');
    if ((length > 0) && !read_exact(fd, payload.data(), length))
    {
        return false;
    }

    return true;
}

std::vector<std::string> collect_json_objects(const std::string& json)
{
    std::vector<std::string> objects;
    std::vector<std::size_t> stack;
    bool in_string = false;
    bool escaped = false;

    for (std::size_t index = 0; index < json.size(); ++index)
    {
        const char ch = json[index];

        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (in_string && (ch == '\\'))
        {
            escaped = true;
            continue;
        }

        if (ch == '"')
        {
            in_string = !in_string;
            continue;
        }

        if (in_string)
        {
            continue;
        }

        if (ch == '{')
        {
            stack.push_back(index);
        } else if ((ch == '}') && !stack.empty())
        {
            const std::size_t start = stack.back();
            stack.pop_back();
            objects.push_back(json.substr(start, index - start + 1));
        }
    }

    return objects;
}

std::string unescape_json_string(const std::string& text)
{
    std::string result;
    result.reserve(text.size());

    bool escaped = false;
    for (const char ch : text)
    {
        if (!escaped)
        {
            if (ch == '\\')
            {
                escaped = true;
            } else
            {
                result.push_back(ch);
            }

            continue;
        }

        switch (ch)
        {
          case '"': result.push_back('"'); break;
          case '\\': result.push_back('\\'); break;
          case '/': result.push_back('/'); break;
          case 'b': result.push_back('\b'); break;
          case 'f': result.push_back('\f'); break;
          case 'n': result.push_back('\n'); break;
          case 'r': result.push_back('\r'); break;
          case 't': result.push_back('\t'); break;
          default: result.push_back(ch); break;
        }

        escaped = false;
    }

    return result;
}

bool extract_string_field(const std::string& object, const char *field, std::string& value)
{
    const std::regex pattern(
        std::string{"\""} + field + "\"\\s*:\\s*\"((?:\\\\.|[^\"])*)\""
    );
    std::smatch match;

    if (!std::regex_search(object, match, pattern))
    {
        return false;
    }

    value = unescape_json_string(match[1].str());
    return true;
}

bool extract_bool_field(const std::string& object, const char *field, bool& value)
{
    const std::regex pattern(std::string{"\""} + field + "\"\\s*:\\s*(true|false)");
    std::smatch match;

    if (!std::regex_search(object, match, pattern))
    {
        return false;
    }

    value = (match[1].str() == "true");
    return true;
}
}

void WayfireSwayTitle::init(Gtk::HBox *container)
{
    label.set_name(PLUGIN_NAME);
    label.set_margin_start(4);
    label.set_margin_end(4);
    label.set_text("");
    label.show();

    container->pack_start(label, false, false);

    refresh_dispatcher.connect(sigc::mem_fun(*this, &WayfireSwayTitle::refresh_title));

    max_chars.set_callback([this] ()
    {
        refresh_title();
    });

    refresh_title();
    start_event_listener();
}

void WayfireSwayTitle::command(const char *cmd)
{
    (void)cmd;
    refresh_title();
}

WayfireSwayTitle::~WayfireSwayTitle()
{
    stop_event_listener_thread();
}

void WayfireSwayTitle::refresh_title()
{
    std::string title = query_focused_title();
    if (title.empty())
    {
        title = "";
    }

    const std::string shown = trim_title(title);
    label.set_text(shown);
    label.set_tooltip_text(title);
}

std::string WayfireSwayTitle::query_focused_title() const
{
    const int fd = connect_to_sway_socket(true);
    if (fd < 0)
    {
        return {};
    }

    std::string payload;
    std::uint32_t type = 0;

    if (!send_ipc_message(fd, IPC_GET_TREE, "") || !read_ipc_message(fd, type, payload))
    {
        close(fd);
        return {};
    }

    close(fd);

    if (type != IPC_GET_TREE)
    {
        return {};
    }

    const auto objects = collect_json_objects(payload);
    for (const auto& object : objects)
    {
        bool focused = false;
        if (!extract_bool_field(object, "focused", focused) || !focused)
        {
            continue;
        }

        std::string type_name;
        if (!extract_string_field(object, "type", type_name))
        {
            continue;
        }

        if ((type_name != "con") && (type_name != "floating_con"))
        {
            continue;
        }

        std::string name;
        if (!extract_string_field(object, "name", name))
        {
            continue;
        }

        if (!name.empty())
        {
            return name;
        }
    }

    return {};
}

std::string WayfireSwayTitle::trim_title(const std::string& title) const
{
    int max = max_chars;
    if (max <= 0)
    {
        max = 20;
    }

    Glib::ustring value = title;
    if (static_cast<int>(value.size()) <= max)
    {
        return title;
    }

    return value.substr(0, max).raw() + "…";
}

void WayfireSwayTitle::start_event_listener()
{
    stop_event_listener = false;
    event_thread = std::thread(&WayfireSwayTitle::listen_for_sway_events, this);
}

void WayfireSwayTitle::stop_event_listener_thread()
{
    stop_event_listener = true;

    const int fd = event_socket_fd.exchange(-1);
    if (fd >= 0)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    if (event_thread.joinable())
    {
        event_thread.join();
    }
}

void WayfireSwayTitle::listen_for_sway_events()
{
    const int fd = connect_to_sway_socket(false);
    if (fd < 0)
    {
        return;
    }

    event_socket_fd = fd;

    if (!send_ipc_message(fd, IPC_SUBSCRIBE, "[\"window\",\"workspace\"]"))
    {
        close(fd);
        event_socket_fd = -1;
        return;
    }

    std::uint32_t type = 0;
    std::string payload;
    if (!read_ipc_message(fd, type, payload))
    {
        close(fd);
        event_socket_fd = -1;
        return;
    }

    while (!stop_event_listener)
    {
        if (!read_ipc_message(fd, type, payload))
        {
            break;
        }

        refresh_dispatcher.emit();
    }

    const int open_fd = event_socket_fd.exchange(-1);
    if (open_fd >= 0)
    {
        close(open_fd);
    }
}