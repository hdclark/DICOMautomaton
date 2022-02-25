// Tray_Notification.h -- dialogs for notifying the user.

#pragma once

#include <string>
#include <vector>
#include <functional>

enum class notification_urgency_t {
    low,
    medium,
    high
};

struct notification_t {
    int32_t duration = 10'000; // in milliseconds.
    notification_urgency_t urgency = notification_urgency_t::medium;
    //std::string title;
    std::string message;
};

bool
tray_notification(const notification_t &n);

