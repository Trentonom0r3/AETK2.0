#include <aetk/aegp.hpp>

// SDK example headers
#include "DuckSuite.h"

#ifdef AE_OS_WIN
    #include <windows.h>
#else
    #include <AudioToolbox/AudioServices.h>
#endif

/**
 * @brief Modernized Duck Logic.
 * 
 * This class contains the actual implementation logic for the Sweetie sample.
 * It demonstrates how to use OS-native calls (MessageBox/AudioServices) 
 * within a modernized C++ structure.
 */
class duck_logic {
public:
    static duck_logic& instance() {
        static duck_logic s_instance;
        return s_instance;
    }

    void quack(int32_t times) {
        for (int32_t i = 0; i < times; ++i) {
            // Direct OS-native "Quack" logic as seen in the original SDK sample
#ifdef AE_OS_WIN
            MessageBoxA(NULL, "Quack!", "AEGP-provided PICA Suite!", MB_OK);
#else
            AudioServicesPlayAlertSound(kUserPreferredAlert);
#endif
        }
    }
};

/**
 * @brief AEGP Plugin for Sweetie.
 */
class sweetie_plugin : public aetk::aegp::plugin<sweetie_plugin> {
public:
    static void on_entry(const aetk::aegp::entry_context& ctx) {
        
        // 1. Define the PICA proxy.
        // Bridges the binary PICA interface to our modern C++ logic.
        static DuckSuite1 s_duck_suite = {
            .Quack = [](A_u_short times) -> A_Err {
                AETK_EXECUTE(duck_logic::instance().quack(times))
            }
        };

        // 2. Register the suite.
        ctx.register_suite<DuckSuite1>(kDuckSuite1, kDuckSuiteVersion1, &s_duck_suite);
    }
};

// 3. Register the entry point.
AETK_AEGP_MAIN(sweetie_plugin)
