#pragma once

#include <aetk/effect/context/context.hpp>
#include <aetk/effect/ui/panel.hpp>
#include <aetk/effect/ui/widget.hpp>
#include <memory>
#include <unordered_map>


namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  Widget Registry & Auto-Event Dispatch
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Manages standalone custom UI widgets and routes events to them
 * automatically.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom Drawbot widget
 * rendering and interactions require manually writing custom user-interface
 * draw hooks, defining complex arbitrary parameter setups, managing absolute
 * padding bounding boxes, and routing events step-by-step in `EffectMain`. Any
 * mismatch in layout sizes, event handling, or state commitments thrashes UI
 * rendering. `aetk::effect::ui::widget_registry` and `add_widget` templates
 * automate this lookup pipeline. It automatically registers UI containers, maps
 * events to corresponding panel dispatch loops, synchronizes rendering states
 * before draws, and automatically enforces a compile-time static assertion in
 * Premiere Pro (since arbitrary data custom UIs are After Effects exclusives).
 *
 * @warning <b>Memory & Lifecycles:</b> The static widget and panel registries
 * (`s_widgets`, `s_panels`) utilize global unique pointer maps. They manage
 * child widget lifetimes persistently across rendering frames, releasing memory
 * safely when the host unloads translation units.
 */
class widget_registry {
public:
  /**
   * @brief Registers widget dynamically into mapping.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Central widget register mapping.
   *
   * @warning <b>Memory & Lifecycles:</b> Transfers unique pointer ownership to
   * static storage.
   *
   * @tparam WidgetType Custom widget type.
   * @param param_idx Parameter identifier index.
   * @param widget Unique pointer to custom widget instance.
   */
  template <typename WidgetType>
  static void register_widget(int32_t param_idx,
                              std::unique_ptr<WidgetType> widget) {
    s_widgets[param_idx] = std::move(widget);
  }

  static void set_widget_tooltip(int32_t param_idx, std::string text) {
    auto it = s_widgets.find(param_idx);
    if (it != s_widgets.end() && it->second) {
      it->second->set_tooltip(std::move(text));
    }
  }

  /**
   * @brief Static event dispatch loop routing host events to active widgets.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Replaces procedural event routing
   * loops.
   *
   * @warning <b>Memory & Lifecycles:</b> Triggers sync and commit state passes.
   *
   * @param ctx Bounded interaction context.
   */
  static void dispatch(const interaction_context &ctx) {
    auto type = ctx.type();

    if (type == interaction_context::event_type::keydown) {
      // For keyboard events, find the active panel that has the focused widget
      for (auto &pair : s_panels) {
        auto *p = pair.second.get();
        if (p && p->focused_widget()) {
          p->dispatch(ctx);
          auto *w = s_widgets[pair.first].get();
          if (w) {
            w->commit_state(ctx);
          }
          return;
        }
      }
    }

    int32_t p_idx = ctx.param_index();

    // Only handle specific parameter Custom UIs right now
    if (p_idx > 0) {
      auto it = s_widgets.find(p_idx);
      if (it != s_widgets.end()) {
        dispatch_to_widget(ctx, p_idx, it->second.get());
      }
    }
  }

  /**
   * @brief Ask the host to refresh all registered custom widget parameters.
   *
   * @details After Effects does not always redraw arbitrary-data custom UIs
   * when the current time changes while playback is paused. Reissuing
   * `PF_UpdateParamUI` for registered widgets during `PF_Cmd_UPDATE_PARAMS_UI`
   * nudges the Effect Controls panel to repaint those custom controls using
   * the newly evaluated keyframed values.
   *
   * @warning <b>Memory & Lifecycles:</b> Uses host-owned `PF_ParamDef`
   * pointers supplied for the current callback only. No widget state is
   * cached or mutated here.
   *
   * @param ctx Bounded UI update context.
   */
  static void refresh_registered_widgets(const ui_update_context &ctx) {
    auto **params = ctx.params_ptr();
    if (!params || !ctx.in_data_ptr() ||
        !::aetk::core::context::get_basic_suite()) {
      return;
    }

    aetk::core::suite<PF_ParamUtilsSuite3> param_utils(
        ::aetk::core::context::get_basic_suite());
    for (const auto &pair : s_widgets) {
      const int32_t param_index = pair.first;
      if (param_index <= 0 || !params[param_index]) {
        continue;
      }
      aetk::core::check_err(param_utils->PF_UpdateParamUI(
          ctx.in_data_ptr()->effect_ref,
          static_cast<PF_ParamIndex>(param_index), params[param_index]));
    }
  }

private:
  static inline std::unordered_map<int32_t, std::unique_ptr<widget>> s_widgets;
  static inline std::unordered_map<int32_t, std::unique_ptr<panel>> s_panels;

  /**
   * @brief Dispatches the event to the custom widget.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Lazy event loop mapping.
   *
   * @warning <b>Memory & Lifecycles:</b> Lazy panel initialization.
   *
   * @param ctx Bounded interaction context.
   * @param p_idx Parameter index.
   * @param w Widget pointer.
   */
  static void dispatch_to_widget(const interaction_context &ctx, int32_t p_idx,
                                 widget *w) {
    // Lazy-create a panel wrapper for this widget to handle AE events
    if (s_panels.find(p_idx) == s_panels.end()) {
      s_panels[p_idx] = std::make_unique<panel>(w);
    }
    auto *p = s_panels[p_idx].get();

    auto type = ctx.type();

    // 1. Sync state BEFORE drawing
    if (type == interaction_context::event_type::draw) {
      w->sync_state(ctx);
    }

    // 2. Dispatch the event through the panel
    p->dispatch(ctx);

    // 3. Commit state AFTER interaction
    if (type == interaction_context::event_type::click ||
        type == interaction_context::event_type::drag ||
        type == interaction_context::event_type::keydown) {
      w->commit_state(ctx);
    }
  }
};

#include <aetk/core/premiere_compat.hpp>
#include <type_traits>


// ══════════════════════════════════════════════════════════════════════
//  Convenience Registration Methods
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Registers a custom Drawbot widget under a specified width/height
 * bounding box.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Fluent registration mapping.
 *
 * @warning <b>Memory & Lifecycles:</b> Fails compile-time assertions on
 * Premiere Pro projects.
 *
 * @tparam WidgetType Custom widget structure.
 * @tparam Args Forwarding constructor arguments.
 * @param ctx Setup context registry.
 * @param name Parameter display name.
 * @param ui_width Parameter custom UI width.
 * @param ui_height Parameter custom UI height.
 * @param args Bounded arguments.
 * @return Parameter builder.
 */
template <typename WidgetType, typename... Args>
auto add_widget(const params_setup_context &ctx, const char *name,
                short ui_width, short ui_height, Args &&...args) {
#ifdef AETK_PREMIERE_COMPAT
  static_assert(
      aetk::core::always_false<WidgetType>::value,
      "AETK Error: Custom UI widgets (ui::add_widget) and Arbitrary Parameters "
      "are After Effects exclusives and incompatible with Premiere Pro.");
#endif
  using DataT = typename WidgetType::data_type;
  DataT default_data =
      WidgetType::get_default_data(std::forward<Args>(args)...);

  int32_t param_idx = ctx.current_count();
  auto widget_inst =
      std::make_unique<WidgetType>(param_idx, std::forward<Args>(args)...);

  int pui_flags = PF_PUI_CONTROL | PF_PUI_TOPIC;
  if (widget_inst->is_title_only()) {
    pui_flags = PF_PUI_TOPIC;
    ui_height = 0;
  }

  auto builder = ctx.add_arbitrary<DataT>(
      name, &default_data, PF_ParamFlag_SUPERVISE,
      pui_flags, ui_width + 4, ui_height + 4);

  // Register widget globally
  widget_registry::register_widget(builder.index, std::move(widget_inst));

  return builder;
}

/**
 * @brief Metaprogramming widget constructor inspector.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Static SFINAE test.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @tparam T Widget type.
 * @tparam Args Argument type templates.
 */
template <typename T, typename... Args> struct is_widget_constructible {
private:
  template <typename U, typename = decltype(U(std::declval<Args>()...))>
  static std::true_type test(int);

  template <typename U> static std::false_type test(...);

public:
  static constexpr bool value = decltype(test<T>(0))::value;
};

/**
 * @brief Registers a custom Drawbot widget and extracts its layout dimensions
 * recursively using measure pass.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Auto-dimensioning custom UI arb parameter
 * mapping.
 *
 * @warning <b>Memory & Lifecycles:</b> Fails compile-time assertions on
 * Premiere Pro projects.
 *
 * @tparam WidgetType Custom widget structure.
 * @tparam Args Forwarding constructor arguments.
 * @param ctx Setup context registry.
 * @param name Parameter display name.
 * @param args Bounded arguments.
 * @return Parameter builder.
 */
template <typename WidgetType, typename... Args,
          typename = std::enable_if_t<is_widget_constructible<
              WidgetType, int32_t, std::decay_t<Args>...>::value>>
auto add_widget(const params_setup_context &ctx, const char *name,
                Args &&...args) {
#ifdef AETK_PREMIERE_COMPAT
  static_assert(
      aetk::core::always_false<WidgetType>::value,
      "AETK Error: Custom UI widgets (ui::add_widget) and Arbitrary Parameters "
      "are After Effects exclusives and incompatible with Premiere Pro.");
#endif
  // 1. Get predicted param index
  int32_t param_idx = ctx.current_count();

  // 2. Instantiate the widget first
  auto widget_inst =
      std::make_unique<WidgetType>(param_idx, std::forward<Args>(args)...);

  // 3. Extract layout dimensions recursively using measure pass
  aetk::core::vec2 measured_sz = widget_inst->measure(250.0f, 1000.0f);
  short ui_width =
      (measured_sz.x > 0.0f) ? static_cast<short>(measured_sz.x) : 250;
  short ui_height =
      (measured_sz.y > 0.0f) ? static_cast<short>(measured_sz.y) : 150;

  using DataT = typename WidgetType::data_type;
  DataT default_data =
      WidgetType::get_default_data(std::forward<Args>(args)...);

  int pui_flags = PF_PUI_CONTROL | PF_PUI_TOPIC;
  if (widget_inst->is_title_only()) {
    pui_flags = PF_PUI_TOPIC;
    ui_height = 0;
  }

  auto builder = ctx.add_arbitrary<DataT>(
      name, &default_data, PF_ParamFlag_SUPERVISE,
      pui_flags, ui_width + 4, ui_height + 4);

  // Register the pre-allocated widget
  widget_registry::register_widget(builder.index, std::move(widget_inst));

  return builder;
}

/**
 * @brief Registers a custom Drawbot widget with custom parameter flags.
 */
template <typename WidgetType, typename... Args,
          typename = std::enable_if_t<is_widget_constructible<
              WidgetType, int32_t, std::decay_t<Args>...>::value>>
auto add_widget_with_flags(const params_setup_context &ctx, const char *name,
                           int32_t param_flags, Args &&...args) {
#ifdef AETK_PREMIERE_COMPAT
  static_assert(
      aetk::core::always_false<WidgetType>::value,
      "AETK Error: Custom UI widgets (ui::add_widget_with_flags) and Arbitrary Parameters "
      "are After Effects exclusives and incompatible with Premiere Pro.");
#endif
  int32_t param_idx = ctx.current_count();
  auto widget_inst =
      std::make_unique<WidgetType>(param_idx, std::forward<Args>(args)...);

  aetk::core::vec2 measured_sz = widget_inst->measure(250.0f, 1000.0f);
  short ui_width =
      (measured_sz.x > 0.0f) ? static_cast<short>(measured_sz.x) : 250;
  short ui_height =
      (measured_sz.y > 0.0f) ? static_cast<short>(measured_sz.y) : 150;

  using DataT = typename WidgetType::data_type;
  DataT default_data =
      WidgetType::get_default_data(std::forward<Args>(args)...);

  int pui_flags = PF_PUI_CONTROL | PF_PUI_TOPIC;
  if (widget_inst->is_title_only()) {
    pui_flags = PF_PUI_TOPIC;
    ui_height = 0;
  }

  auto builder = ctx.add_arbitrary<DataT>(
      name, &default_data, param_flags,
      pui_flags, ui_width + 4, ui_height + 4);

  widget_registry::register_widget(builder.index, std::move(widget_inst));

  return builder;
}

} // namespace aetk::effect::ui

namespace aetk::effect {
template <typename T>
arb_param_builder<T> &arb_param_builder<T>::set_tooltip(std::string text) {
  ui::widget_registry::set_widget_tooltip(this->index, std::move(text));
  return *this;
}
} // namespace aetk::effect
