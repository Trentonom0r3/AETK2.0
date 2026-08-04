#pragma once

#include <aetk/core/error.hpp>
#include <aetk/core/suite.hpp>
#include <aetk/core/utility.hpp>
#include <string>


namespace aetk::aegp {

/**
 * @brief Scoped RAII guard for managing After Effects undo groups.
 *
 * @details Automatically starts an undo group in the constructor and ends it in
 * the destructor, ensuring that timeline actions are cleanly grouped even if
 * exceptions occur.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Automatically manages the balancing of
 * start/end undo group calls.
 *
 * @warning <b>Memory & Lifecycles:</b> Destructor swallows exceptions to
 * prevent propagation during unwinding.
 */
class scoped_undo_guard {
public:
  explicit scoped_undo_guard(const std::string &name) {
    aetk::core::utility::start_undo_group(name.c_str());
  }

  explicit scoped_undo_guard(const char *name) {
    aetk::core::utility::start_undo_group(name);
  }

  ~scoped_undo_guard() { aetk::core::utility::end_undo_group(); }

  // Move-only / non-copyable
  scoped_undo_guard(const scoped_undo_guard &) = delete;
  scoped_undo_guard &operator=(const scoped_undo_guard &) = delete;
  scoped_undo_guard(scoped_undo_guard &&) noexcept = default;
  scoped_undo_guard &operator=(scoped_undo_guard &&) noexcept = default;
};

/**
 * @brief Scoped RAII guard for silencing After Effects interactive error
 * dialogs.
 *
 * @details Silences errors during its scope, letting the plugin handle them
 * programmatically.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Wraps raw AEGP_StartQuietErrors and
 * AEGP_EndQuietErrors calls.
 */
class scoped_quiet_guard {
private:
  using utility_suite =
      aetk::core::suite<AEGP_UtilitySuite6,
                        aetk::core::fixed_string(kAEGPUtilitySuite),
                        kAEGPUtilitySuiteVersion6>;
  AEGP_ErrReportState m_state{};

public:
  scoped_quiet_guard() {

    utility_suite::call<&AEGP_UtilitySuite6::AEGP_StartQuietErrors>(&m_state);
  }

  ~scoped_quiet_guard() {

    utility_suite::call<&AEGP_UtilitySuite6::AEGP_EndQuietErrors>(FALSE,
                                                                  &m_state);
  }

  // Move-only / non-copyable
  scoped_quiet_guard(const scoped_quiet_guard &) = delete;
  scoped_quiet_guard &operator=(const scoped_quiet_guard &) = delete;
  scoped_quiet_guard(scoped_quiet_guard &&) noexcept = default;
  scoped_quiet_guard &operator=(scoped_quiet_guard &&) noexcept = default;
};

} // namespace aetk::aegp
