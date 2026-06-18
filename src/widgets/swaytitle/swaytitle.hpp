#ifndef WIDGETS_SWAYTITLE_HPP
#define WIDGETS_SWAYTITLE_HPP

#include <widget.hpp>

#include <glibmm/dispatcher.h>
#include <gtkmm/label.h>

#include <atomic>
#include <string>
#include <thread>

class WayfireSwayTitle : public WayfireWidget
{
  private:
    Gtk::Label label;

    WfOption<int> max_chars {"panel/swaytitle_max_chars"};

    Glib::Dispatcher refresh_dispatcher;
    std::thread event_thread;
    std::atomic<bool> stop_event_listener {false};
    std::atomic<int> event_socket_fd {-1};

    void refresh_title();
    std::string query_focused_title() const;
    std::string trim_title(const std::string& title) const;

    void start_event_listener();
    void stop_event_listener_thread();
    void listen_for_sway_events();

  public:
    void init(Gtk::HBox *container) override;
    void command(const char *cmd) override;
    ~WayfireSwayTitle() override;
};

#endif /* WIDGETS_SWAYTITLE_HPP */
