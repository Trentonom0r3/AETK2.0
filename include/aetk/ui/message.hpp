#pragma once

#include <aetk/core/types.hpp>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <AudioToolbox/AudioToolbox.h>
#endif

namespace aetk::ui {

/**
 * @brief Play the system alert sound.
 * 
 * @details On Windows: Triggers MessageBeep(MB_OK).
 * On macOS: Triggers AudioServicesPlayAlertSound(kUserPreferredAlert).
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, displaying dialog notifications requires referencing disparate Win32 API functions or macOS AudioToolbox hooks. `aetk::ui::play_alert_sound` wraps alert dialogs and audio beeps in platform-agnostic modern wrappers.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
inline void play_alert_sound() {
#ifdef _WIN32
    MessageBeep(MB_OK);
#else
    AudioServicesPlayAlertSound(kUserPreferredAlert);
#endif
}

/**
 * @brief Show a simple native message box.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Platform-agnostic notifications.
 *
 * @warning <b>Memory & Lifecycles:</b> Blocks the rendering threads of the host.
 *
 * @param message Content message.
 * @param title Custom title.
 */
inline void message_box(const char* message, const char* title = "AETK") {
#ifdef _WIN32
    MessageBoxA(NULL, message, title, MB_OK);
#else
    // Fallback to alert sound on Mac if no UI provided
    play_alert_sound();
#endif
}

/**
 * @brief Show a simple alert dialog.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Platform-agnostic notifications.
 *
 * @warning <b>Memory & Lifecycles:</b> Blocks the rendering threads of the host.
 *
 * @param message Content message.
 * @param title Custom title.
 */
inline void alert(const char* message, const char* title = "Alert") {
    message_box(message, title);
}

} // namespace aetk::ui
