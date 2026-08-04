#pragma once

/**
 * @file ui.hpp
 * @brief Master inclusion header for AETK Effect UI widgets and components.
 * 
 * @details Unifies all custom overlay drawing components, panel layouts, 
 * widget interactions, and curve editors into a single import boundary.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom overlay drawings (Custom UI) requires including a dozen disparate headers and manually binding Drawbot or Event hooks inside `EffectMain` blocks. `aetk/effect/ui.hpp` provides a unified entry point, exporting widgets, curves, panels, and layouts cleanly.
 *
 * @warning <b>Memory & Lifecycles:</b> The UI system relies on internal static registries (`widget_registry`) which must be registered on plugin startup. Custom UI overlays are exclusively supported in After Effects and incompatible with Premiere Pro.
 */

#include <aetk/ui.hpp>
#include <aetk/effect/ui/widget.hpp>
#include <aetk/effect/ui/layout.hpp>
#include <aetk/effect/ui/panel.hpp>
#include <aetk/effect/ui/component.hpp>
#include <aetk/effect/ui/registration.hpp>
#include <aetk/effect/ui/curve_data.hpp>
#include <aetk/effect/comp_ui.hpp>

// Widgets
#include <aetk/effect/ui/widgets/label.hpp>
#include <aetk/effect/ui/widgets/button.hpp>
#include <aetk/effect/ui/widgets/button_data.hpp>
#include <aetk/effect/ui/widgets/text_input.hpp>
#include <aetk/effect/ui/widgets/slider.hpp>
#include <aetk/effect/ui/widgets/slider_data.hpp>
#include <aetk/effect/ui/widgets/joystick.hpp>
#include <aetk/effect/ui/widgets/joystick_data.hpp>
#include <aetk/effect/ui/widgets/accordion.hpp>
#include <aetk/effect/ui/widgets/accordion_data.hpp>
#include <aetk/effect/ui/widgets/resizable.hpp>
#include <aetk/effect/ui/widgets/resizable_data.hpp>
#include <aetk/effect/ui/widgets/curve_editor.hpp>
#include <aetk/effect/ui/widgets/curve_group.hpp>
#include <aetk/effect/ui/widgets/segment_selector.hpp>
