#pragma once

struct psychedelia_render_params {
  float time = 0.0f;
  float intensity = 0.0f;
  float effect_mix = 1.0f;
  float breathing = 0.0f;
  float tracers = 0.0f;
  float spectrum = 0.0f;
  float color_separation = 0.0f;
  float halo = 0.0f;
  float fractal = 0.0f;
  float speed = 1.0f;
  float detail = 1.0f;
  int state = 2;
  int quality = 2;
  int allow_gpu = 1;
  int pattern_style = 1;
  float flower_density = 0.0f;
  float palette_blend = 1.0f;
  int history_frames = 0;
  float tracer_drift = 1.0f;
  float drift_spread = 50.0f;
  float tracer_solidity = 80.0f;
  float acuity = 30.0f;
  float color_enhancement = 40.0f;
  float magnification = 0.0f;
  float melting = 20.0f;
  float flowing = 40.0f;
  float morphing = 30.0f;
  float center_x = 0.0f;
  float center_y = 0.0f;
  float radius = 0.0f;
  float feather = 100.0f;
  float par = 1.0f;
  int origin_x = 0;
  int origin_y = 0;
  int src_origin_x = 0;
  int src_origin_y = 0;
  int full_width = 0;
  int full_height = 0;
  int history_origin_x[16] = {0};
  int history_origin_y[16] = {0};
  int respect_source_alpha = 1;
  int dynamic_flower_colors = 1;
};

enum class param_id : int {
  intensity = 1,
  visual_state,
  render_quality,
  use_gpu,
  breath_speed,
  // Focus / Local Warp
  center_point,
  radius,
  feather,
  // Enhancements
  visual_acuity,
  color_enhancement,
  magnification,
  // Drifting
  breathing,
  melting,
  flowing,
  morphing,
  fractal_pattern,
  detail_scale,
  // Perception
  tracers,
  spectrum_drift,
  color_separation,
  edge_halo,
  tracer_drift,
  drift_spread,
  tracer_solidity,
  // Level 4+ Geometry
  recursion_depth,
  geometry_quantization,
  effect_mix,
  pattern_style,
  flower_density,
  palette_blend,
  respect_source_alpha,
  dynamic_flower_colors,
};
