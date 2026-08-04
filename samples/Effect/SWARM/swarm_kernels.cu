#include <cuda_runtime.h>
#include <device_launch_parameters.h>

struct GpuPixel8_ARGB {
    unsigned char alpha, red, green, blue;
};
struct GpuPixel16_ARGB {
    unsigned short alpha, red, green, blue;
};
struct GpuPixel32_ARGB {
    float alpha, red, green, blue;
};
struct GpuPixel32_BGRA {
    float blue, green, red, alpha;
};
struct GpuPixel8_BGRA {
    unsigned char blue, green, red, alpha;
};
struct GpuPixel16_BGRA {
    unsigned short blue, green, red, alpha;
};

struct DeviceColor {
    float red, green, blue;
};

__device__ __forceinline__ DeviceColor get_pixel_color(
    const unsigned char* src,
    int src_pitch,
    int src_w,
    int src_h,
    int format, // 0: ARGB 8-bit, 1: ARGB 16-bit, 2: ARGB float, 3: BGRA float, 4: BGRA 8-bit, 5: BGRA 16-bit
    int x,
    int y
) {
    DeviceColor c = {0.0f, 0.0f, 0.0f};
    if (x < 0 || x >= src_w || y < 0 || y >= src_h) return c;

    const unsigned char* row = src + y * src_pitch;

    if (format == 0) {
        const GpuPixel8_ARGB* p = reinterpret_cast<const GpuPixel8_ARGB*>(row) + x;
        c.red = p->red / 255.0f;
        c.green = p->green / 255.0f;
        c.blue = p->blue / 255.0f;
    } else if (format == 1) {
        const GpuPixel16_ARGB* p = reinterpret_cast<const GpuPixel16_ARGB*>(row) + x;
        c.red = p->red / 32768.0f;
        c.green = p->green / 32768.0f;
        c.blue = p->blue / 32768.0f;
    } else if (format == 2) {
        const GpuPixel32_ARGB* p = reinterpret_cast<const GpuPixel32_ARGB*>(row) + x;
        c.red = p->red;
        c.green = p->green;
        c.blue = p->blue;
    } else if (format == 3) {
        const GpuPixel32_BGRA* p = reinterpret_cast<const GpuPixel32_BGRA*>(row) + x;
        c.red = p->red;
        c.green = p->green;
        c.blue = p->blue;
    } else if (format == 4) {
        const GpuPixel8_BGRA* p = reinterpret_cast<const GpuPixel8_BGRA*>(row) + x;
        c.red = p->red / 255.0f;
        c.green = p->green / 255.0f;
        c.blue = p->blue / 255.0f;
    } else if (format == 5) {
        const GpuPixel16_BGRA* p = reinterpret_cast<const GpuPixel16_BGRA*>(row) + x;
        c.red = p->red / 32768.0f;
        c.green = p->green / 32768.0f;
        c.blue = p->blue / 32768.0f;
    }
    return c;
}

__device__ __forceinline__ void rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v) {
    float min_val = fminf(r, fminf(g, b));
    float max_val = fmaxf(r, fmaxf(g, b));
    float delta = max_val - min_val;

    v = max_val * 255.0f;
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
        if (h < 0.0f) h += 360.0f;
        h /= 2.0f; // H range [0, 180]
    } else {
        h = 0.0f;
    }
}

__global__ void silhouette_downscale_kernel(
    const unsigned char* src,
    int src_pitch,
    int src_w,
    int src_h,
    int format,
    float* dst_gray,
    int dst_w,
    int dst_h
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= dst_w || y >= dst_h) return;

    int src_x = (x * src_w) / dst_w;
    int src_y = (y * src_h) / dst_h;

    DeviceColor c = get_pixel_color(src, src_pitch, src_w, src_h, format, src_x, src_y);
    float gray = (0.299f * c.red + 0.587f * c.green + 0.114f * c.blue) * 255.0f;

    dst_gray[y * dst_w + x] = gray;
}

__global__ void hsv_threshold_kernel(
    const unsigned char* src,
    int src_pitch,
    int src_w,
    int src_h,
    int format,
    unsigned char* dst_mask,
    int dst_w,
    int dst_h,
    float target_h,
    float target_s,
    float target_v,
    float h_tol,
    float s_tol,
    float v_tol
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= dst_w || y >= dst_h) return;

    int src_x = (x * src_w) / dst_w;
    int src_y = (y * src_h) / dst_h;

    DeviceColor c = get_pixel_color(src, src_pitch, src_w, src_h, format, src_x, src_y);
    float h, s, v;
    rgb_to_hsv(c.red, c.green, c.blue, h, s, v);

    float diff_h = fabsf(h - target_h);
    if (diff_h > 90.0f) diff_h = 180.0f - diff_h;

    unsigned char result = 0;
    if (diff_h <= h_tol) {
        bool s_match = (target_s >= 128.0f) ? (s >= target_s - s_tol) : (fabsf(s - target_s) <= s_tol);
        bool v_match = (target_v >= 128.0f) ? (v >= target_v - v_tol) : (fabsf(v - target_v) <= v_tol);
        if (s_match && v_match) {
            if (s >= 20.0f && v >= 20.0f) {
                result = 1;
            }
        }
    }

    dst_mask[y * dst_w + x] = result;
}

extern "C" void launch_silhouette_downscale_cuda(
    const void* src_ptr,
    int src_pitch,
    int src_w,
    int src_h,
    int format,
    float* dst_gray,
    int dst_w,
    int dst_h
) {
    dim3 block(16, 16);
    dim3 grid((dst_w + block.x - 1) / block.x, (dst_h + block.y - 1) / block.y);

    silhouette_downscale_kernel<<<grid, block>>>(
        static_cast<const unsigned char*>(src_ptr),
        src_pitch, src_w, src_h, format,
        dst_gray, dst_w, dst_h
    );
}

extern "C" void launch_hsv_threshold_cuda(
    const void* src_ptr,
    int src_pitch,
    int src_w,
    int src_h,
    int format,
    unsigned char* dst_mask,
    int dst_w,
    int dst_h,
    float target_h,
    float target_s,
    float target_v,
    float h_tol,
    float s_tol,
    float v_tol
) {
    dim3 block(16, 16);
    dim3 grid((dst_w + block.x - 1) / block.x, (dst_h + block.y - 1) / block.y);

    hsv_threshold_kernel<<<grid, block>>>(
        static_cast<const unsigned char*>(src_ptr),
        src_pitch, src_w, src_h, format,
        dst_mask, dst_w, dst_h,
        target_h, target_s, target_v,
        h_tol, s_tol, v_tol
    );
}

__device__ __forceinline__ float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

__global__ void preprocess_yolo_kernel(
    const unsigned char* src,
    int src_pitch,
    int src_w,
    int src_h,
    int format,
    float* dst_planar,
    int dst_w,
    int dst_h
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= dst_w || y >= dst_h) return;

    float src_x = ((float)x + 0.5f) * src_w / dst_w - 0.5f;
    float src_y = ((float)y + 0.5f) * src_h / dst_h - 0.5f;

    int x0 = (int)floorf(src_x);
    int x1 = min(x0 + 1, src_w - 1);
    int y0 = (int)floorf(src_y);
    int y1 = min(y0 + 1, src_h - 1);
    x0 = max(0, x0);
    y0 = max(0, y0);

    float tx = src_x - x0;
    float ty = src_y - y0;

    DeviceColor c00 = get_pixel_color(src, src_pitch, src_w, src_h, format, x0, y0);
    DeviceColor c10 = get_pixel_color(src, src_pitch, src_w, src_h, format, x1, y0);
    DeviceColor c01 = get_pixel_color(src, src_pitch, src_w, src_h, format, x0, y1);
    DeviceColor c11 = get_pixel_color(src, src_pitch, src_w, src_h, format, x1, y1);

    float r = lerpf(lerpf(c00.red, c10.red, tx), lerpf(c01.red, c11.red, tx), ty);
    float g = lerpf(lerpf(c00.green, c10.green, tx), lerpf(c01.green, c11.green, tx), ty);
    float b = lerpf(lerpf(c00.blue, c10.blue, tx), lerpf(c01.blue, c11.blue, tx), ty);

    int plane_size = dst_w * dst_h;
    int idx = y * dst_w + x;
    
    // Planar layout: R plane first, then G, then B
    dst_planar[idx] = r;
    dst_planar[idx + plane_size] = g;
    dst_planar[idx + plane_size * 2] = b;
}

__global__ void preprocess_yolo_kernel_half(
    const unsigned char* src,
    int src_pitch,
    int src_w,
    int src_h,
    int format,
    unsigned short* dst_planar_half,
    int dst_w,
    int dst_h
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= dst_w || y >= dst_h) return;

    float src_x = ((float)x + 0.5f) * src_w / dst_w - 0.5f;
    float src_y = ((float)y + 0.5f) * src_h / dst_h - 0.5f;

    int x0 = (int)floorf(src_x);
    int x1 = min(x0 + 1, src_w - 1);
    int y0 = (int)floorf(src_y);
    int y1 = min(y0 + 1, src_h - 1);
    x0 = max(0, x0);
    y0 = max(0, y0);

    float tx = src_x - x0;
    float ty = src_y - y0;

    DeviceColor c00 = get_pixel_color(src, src_pitch, src_w, src_h, format, x0, y0);
    DeviceColor c10 = get_pixel_color(src, src_pitch, src_w, src_h, format, x1, y0);
    DeviceColor c01 = get_pixel_color(src, src_pitch, src_w, src_h, format, x0, y1);
    DeviceColor c11 = get_pixel_color(src, src_pitch, src_w, src_h, format, x1, y1);

    float r = lerpf(lerpf(c00.red, c10.red, tx), lerpf(c01.red, c11.red, tx), ty);
    float g = lerpf(lerpf(c00.green, c10.green, tx), lerpf(c01.green, c11.green, tx), ty);
    float b = lerpf(lerpf(c00.blue, c10.blue, tx), lerpf(c01.blue, c11.blue, tx), ty);

    int plane_size = dst_w * dst_h;
    int idx = y * dst_w + x;

    // Convert float32 to half uint16_t using bitwise float_to_half
    uint32_t ir, ig, ib;
    memcpy(&ir, &r, 4);
    memcpy(&ig, &g, 4);
    memcpy(&ib, &b, 4);

    auto f2h = [](uint32_t i) -> uint16_t {
        int s = (i >> 16) & 0x00008000;
        int e = ((i >> 23) & 0x000000ff) - (127 - 15);
        int m = i & 0x007fffff;
        if (e <= 0) return s;
        if (e >= 31) return s | 0x7c00;
        return s | (e << 10) | (m >> 13);
    };

    dst_planar_half[idx] = f2h(ir);
    dst_planar_half[idx + plane_size] = f2h(ig);
    dst_planar_half[idx + plane_size * 2] = f2h(ib);
}

extern "C" void launch_preprocess_yolo_cuda(
    const void* src_ptr,
    int src_pitch,
    int src_w,
    int src_h,
    int format,
    float* dst_planar,
    int dst_w,
    int dst_h
) {
    dim3 block(16, 16);
    dim3 grid((dst_w + block.x - 1) / block.x, (dst_h + block.y - 1) / block.y);

    preprocess_yolo_kernel<<<grid, block>>>(
        static_cast<const unsigned char*>(src_ptr),
        src_pitch, src_w, src_h, format,
        dst_planar, dst_w, dst_h
    );
}

extern "C" void launch_preprocess_yolo_cuda_half(
    const void* src_ptr,
    int src_pitch,
    int src_w,
    int src_h,
    int format,
    unsigned short* dst_planar_half,
    int dst_w,
    int dst_h
) {
    dim3 block(16, 16);
    dim3 grid((dst_w + block.x - 1) / block.x, (dst_h + block.y - 1) / block.y);

    preprocess_yolo_kernel_half<<<grid, block>>>(
        static_cast<const unsigned char*>(src_ptr),
        src_pitch, src_w, src_h, format,
        dst_planar_half, dst_w, dst_h
    );
}

