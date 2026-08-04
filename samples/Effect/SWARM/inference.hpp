#pragma once
#include "utils.hpp"
#include <optional>
#include <aetk/effect/gpu/suite.hpp>
// gpu_copy_to_host removed in favor of unified smart_world::to(device::cpu)

using namespace aetk::effect;

extern "C" void launch_preprocess_yolo_cuda(const void* src_ptr, int src_pitch, int src_w,
    int src_h, int format, float* dst_planar, int dst_w, int dst_h);
extern "C" void launch_preprocess_yolo_cuda_half(const void* src_ptr, int src_pitch, int src_w,
    int src_h, int format, unsigned short* dst_planar_half, int dst_w, int dst_h);

bool can_run_cuda() {
    bool cuda_available = false;
    int deviceCount = 0;
    cudaError_t error = cudaGetDeviceCount(&deviceCount);
    cuda_available = (error == cudaSuccess && deviceCount > 0);
    return cuda_available;
}

inline bool is_cuda_active_for_non_ai(const smart_render_context& ctx) {
    return ctx.is_gpu() && can_run_cuda();
}

inline bool is_cuda_active_for_yolo(const smart_render_context& ctx) {
    return false;
}

#ifdef AETK_ENABLE_CUDA
class vram_scratch_pool {
private:
    std::mutex m_mutex;
    void* m_ptr = nullptr;
    size_t m_capacity = 0;

public:
    ~vram_scratch_pool() {
        if (m_ptr) {
            cudaFree(m_ptr);
            m_ptr = nullptr;
        }
    }

    void* get(size_t required_bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (required_bytes > m_capacity) {
            if (m_ptr) {
                cudaFree(m_ptr);
            }
            m_capacity = std::max(required_bytes * 2, static_cast<size_t>(16 * 1024 * 1024));
            if (cudaMalloc(&m_ptr, m_capacity) != cudaSuccess) {
                m_ptr = nullptr;
                m_capacity = 0;
            }
        }
        return m_ptr;
    }
};

static thread_local vram_scratch_pool t_vram_scratch;
#endif

inline std::vector<GridCell> trace_contour(const std::vector<bool>& in_component, int dw, int dh, int start_x, int start_y) {
    std::vector<GridCell> contour;
    int cx = start_x;
    int cy = start_y;
    
    // Direction offsets clockwise:
    // 0: Up, 1: Up-Right, 2: Right, 3: Down-Right, 4: Down, 5: Down-Left, 6: Left, 7: Up-Left
    const int dx[] = { 0,  1, 1, 1, 0, -1, -1, -1 };
    const int dy[] = { -1, -1, 0, 1, 1,  1,  0, -1 };
    
    // Start searching from direction 6 (Left) since we scan top-to-bottom, left-to-right,
    // which guarantees that (start_x - 1, start_y) is background.
    int dir = 6; 
    
    contour.push_back({ start_x, start_y });
    
    int max_steps = std::min(1000, dw * dh);
    int step = 0;
    
    while (step < max_steps) {
        int next_cx = -1, next_cy = -1;
        int found_dir = -1;
        
        for (int i = 0; i < 8; ++i) {
            int check_dir = (dir + i) % 8;
            int nx = cx + dx[check_dir];
            int ny = cy + dy[check_dir];
            if (nx >= 0 && nx < dw && ny >= 0 && ny < dh && in_component[ny * dw + nx]) {
                next_cx = nx;
                next_cy = ny;
                found_dir = check_dir;
                break;
            }
        }
        
        if (found_dir == -1) {
            break;
        }
        
        if (next_cx == start_x && next_cy == start_y) {
            break;
        }
        
        contour.push_back({ next_cx, next_cy });
        cx = next_cx;
        cy = next_cy;
        dir = (found_dir + 5) % 8;
        step++;
    }
    return contour;
}

std::vector<Detection> run_silhouette_tracking(const smart_render_context& ctx,
    const aetk::effect::smart_world& src, int threshold, int detect_style,
    int max_detections, int spawning_mode = 0, float spawning_spacing = 16.0f) {
    std::vector<Detection> detections;
    int sw = src.width();
    int sh = src.height();

    int dw = 256;
    int dh = (sh * dw) / sw;
    if (dh < 1)
        dh = 1;

    std::vector<float> gray(dw * dh, 0.0f);
    bool cuda_available = is_cuda_active_for_non_ai(ctx);
    AETK_DEBUG("cuda_available for silhouette tracking: {}", cuda_available);
    if (cuda_available) {
        try {
            size_t gray_size = dw * dh * sizeof(float);
#ifdef AETK_ENABLE_CUDA
            void* t_gray_ptr = t_vram_scratch.get(gray_size);
#else
            void* t_gray_ptr = nullptr;
#endif

            const void* kernel_src_ptr = nullptr;
            int format = 0;

            if (src.is_gpu()) {
                // src pixels are already on the GPU — use directly, no copy
                kernel_src_ptr = src.gpu_data();
                format = 3; // GPU worlds are BGRA float
            }

            if (cuda_available && kernel_src_ptr && t_gray_ptr) {
                launch_silhouette_downscale_cuda(kernel_src_ptr, src.rowbytes(), sw, sh,
                    format, static_cast<float*>(t_gray_ptr), dw, dh);
              //  cudaDeviceSynchronize();

                if (cudaMemcpy(
                        gray.data(), t_gray_ptr, gray_size, cudaMemcpyDeviceToHost)
                    != cudaSuccess) {
                    cuda_available = false;
                }
            }
        } catch (...) {
            cuda_available = false;
        }
    }

    if (!cuda_available) {
        // If src is GPU-backed but CUDA path failed, convert to CPU first
        std::optional<smart_world> cpu_copy;
        if (src.is_gpu())
            cpu_copy = src.to(device_kind::cpu);
        const auto& cpu_src = cpu_copy ? *cpu_copy : src;

        std::vector<int> x_map(dw);
        for (int x = 0; x < dw; x++) {
            x_map[x] = (x * sw) / dw;
        }

        visit_pixel_format<pixel_range::tkuint8>(cpu_src.pixel_format(),
            cpu_src.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
                using ChannelT = typename pixel_accessor<PixelT, IsBGRA,
                    pixel_range::tkuint8>::channel_type;
                auto view = cpu_src.tensor_view<ChannelT>();

                ctx.iterate_generic(dh, [&](A_long thread_idx, A_long y, A_long count) {
                    int src_y = (y * sh) / dh;
                    float* dst_row = gray.data() + y * dw;
                    for (int x = 0; x < dw; x++) {
                        int src_x = x_map[x];
                        const PixelT* px
                            = reinterpret_cast<const PixelT*>(&view(src_y, src_x, 0));
                        aetk::core::color<pixel_range::tkuint8> c
                            = pixel_accessor<PixelT, IsBGRA, pixel_range::tkuint8>::read(
                                px);
                        dst_row[x] = static_cast<float>(
                            0.299 * c.red + 0.587 * c.green + 0.114 * c.blue);
                    }
                });
            });
    }

    float sum_gray = 0.0f;
    for (float val : gray) {
        sum_gray += val;
    }
    float mean_gray = sum_gray / (dw * dh);

    std::vector<uint8_t> visited(dw * dh, 0);
    std::vector<bool> in_component_scratch(dw * dh, false);

    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            if (visited[y * dw + x])
                continue;

            float val = gray[y * dw + x];
            bool is_target = false;
            if (detect_style == 0) { // Dark Objects
                is_target = val < threshold;
            } else if (detect_style == 1) { // Bright Objects
                is_target = val > threshold;
            } else { // Auto Detect
                if (mean_gray > 127.0f) {
                    is_target
                        = val < threshold; // bright background -> track dark objects
                } else {
                    is_target
                        = val > threshold; // dark background -> track bright objects
                }
            }

            if (!is_target)
                continue;

            std::vector<GridCell> queue;
            queue.reserve(4096);
            queue.push_back({ x, y });
            visited[y * dw + x] = 1;

            int min_gx = x, max_gx = x;
            int min_gy = y, max_gy = y;
            long long sum_gx = 0, sum_gy = 0;
            int count = 0;

            size_t head = 0;
            while (head < queue.size()) {
                GridCell curr = queue[head++];
                count++;
                sum_gx += curr.x;
                sum_gy += curr.y;

                min_gx = std::min(min_gx, curr.x);
                max_gx = std::max(max_gx, curr.x);
                min_gy = std::min(min_gy, curr.y);
                max_gy = std::max(max_gy, curr.y);

                int dx[] = { -1, 1, 0, 0 };
                int dy[] = { 0, 0, -1, 1 };
                for (int i = 0; i < 4; i++) {
                    int nx = curr.x + dx[i];
                    int ny = curr.y + dy[i];
                    if (nx >= 0 && nx < dw && ny >= 0 && ny < dh) {
                        int nidx = ny * dw + nx;
                        if (!visited[nidx]) {
                            float nval = gray[nidx];
                            bool nis_target = false;
                            if (detect_style == 0) {
                                nis_target = nval < threshold;
                            } else {
                                nis_target = nval > threshold;
                            }

                            if (nis_target) {
                                visited[nidx] = 1;
                                queue.push_back({ nx, ny });
                            }
                        }
                    }
                }
            }

            if (count >= 16) {
                if (spawning_mode == 0) { // Centroid
                    Detection det { };
                    det.xmin = (float)min_gx / dw;
                    det.xmax = (float)max_gx / dw;
                    det.ymin = (float)min_gy / dh;
                    det.ymax = (float)max_gy / dh;
                    det.cx = (float)sum_gx / count / dw;
                    det.cy = (float)sum_gy / count / dh;
                    det.confidence = 1.0f;
                    det.class_name = "TARGET";
                    detections.push_back(det);
                } else if (spawning_mode == 1) { // Grid Fill
                    for (const auto& cell : queue) {
                        in_component_scratch[cell.y * dw + cell.x] = true;
                    }
                    int spacing = (int)std::clamp(spawning_spacing, 4.0f, 64.0f);
                    bool first = true;
                    for (int gy = min_gy; gy <= max_gy; gy += spacing) {
                        for (int gx = min_gx; gx <= max_gx; gx += spacing) {
                            if (in_component_scratch[gy * dw + gx]) {
                                Detection det { };
                                det.xmin = (float)min_gx / dw;
                                det.xmax = (float)max_gx / dw;
                                det.ymin = (float)min_gy / dh;
                                det.ymax = (float)max_gy / dh;
                                det.cx = (float)gx / dw;
                                det.cy = (float)gy / dh;
                                det.confidence = 1.0f;
                                det.class_name = "TARGET";
                                detections.push_back(det);
                                if ((int)detections.size() >= max_detections) {
                                    for (const auto& cell : queue) {
                                        in_component_scratch[cell.y * dw + cell.x] = false;
                                    }
                                    return detections;
                                }
                            }
                        }
                    }
                    for (const auto& cell : queue) {
                        in_component_scratch[cell.y * dw + cell.x] = false;
                    }
                } else if (spawning_mode == 2) { // Contour Outline
                    for (const auto& cell : queue) {
                        in_component_scratch[cell.y * dw + cell.x] = true;
                    }
                    std::vector<GridCell> contour = trace_contour(in_component_scratch, dw, dh, queue[0].x, queue[0].y);
                    int spacing = (int)std::clamp(spawning_spacing, 4.0f, 64.0f);
                    bool first = true;
                    for (size_t i = 0; i < contour.size(); i += spacing) {
                        int gx = contour[i].x;
                        int gy = contour[i].y;
                        Detection det { };
                        det.xmin = (float)min_gx / dw;
                        det.xmax = (float)max_gx / dw;
                        det.ymin = (float)min_gy / dh;
                        det.ymax = (float)max_gy / dh;
                        det.cx = (float)gx / dw;
                        det.cy = (float)gy / dh;
                        det.confidence = 1.0f;
                        det.class_name = "TARGET";
                        detections.push_back(det);
                        if ((int)detections.size() >= max_detections) {
                            for (const auto& cell : queue) {
                                in_component_scratch[cell.y * dw + cell.x] = false;
                            }
                            return detections;
                        }
                    }
                    for (const auto& cell : queue) {
                        in_component_scratch[cell.y * dw + cell.x] = false;
                    }
                }

                if ((int)detections.size() >= max_detections) {
                    return detections;
                }
            }
        }
    }

    return detections;
}

std::vector<Detection> run_hsv_keyer_tracking(const smart_render_context& ctx,
    const aetk::effect::smart_world& src,
    const aetk::core::color<pixel_range::tkuint8>& key_color, float h_tol, float s_tol,
    float v_tol, int max_detections, int spawning_mode = 0, float spawning_spacing = 16.0f) {
    std::vector<Detection> detections;
    int sw = src.width();
    int sh = src.height();

    int dw = 256;
    int dh = (sh * dw) / sw;
    if (dh < 1)
        dh = 1;

    float target_h, target_s, target_v;
    key_color.to_hsv(target_h, target_s, target_v);

    std::vector<uint8_t> is_target_pixel(dw * dh, 0);
    bool cuda_available = is_cuda_active_for_non_ai(ctx);
    AETK_DEBUG("cuda_available for hsv keyer: {}", cuda_available);
    if (cuda_available) {
        try {
            size_t mask_size = dw * dh * sizeof(unsigned char);
#ifdef AETK_ENABLE_CUDA
            void* t_mask_ptr = t_vram_scratch.get(mask_size);
#else
            void* t_mask_ptr = nullptr;
#endif

            const void* kernel_src_ptr = nullptr;
            int format = 0;

            if (src.is_gpu()) {
                // src pixels are already on the GPU — use directly, no copy
                kernel_src_ptr = src.gpu_data();
                format = 3; // GPU worlds are BGRA float
            }

            if (cuda_available && kernel_src_ptr && t_mask_ptr) {
                launch_hsv_threshold_cuda(kernel_src_ptr, src.rowbytes(), sw, sh, format,
                    static_cast<unsigned char*>(t_mask_ptr), dw, dh, target_h, target_s, target_v, h_tol, s_tol,
                    v_tol);
               // cudaDeviceSynchronize();

                if (cudaMemcpy(is_target_pixel.data(), t_mask_ptr, mask_size,
                        cudaMemcpyDeviceToHost)
                    != cudaSuccess) {
                    cuda_available = false;
                }
            }
        } catch (...) {
            cuda_available = false;
        }
    }

    if (!cuda_available) {
        // If src is GPU-backed but CUDA path failed, convert to CPU first
        std::optional<smart_world> cpu_copy;
        if (src.is_gpu())
            cpu_copy = src.to(device_kind::cpu);
        const auto& cpu_src = cpu_copy ? *cpu_copy : src;

        const unsigned char* base_ptr
            = reinterpret_cast<const unsigned char*>(cpu_src.ptr()->data);
        int rowbytes = cpu_src.rowbytes();

        std::vector<int> x_map(dw);
        for (int x = 0; x < dw; x++) {
            x_map[x] = (x * sw) / dw;
        }

        auto hsv_check = [&](float r, float g, float b, uint8_t& out_pixel) {
            float h, s, v;
            float min_val = std::min({ r, g, b });
            float max_val = std::max({ r, g, b });
            float delta = max_val - min_val;

            v = max_val;
            s = (max_val > 0.0f) ? (delta / max_val * 255.0f) : 0.0f;

            if (delta > 0.0f) {
                if (max_val == r) {
                    h = (g - b) / delta;
                } else if (max_val == g) {
                    h = 2.0f + (b - r) / delta;
                } else {
                    h = 4.0f + (r - g) / delta;
                }
                h *= 60.0f;
                if (h < 0.0f)
                    h += 360.0f;
                h /= 2.0f; // H range [0, 180]
            } else {
                h = 0.0f;
            }

            float diff_h = std::abs(h - target_h);
            if (diff_h > 90.0f)
                diff_h = 180.0f - diff_h;

            if (diff_h <= h_tol) {
                bool s_match = (target_s >= 128.0f) ? (s >= target_s - s_tol)
                                                    : (std::abs(s - target_s) <= s_tol);
                bool v_match = (target_v >= 128.0f) ? (v >= target_v - v_tol)
                                                    : (std::abs(v - target_v) <= v_tol);
                if (s_match && v_match) {
                    if (s >= 20.0f && v >= 20.0f) {
                        out_pixel = 1;
                    }
                }
            }
        };

        visit_pixel_format<pixel_range::tkuint8>(cpu_src.pixel_format(),
            cpu_src.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
                using ChannelT = typename pixel_accessor<PixelT, IsBGRA,
                    pixel_range::tkuint8>::channel_type;
                auto view = cpu_src.tensor_view<ChannelT>();

                ctx.iterate_generic(dh, [&](A_long thread_idx, A_long y, A_long count) {
                    int src_y = (y * sh) / dh;
                    uint8_t* dst_row = is_target_pixel.data() + y * dw;
                    for (int x = 0; x < dw; x++) {
                        int src_x = x_map[x];
                        const PixelT* px
                            = reinterpret_cast<const PixelT*>(&view(src_y, src_x, 0));
                        aetk::core::color<pixel_range::tkuint8> c
                            = pixel_accessor<PixelT, IsBGRA, pixel_range::tkuint8>::read(
                                px);
                        hsv_check(
                            (float)c.red, (float)c.green, (float)c.blue, dst_row[x]);
                    }
                });
            });
    }

    std::vector<uint8_t> visited(dw * dh, 0);
    std::vector<bool> in_component_scratch(dw * dh, false);

    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            if (visited[y * dw + x])
                continue;
            if (!is_target_pixel[y * dw + x])
                continue;

            std::vector<GridCell> queue;
            queue.reserve(4096);
            queue.push_back({ x, y });
            visited[y * dw + x] = 1;

            int min_gx = x, max_gx = x;
            int min_gy = y, max_gy = y;
            long long sum_gx = 0, sum_gy = 0;
            int count = 0;

            size_t head = 0;
            while (head < queue.size()) {
                GridCell curr = queue[head++];
                count++;
                sum_gx += curr.x;
                sum_gy += curr.y;

                min_gx = std::min(min_gx, curr.x);
                max_gx = std::max(max_gx, curr.x);
                min_gy = std::min(min_gy, curr.y);
                max_gy = std::max(max_gy, curr.y);

                int dx[] = { -1, 1, 0, 0 };
                int dy[] = { 0, 0, -1, 1 };
                for (int i = 0; i < 4; i++) {
                    int nx = curr.x + dx[i];
                    int ny = curr.y + dy[i];
                    if (nx >= 0 && nx < dw && ny >= 0 && ny < dh) {
                        int nidx = ny * dw + nx;
                        if (!visited[nidx] && is_target_pixel[nidx]) {
                            visited[nidx] = 1;
                            queue.push_back({ nx, ny });
                        }
                    }
                }
            }

            if (count >= 16) {
                if (spawning_mode == 0) { // Centroid
                    Detection det { };
                    det.xmin = (float)min_gx / dw;
                    det.xmax = (float)max_gx / dw;
                    det.ymin = (float)min_gy / dh;
                    det.ymax = (float)max_gy / dh;
                    det.cx = (float)sum_gx / count / dw;
                    det.cy = (float)sum_gy / count / dh;
                    det.confidence = 1.0f;
                    det.class_name = "KEYER";
                    detections.push_back(det);
                } else if (spawning_mode == 1) { // Grid Fill
                    for (const auto& cell : queue) {
                        in_component_scratch[cell.y * dw + cell.x] = true;
                    }
                    int spacing = (int)std::clamp(spawning_spacing, 4.0f, 64.0f);
                    bool first = true;
                    for (int gy = min_gy; gy <= max_gy; gy += spacing) {
                        for (int gx = min_gx; gx <= max_gx; gx += spacing) {
                            if (in_component_scratch[gy * dw + gx]) {
                                Detection det { };
                                det.xmin = (float)min_gx / dw;
                                det.xmax = (float)max_gx / dw;
                                det.ymin = (float)min_gy / dh;
                                det.ymax = (float)max_gy / dh;
                                det.cx = (float)gx / dw;
                                det.cy = (float)gy / dh;
                                det.confidence = 1.0f;
                                det.class_name = "KEYER";
                                detections.push_back(det);
                                if ((int)detections.size() >= max_detections) {
                                    for (const auto& cell : queue) {
                                        in_component_scratch[cell.y * dw + cell.x] = false;
                                    }
                                    return detections;
                                }
                            }
                        }
                    }
                    for (const auto& cell : queue) {
                        in_component_scratch[cell.y * dw + cell.x] = false;
                    }
                } else if (spawning_mode == 2) { // Contour Outline
                    for (const auto& cell : queue) {
                        in_component_scratch[cell.y * dw + cell.x] = true;
                    }
                    std::vector<GridCell> contour = trace_contour(in_component_scratch, dw, dh, queue[0].x, queue[0].y);
                    int spacing = (int)std::clamp(spawning_spacing, 4.0f, 64.0f);
                    bool first = true;
                    for (size_t i = 0; i < contour.size(); i += spacing) {
                        int gx = contour[i].x;
                        int gy = contour[i].y;
                        Detection det { };
                        det.xmin = (float)min_gx / dw;
                        det.xmax = (float)max_gx / dw;
                        det.ymin = (float)min_gy / dh;
                        det.ymax = (float)max_gy / dh;
                        det.cx = (float)gx / dw;
                        det.cy = (float)gy / dh;
                        det.confidence = 1.0f;
                        det.class_name = "KEYER";
                        detections.push_back(det);
                        if ((int)detections.size() >= max_detections) {
                            for (const auto& cell : queue) {
                                in_component_scratch[cell.y * dw + cell.x] = false;
                            }
                            return detections;
                        }
                    }
                    for (const auto& cell : queue) {
                        in_component_scratch[cell.y * dw + cell.x] = false;
                    }
                }

                if ((int)detections.size() >= max_detections) {
                    return detections;
                }
            }
        }
    }

    return detections;
}

float calculate_iou(const Detection& a, const Detection& b) {
    float x1 = std::max(a.xmin, b.xmin);
    float y1 = std::max(a.ymin, b.ymin);
    float x2 = std::min(a.xmax, b.xmax);
    float y2 = std::min(a.ymax, b.ymax);

    if (x1 >= x2 || y1 >= y2)
        return 0.0f;

    float intersection = (x2 - x1) * (y2 - y1);
    float area_a = (a.xmax - a.xmin) * (a.ymax - a.ymin);
    float area_b = (b.xmax - b.xmin) * (b.ymax - b.ymin);
    float union_area = area_a + area_b - intersection;

    return (union_area > 0.0f) ? (intersection / union_area) : 0.0f;
}
std::vector<float> preprocess_yolo(const smart_render_context& ctx,
    const aetk::effect::smart_world& src, int width, int height) {
    std::vector<float> tensor(width * height * 3, 0.0f);
    int sw = src.width();
    int sh = src.height();

    // Create fast plane pointers
    float* r_plane = tensor.data();
    float* g_plane = tensor.data() + (width * height);
    float* b_plane = tensor.data() + (2 * width * height);

    const unsigned char* base_ptr
        = reinterpret_cast<const unsigned char*>(src.ptr()->data);
    int rowbytes = src.rowbytes();

    // Precompute vertical mappings
    std::vector<int> y0_map(height);
    std::vector<int> y1_map(height);
    std::vector<float> ty_map(height);
    for (int y = 0; y < height; y++) {
        float src_yf = (float)y / height * sh;
        y0_map[y] = (int)std::floor(src_yf);
        y1_map[y] = std::min(y0_map[y] + 1, sh - 1);
        ty_map[y] = src_yf - y0_map[y];
    }

    // Precompute horizontal mappings
    std::vector<int> x0_map(width);
    std::vector<int> x1_map(width);
    std::vector<float> tx_map(width);
    for (int x = 0; x < width; x++) {
        float src_xf = (float)x / width * sw;
        x0_map[x] = (int)std::floor(src_xf);
        x1_map[x] = std::min(x0_map[x] + 1, sw - 1);
        tx_map[x] = src_xf - x0_map[x];
    }

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    visit_pixel_format(
        src.pixel_format(), src.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
            ctx.iterate_generic(height, [&](A_long thread_idx, A_long y, A_long count) {
                int y0 = y0_map[y];
                int y1 = y1_map[y];
                float ty = ty_map[y];

                const PixelT* row0
                    = reinterpret_cast<const PixelT*>(base_ptr + y0 * rowbytes);
                const PixelT* row1
                    = reinterpret_cast<const PixelT*>(base_ptr + y1 * rowbytes);

                float* r_row = r_plane + y * width;
                float* g_row = g_plane + y * width;
                float* b_row = b_plane + y * width;

                for (int x = 0; x < width; x++) {
                    int x0 = x0_map[x];
                    int x1 = x1_map[x];
                    float tx = tx_map[x];

                    aetk::core::color<> c00
                        = pixel_accessor<PixelT, IsBGRA>::read(row0 + x0);
                    aetk::core::color<> c10
                        = pixel_accessor<PixelT, IsBGRA>::read(row0 + x1);
                    aetk::core::color<> c01
                        = pixel_accessor<PixelT, IsBGRA>::read(row1 + x0);
                    aetk::core::color<> c11
                        = pixel_accessor<PixelT, IsBGRA>::read(row1 + x1);

                    r_row[x] = lerp(lerp((float)c00.red, (float)c10.red, tx),
                        lerp((float)c01.red, (float)c11.red, tx), ty);
                    g_row[x] = lerp(lerp((float)c00.green, (float)c10.green, tx),
                        lerp((float)c01.green, (float)c11.green, tx), ty);
                    b_row[x] = lerp(lerp((float)c00.blue, (float)c10.blue, tx),
                        lerp((float)c01.blue, (float)c11.blue, tx), ty);
                }
            });
        });

    return tensor;
}

std::vector<Detection> run_yolo_inference(const smart_render_context& ctx,
    const aetk::effect::smart_world& src, float conf_threshold, int max_detections,
    int ai_model_size = 0) {
    std::vector<Detection> detections;
    int sw = src.width();
    int sh = src.height();

    std::string preferred_model = "yolox_s.onnx";
    if (ai_model_size == 1) {
        preferred_model = "yolox_s.onnx";
    } else if (ai_model_size == 2) {
        preferred_model = "yolox_m.onnx";
    } else if (ai_model_size == 3) {
        preferred_model = "yolox_l.onnx";
    }

    std::string dir = GetPluginDirectoryA();
    std::string target_path = dir + "\\models\\" + preferred_model;
    if (!file_exists(target_path)) {
        target_path = dir + "\\" + preferred_model;
    }
    // Fallbacks if target model isn't installed
    if (!file_exists(target_path)) {
        target_path = dir + "\\models\\yolox_m.onnx";
    }
    if (!file_exists(target_path)) {
        target_path = dir + "\\yolox_m.onnx";
    }
    if (!file_exists(target_path)) {
        target_path = dir + "\\models\\yolox_s.onnx";
    }
    if (!file_exists(target_path)) {
        target_path = dir + "\\yolox_s.onnx";
    }

    static std::string s_current_loaded_path;
    static std::mutex s_model_load_mutex;
    std::lock_guard<std::mutex> model_lock(s_model_load_mutex);

    bool is_loaded = OrtEngine::IsModelLoaded("yolo");
    if (is_loaded && s_current_loaded_path != target_path) {
        OrtEngine::UnloadModel("yolo");
        is_loaded = false;
    }

    std::string device_str = "cpu";
    if (!is_loaded && file_exists(target_path)) {
        if (OrtEngine::LoadModel("yolo", target_path, "dml")
            && OrtEngine::IsModelLoaded("yolo", "dml")) {
            device_str = "dml";
            s_current_loaded_path = target_path;
            AETK_DEBUG("DirectML loaded model successfully: {}", target_path);
        } else if (OrtEngine::LoadModel("yolo", target_path, "cpu")
            && OrtEngine::IsModelLoaded("yolo", "cpu")) {
            device_str = "cpu";
            s_current_loaded_path = target_path;
            AETK_DEBUG("CPU loaded model successfully: {}", target_path);
        }
    } else if (is_loaded) {
        device_str = OrtEngine::IsModelLoaded("yolo", "dml") ? "dml" : "cpu";
    }

    // Query actual input dimensions of the loaded model (fast cached query)
    int model_w = 640;
    int model_h = 640;
    if (!OrtEngine::GetCachedModelShape("yolo", model_w, model_h)) {
        auto input_info = OrtEngine::GetInputInfo("yolo");
        if (!input_info.empty() && input_info[0].shape.size() == 4) {
            if (input_info[0].shape[2] > 0) model_h = (int)input_info[0].shape[2];
            if (input_info[0].shape[3] > 0) model_w = (int)input_info[0].shape[3];
        }
    }

    std::vector<float> input_data;
    std::vector<OrtEngine::IOTensor> outputs;
    bool processed_on_gpu = false;

    if (src.is_gpu() && device_str == "cuda") {
        try {
            auto input_info = OrtEngine::GetInputInfo("yolo");
            bool is_fp16_input = (!input_info.empty() && input_info[0].dtype == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16);
            size_t elem_size = is_fp16_input ? sizeof(uint16_t) : sizeof(float);
            size_t size_in_bytes = model_w * model_h * 3 * elem_size;
#ifdef AETK_ENABLE_CUDA
            void* t_input_ptr = t_vram_scratch.get(size_in_bytes);
#else
            void* t_input_ptr = nullptr;
#endif

            int format = 3; // BGRA float format
            void* src_ptr = src.gpu_data();

            if (t_input_ptr) {
                if (is_fp16_input) {
                    launch_preprocess_yolo_cuda_half(src_ptr, src.rowbytes(), sw, sh, format,
                        static_cast<unsigned short*>(t_input_ptr), model_w, model_h);
                } else {
                    launch_preprocess_yolo_cuda(src_ptr, src.rowbytes(), sw, sh, format,
                        static_cast<float*>(t_input_ptr), model_w, model_h);
                }

                if (device_str == "cuda") {
                    outputs = OrtEngine::RunCUDAInference("yolo", "images", t_input_ptr,
                        size_in_bytes, { 1, 3, (size_t)model_h, (size_t)model_w });
                    if (!outputs.empty()) {
                        processed_on_gpu = true;
                    }
                } else {
                    input_data.resize(model_w * model_h * 3);
                    if (cudaMemcpy(input_data.data(), t_input_ptr, size_in_bytes,
                            cudaMemcpyDeviceToHost)
                        == cudaSuccess) {
                        processed_on_gpu = true;
                    }
                }
            }
        } catch (...) {
            processed_on_gpu = false;
        }
    }

    if (!processed_on_gpu) {
        if (src.is_gpu()) {
            auto cpu_src = src.to(device_kind::cpu);
            input_data = preprocess_yolo(ctx, cpu_src, model_w, model_h);
        } else {
            input_data = preprocess_yolo(ctx, src, model_w, model_h);
        }

        OrtEngine::IOTensor input_tensor { };
        input_tensor.name = "images";
        input_tensor.shape = { 1, 3, (size_t)model_h, (size_t)model_w };
        input_tensor.data = std::move(input_data);

        outputs = OrtEngine::Run("yolo", { input_tensor });
    }

    if (outputs.empty())
        return { };

    auto& out = outputs[0];
    if (out.shape.size() < 3) {
        return { };
    }

    const float* data = out.data.data();
    std::vector<Detection> candidates;
    candidates.reserve(128);

    // Support both YOLOX [1, N, 85] and YOLOv8 [1, 84/604, N] tensor layouts
    if (out.shape[2] <= 604 && out.shape[1] > out.shape[2]) {
        // YOLOX Layout: [1, num_predictions, 5 + num_classes]
        int num_predictions = (int)out.shape[1];
        int num_classes = (int)out.shape[2] - 5;
        int num_elements_per_pred = (int)out.shape[2];

        for (int i = 0; i < num_predictions; i++) {
            const float* row = data + i * num_elements_per_pred;
            float obj_conf = row[4];
            if (obj_conf < 0.05f) continue;

            float max_score = 0.0f;
            int best_class = -1;
            for (int c = 0; c < num_classes; c++) {
                float score = obj_conf * row[5 + c];
                if (score > max_score) {
                    max_score = score;
                    best_class = c;
                }
            }

            if (max_score >= conf_threshold) {
                float cx = row[0];
                float cy = row[1];
                float w  = row[2];
                float h  = row[3];

                Detection det { };
                det.xmin = std::max(0.0f, (cx - w * 0.5f) / model_w);
                det.xmax = std::min(1.0f, (cx + w * 0.5f) / model_w);
                det.ymin = std::max(0.0f, (cy - h * 0.5f) / model_h);
                det.ymax = std::min(1.0f, (cy + h * 0.5f) / model_h);
                det.cx = cx / model_w;
                det.cy = cy / model_h;
                det.confidence = max_score;
                det.class_name = get_class_name(best_class, num_classes);
                candidates.push_back(det);
            }
        }
    } else {
        // YOLOv8 Layout: [1, 4 + num_classes, num_predictions]
        int num_classes = (int)out.shape[1] - 4;
        int num_predictions = (int)out.shape[2];

        for (int i = 0; i < num_predictions; i++) {
            float max_score = 0.0f;
            int best_class = -1;
            for (int c = 0; c < num_classes; c++) {
                float score = data[(4 + c) * num_predictions + i];
                if (score > max_score) {
                    max_score = score;
                    best_class = c;
                }
            }

            if (max_score >= conf_threshold) {
                float cx = data[0 * num_predictions + i];
                float cy = data[1 * num_predictions + i];
                float w  = data[2 * num_predictions + i];
                float h  = data[3 * num_predictions + i];

                Detection det { };
                det.xmin = std::max(0.0f, (cx - w * 0.5f) / model_w);
                det.xmax = std::min(1.0f, (cx + w * 0.5f) / model_w);
                det.ymin = std::max(0.0f, (cy - h * 0.5f) / model_h);
                det.ymax = std::min(1.0f, (cy + h * 0.5f) / model_h);
                det.cx = cx / model_w;
                det.cy = cy / model_h;
                det.confidence = max_score;
                det.class_name = get_class_name(best_class, num_classes);
                candidates.push_back(det);
            }
        }
    }

    std::sort(
        candidates.begin(), candidates.end(), [](const Detection& a, const Detection& b) {
            return a.confidence > b.confidence;
        });

    std::vector<bool> suppressed(candidates.size(), false);
    for (size_t i = 0; i < candidates.size(); i++) {
        if (suppressed[i])
            continue;

        detections.push_back(candidates[i]);
        if ((int)detections.size() >= max_detections)
            break;

        for (size_t j = i + 1; j < candidates.size(); j++) {
            if (suppressed[j])
                continue;

            float iou = calculate_iou(candidates[i], candidates[j]);
            if (iou >= 0.45f) {
                suppressed[j] = true;
            }
        }
    }

    return detections;
}

std::vector<Edge> compute_mst(const std::vector<Detection>& nodes, float par = 1.0f) {
    std::vector<Edge> mst;
    int n = (int)nodes.size();
    if (n < 2)
        return mst;

    std::vector<Edge> edges;
    edges.reserve((n * (n - 1)) / 2);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            float dx = (nodes[i].cx - nodes[j].cx) * par;
            float dy = nodes[i].cy - nodes[j].cy;
            // SKIP SQRT HERE: Just store squared distance
            float dist_sq = (dx * dx + dy * dy);
            edges.push_back({ i, j, dist_sq });
        }
    }

    std::sort(edges.begin(), edges.end(),
        [](const Edge& a, const Edge& b) { return a.weight < b.weight; });

    aetk::core::math::DSU dsu(n);
    int count = 0;
    for (auto e : edges) { // Copy by value to modify weight
        if (dsu.unite(e.u, e.v)) {
            // COMPUTE SQRT ONLY ON WINNING EDGES
            e.weight = std::sqrt(e.weight);
            mst.push_back(e);
            count++;
            if (count == n - 1)
                break;
        }
    }
    return mst;
}

struct Triangle {
    int p1, p2, p3;
    bool bad = false;
};

inline bool circumcircle_contains(const Detection& p1, const Detection& p2, const Detection& p3, const Detection& p, float par) {
    float ax = p1.cx * par; float ay = p1.cy;
    float bx = p2.cx * par; float by = p2.cy;
    float cx = p3.cx * par; float cy = p3.cy;
    float px = p.cx * par;  float py = p.cy;

    float d = 2.0f * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(d) < 1e-9f) return false;

    float ux = ((ax*ax + ay*ay) * (by - cy) + (bx*bx + by*by) * (cy - ay) + (cx*cx + cy*cy) * (ay - by)) / d;
    float uy = ((ax*ax + ay*ay) * (cx - bx) + (bx*bx + by*by) * (ax - cx) + (cx*cx + cy*cy) * (bx - ax)) / d;

    float r2 = (ax - ux) * (ax - ux) + (ay - uy) * (ay - uy);
    float dist2 = (px - ux) * (px - ux) + (py - uy) * (py - uy);

    return dist2 < r2 + 1e-4f;
}

inline std::vector<Edge> compute_delaunay(const std::vector<Detection>& nodes, float par = 1.0f) {
    std::vector<Edge> edges;
    int n = (int)nodes.size();
    if (n < 3) {
        if (n == 2) {
            edges.push_back({0, 1, std::hypot((nodes[0].cx - nodes[1].cx) * par, nodes[0].cy - nodes[1].cy)});
        }
        return edges;
    }

    float min_x = nodes[0].cx, max_x = nodes[0].cx;
    float min_y = nodes[0].cy, max_y = nodes[0].cy;
    for (const auto& node : nodes) {
        min_x = std::min(min_x, node.cx);
        max_x = std::max(max_x, node.cx);
        min_y = std::min(min_y, node.cy);
        max_y = std::max(max_y, node.cy);
    }

    float dx = max_x - min_x;
    float dy = max_y - min_y;
    float delta_max = std::max(dx, dy);
    if (delta_max < 1e-6f) delta_max = 1.0f;
    float mid_x = (min_x + max_x) * 0.5f;
    float mid_y = (min_y + max_y) * 0.5f;

    Detection st1, st2, st3;
    st1.cx = mid_x - 20.0f * delta_max - 1.0f; st1.cy = mid_y - 20.0f * delta_max - 1.0f;
    st2.cx = mid_x;                            st2.cy = mid_y + 20.0f * delta_max + 1.0f;
    st3.cx = mid_x + 20.0f * delta_max + 1.0f; st3.cy = mid_y - 20.0f * delta_max - 1.0f;

    std::vector<Detection> temp_nodes = nodes;
    temp_nodes.push_back(st1); // index n
    temp_nodes.push_back(st2); // index n+1
    temp_nodes.push_back(st3); // index n+2

    std::vector<Triangle> triangles;
    triangles.reserve(n * 3);
    triangles.push_back({n, n + 1, n + 2});

    struct SimpleEdge {
        int u, v;
        bool operator==(const SimpleEdge& o) const {
            return (u == o.u && v == o.v) || (u == o.v && v == o.u);
        }
    };

    std::vector<Triangle> bad_triangles;
    std::vector<SimpleEdge> polygon;
    bad_triangles.reserve(64);
    polygon.reserve(64);

    for (int i = 0; i < n; ++i) {
        bad_triangles.clear();
        polygon.clear();

        for (auto& t : triangles) {
            if (circumcircle_contains(temp_nodes[t.p1], temp_nodes[t.p2], temp_nodes[t.p3], temp_nodes[i], par)) {
                t.bad = true;
                bad_triangles.push_back(t);
            }
        }

        for (const auto& t : bad_triangles) {
            SimpleEdge es[3] = { {t.p1, t.p2}, {t.p2, t.p3}, {t.p3, t.p1} };
            for (int k = 0; k < 3; ++k) {
                bool shared = false;
                for (const auto& other_t : bad_triangles) {
                    if (&other_t == &t) continue;
                    SimpleEdge oes[3] = { {other_t.p1, other_t.p2}, {other_t.p2, other_t.p3}, {other_t.p3, other_t.p1} };
                    for (int m = 0; m < 3; ++m) {
                        if (es[k] == oes[m]) {
                            shared = true;
                            break;
                        }
                    }
                    if (shared) break;
                }
                if (!shared) {
                    polygon.push_back(es[k]);
                }
            }
        }

        triangles.erase(std::remove_if(triangles.begin(), triangles.end(), [](const Triangle& t) { return t.bad; }), triangles.end());

        for (const auto& edge : polygon) {
            triangles.push_back({edge.u, edge.v, i});
        }
    }

    struct EdgePairHash {
        size_t operator()(const std::pair<int, int>& p) const {
            return std::hash<int>{}(p.first) ^ std::hash<int>{}(p.second);
        }
    };
    std::unordered_set<std::pair<int, int>, EdgePairHash> unique_edges;
    unique_edges.reserve(triangles.size() * 3);

    for (const auto& t : triangles) {
        if (t.p1 < n && t.p2 < n) {
            unique_edges.insert({std::min(t.p1, t.p2), std::max(t.p1, t.p2)});
        }
        if (t.p2 < n && t.p3 < n) {
            unique_edges.insert({std::min(t.p2, t.p3), std::max(t.p2, t.p3)});
        }
        if (t.p3 < n && t.p1 < n) {
            unique_edges.insert({std::min(t.p3, t.p1), std::max(t.p3, t.p1)});
        }
    }

    edges.reserve(unique_edges.size());
    for (const auto& p : unique_edges) {
        float dx = (nodes[p.first].cx - nodes[p.second].cx) * par;
        float dy = nodes[p.first].cy - nodes[p.second].cy;
        edges.push_back({p.first, p.second, std::sqrt(dx * dx + dy * dy)});
    }

    return edges;
}

// ══════════════════════════════════════════════════════════════════════
//  Deterministic Cache Key Generation Helper
// ══════════════════════════════════════════════════════════════════════

inline uint32_t fnv1a_32(uint32_t hash, const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

AEGP_GUID compute_detection_guid(PF_ProgPtr effect_ref, A_long current_time,
    A_long time_step, A_u_long time_scale, int track_mode, int detect_style,
    int silhouette_threshold, const aetk::core::color<>& key_color, int h_tol, int s_tol,
    bool cuda_active, int spawning_mode = 0, float spawning_spacing = 16.0f,
    float tracking_min_area = 10.0f, int max_detections = 500, float max_bounding_area_pct = 50.0f,
    int ai_model_size = 0) {

    A_long snapped_time = current_time;
    if (time_step > 0) {
        snapped_time = ((current_time + time_step / 2) / time_step) * time_step;
    }

    uint32_t h1 = 2166136261u;
    uintptr_t ref_ptr = reinterpret_cast<uintptr_t>(effect_ref);
    h1 = fnv1a_32(h1, &ref_ptr, sizeof(ref_ptr));
    h1 = fnv1a_32(h1, &snapped_time, sizeof(snapped_time));
    h1 = fnv1a_32(h1, &time_scale, sizeof(time_scale));
    h1 = fnv1a_32(h1, &track_mode, sizeof(track_mode));
    int cuda_active_int = cuda_active ? 1 : 0;
    h1 = fnv1a_32(h1, &cuda_active_int, sizeof(cuda_active_int));
    h1 = fnv1a_32(h1, &spawning_mode, sizeof(spawning_mode));
    h1 = fnv1a_32(h1, &spawning_spacing, sizeof(spawning_spacing));

    uint32_t h2 = 2166136261u;
    h2 = fnv1a_32(h2, &tracking_min_area, sizeof(tracking_min_area));
    h2 = fnv1a_32(h2, &max_detections, sizeof(max_detections));
    h2 = fnv1a_32(h2, &max_bounding_area_pct, sizeof(max_bounding_area_pct));
    h2 = fnv1a_32(h2, &ai_model_size, sizeof(ai_model_size));

    if (track_mode == 0) { // Silhouette
        h2 = fnv1a_32(h2, &detect_style, sizeof(detect_style));
        h2 = fnv1a_32(h2, &silhouette_threshold, sizeof(silhouette_threshold));
    } else if (track_mode == 1) { // HSV
        h2 = fnv1a_32(h2, &key_color, sizeof(key_color));
        h2 = fnv1a_32(h2, &h_tol, sizeof(h_tol));
        h2 = fnv1a_32(h2, &s_tol, sizeof(s_tol));
    }

    AEGP_GUID guid { };
    guid.bytes[0] = static_cast<A_long>(h1);
    guid.bytes[1] = static_cast<A_long>(h2);
    guid.bytes[2] = 0;
    guid.bytes[3] = static_cast<A_long>(0x53574152); // Stable plugin identifier "SWAR"
    return guid;
}