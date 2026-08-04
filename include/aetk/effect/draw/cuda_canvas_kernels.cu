#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cmath>
#include <aetk/effect/draw/cuda_canvas.hpp>

namespace aetk::effect::draw {

struct GpuPixel {
    float blue;
    float green;
    float red;
    float alpha;
};

__device__ __forceinline__ void blend_pixel(GpuPixel* p, float r, float g, float b, float a) {
    if (a <= 0.0f) return;
    if (a >= 1.0f) {
        p->red = r;
        p->green = g;
        p->blue = b;
        p->alpha = 1.0f;
    } else {
        float inv_a = 1.0f - a;
        p->red = p->red * inv_a + r * a;
        p->green = p->green * inv_a + g * a;
        p->blue = p->blue * inv_a + b * a;
        p->alpha = p->alpha * inv_a + a;
    }
}

__device__ __forceinline__ float get_segment_dist_sq(float x, float y, float x0, float y0, float x1, float y1) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len_sq = dx * dx + dy * dy;
    if (len_sq < 0.0001f) {
        float d_x = x - x0;
        float d_y = y - y0;
        return d_x * d_x + d_y * d_y;
    }
    float t = ((x - x0) * dx + (y - y0) * dy) / len_sq;
    t = fmaxf(0.0f, fminf(1.0f, t));
    float proj_x = x0 + t * dx;
    float proj_y = y0 + t * dy;
    float d_x = x - proj_x;
    float d_y = y - proj_y;
    return d_x * d_x + d_y * d_y;
}

// ── Kernels ────────────────────────────────────────────────────────

__global__ void draw_pixel_kernel(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    int x,
    int y,
    float r, float g, float b, float a
) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    char* row = (char*)gpu_data + y * rowbytes;
    GpuPixel* p = (GpuPixel*)row + x;
    blend_pixel(p, r, g, b, a);
}

__global__ void draw_line_kernel(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    float x0, float y0,
    float x1, float y1,
    float r, float g, float b, float a,
    float thickness,
    int y_min, int y_max
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int y = y_min + idx;
    if (y < y_min || y > y_max) return;

    float dy = y1 - y0;
    float dx = x1 - x0;
    int x_start_proj = (int)fminf(x0, x1) - (int)ceilf(thickness) - 2;
    int x_end_proj = (int)fmaxf(x0, x1) + (int)ceilf(thickness) + 2;
    int x_start = fmaxf(0, x_start_proj);
    int x_end = fminf(width - 1, x_end_proj);

    if (fabsf(dy) > 1.0f) {
        float t = (float)(y - y0) / dy;
        float x_proj = x0 + t * dx;
        int buffer = (int)ceilf(thickness) + 2;
        x_start = fmaxf(x_start, (int)floorf(x_proj - buffer));
        x_end = fminf(x_end, (int)ceilf(x_proj + buffer));
    }

    char* row = (char*)gpu_data + y * rowbytes;
    GpuPixel* pixels = (GpuPixel*)row;

    float thickness_sq = thickness * thickness;

    for (int x = x_start; x <= x_end; ++x) {
        float d_sq = get_segment_dist_sq((float)x, (float)y, x0, y0, x1, y1);
        if (d_sq <= thickness_sq) {
            float dist = sqrtf(d_sq);
            float weight = 1.0f;
            if (dist > thickness - 1.0f) {
                weight = 1.0f - (dist - (thickness - 1.0f));
            }
            blend_pixel(&pixels[x], r, g, b, a * weight);
        }
    }
}

__global__ void draw_triangle_kernel(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    float x0, float y0,
    float x1, float y1,
    float x2, float y2,
    float r, float g, float b, float a,
    int y_min, int y_max
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int y = y_min + idx;
    if (y < y_min || y > y_max) return;

    float x_min_val = (float)width;
    float x_max_val = -1.0f;
    int intersects = 0;

    auto check_edge = [&](float ax, float ay, float bx, float by) {
        if ((ay <= (float)y && by >= (float)y) || (by <= (float)y && ay >= (float)y)) {
            if (fabsf(by - ay) > 0.0001f) {
                float t = ((float)y - ay) / (by - ay);
                float x_val = ax + t * (bx - ax);
                x_min_val = fminf(x_min_val, x_val);
                x_max_val = fmaxf(x_max_val, x_val);
                intersects++;
            }
        }
    };

    check_edge(x0, y0, x1, y1);
    check_edge(x1, y1, x2, y2);
    check_edge(x2, y2, x0, y0);

    if (intersects > 0) {
        int x_start = fmaxf(0.0f, x_min_val);
        int x_end = fminf((float)(width - 1), x_max_val);

        char* row = (char*)gpu_data + y * rowbytes;
        GpuPixel* pixels = (GpuPixel*)row;
        for (int x = x_start; x <= x_end; ++x) {
            blend_pixel(&pixels[x], r, g, b, a);
        }
    }
}

__global__ void draw_dot_marker_kernel(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    int cx, int cy,
    int radius,
    float r, float g, float b, float a,
    int y_min, int y_max
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int y = y_min + idx;
    if (y < y_min || y > y_max) return;

    float dy = (float)(y - cy);
    float r2 = (float)(radius * radius);
    float dx_sq = r2 - dy * dy;
    if (dx_sq >= 0.0f) {
        float dx = sqrtf(dx_sq);
        int x_start = fmaxf(0.0f, (float)cx - dx);
        int x_end = fminf((float)(width - 1), (float)cx + dx);

        char* row = (char*)gpu_data + y * rowbytes;
        GpuPixel* pixels = (GpuPixel*)row;
        for (int x = x_start; x <= x_end; ++x) {
            blend_pixel(&pixels[x], r, g, b, a);
        }
    }
}

// ── Host Wrappers ──────────────────────────────────────────────────

void cuda_draw_pixel(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    int x,
    int y,
    float r, float g, float b, float a
) {
    draw_pixel_kernel<<<1, 1>>>(
        gpu_data, width, height, rowbytes, x, y,
        r, g, b, a
    );
}

void cuda_draw_line(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    int x0,
    int y0,
    int x1,
    int y1,
    float r, float g, float b, float a,
    int thickness
) {
    int y_min = fmaxf(0, fminf(y0, y1) - thickness);
    int y_max = fminf(height - 1, fmaxf(y0, y1) + thickness);
    int num_scanlines = y_max - y_min + 1;
    if (num_scanlines <= 0) return;

    int threads = 64;
    int blocks = (num_scanlines + threads - 1) / threads;

    draw_line_kernel<<<blocks, threads>>>(
        gpu_data, width, height, rowbytes,
        (float)x0, (float)y0, (float)x1, (float)y1,
        r, g, b, a,
        (float)thickness, y_min, y_max
    );
}

void cuda_draw_triangle(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    int x0,
    int y0,
    int x1,
    int y1,
    int x2,
    int y2,
    float r, float g, float b, float a
) {
    int y_min = fmaxf(0, fminf(y0, fminf(y1, y2)));
    int y_max = fminf(height - 1, fmaxf(y0, fmaxf(y1, y2)));
    int num_scanlines = y_max - y_min + 1;
    if (num_scanlines <= 0) return;

    int threads = 64;
    int blocks = (num_scanlines + threads - 1) / threads;

    draw_triangle_kernel<<<blocks, threads>>>(
        gpu_data, width, height, rowbytes,
        (float)x0, (float)y0, (float)x1, (float)y1, (float)x2, (float)y2,
        r, g, b, a,
        y_min, y_max
    );
}

void cuda_draw_dot_marker(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    int cx,
    int cy,
    int size,
    float r, float g, float b, float a
) {
    int y_min = fmaxf(0, cy - size);
    int y_max = fminf(height - 1, cy + size);
    int num_scanlines = y_max - y_min + 1;
    if (num_scanlines <= 0) return;

    int threads = 64;
    int blocks = (num_scanlines + threads - 1) / threads;

    draw_dot_marker_kernel<<<blocks, threads>>>(
        gpu_data, width, height, rowbytes,
        cx, cy, size,
        r, g, b, a,
        y_min, y_max
    );
}



__global__ void multiply_pixels_kernel(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    float factor
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    char* row = (char*)gpu_data + y * rowbytes;
    GpuPixel* p = (GpuPixel*)row + x;
    p->red *= factor;
    p->green *= factor;
    p->blue *= factor;
}

void cuda_multiply_pixels(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    float factor,
    void* stream
) {
    if (width <= 0 || height <= 0) return;

    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);

    cudaStream_t custream = (cudaStream_t)stream;

    multiply_pixels_kernel<<<grid, block, 0, custream>>>(
        gpu_data, width, height, rowbytes, factor
    );
}

__global__ void fill_pixels_kernel(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    float r, float g, float b, float a,
    int rect_left, int rect_top, int rect_right, int rect_bottom
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;
    if (x < rect_left || x >= rect_right || y < rect_top || y >= rect_bottom) return;

    char* row = (char*)gpu_data + y * rowbytes;
    GpuPixel* p = (GpuPixel*)row + x;
    p->red = r;
    p->green = g;
    p->blue = b;
    p->alpha = a;
}

void cuda_fill_pixels(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    float r, float g, float b, float a,
    int rect_left, int rect_top, int rect_right, int rect_bottom,
    void* stream
) {
    if (width <= 0 || height <= 0) return;

    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);

    cudaStream_t custream = (cudaStream_t)stream;

    fill_pixels_kernel<<<grid, block, 0, custream>>>(
        gpu_data, width, height, rowbytes,
        r, g, b, a,
        rect_left, rect_top, rect_right, rect_bottom
    );
}

__global__ void draw_lines_batched_kernel(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    const CudaLineSegment* lines,
    int num_lines,
    const CudaScanlineTask* tasks,
    int num_tasks
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_tasks) return;

    CudaScanlineTask task = tasks[idx];
    int line_idx = task.line_index;
    int y = task.y;

    if (line_idx < 0 || line_idx >= num_lines) return;
    if (y < 0 || y >= height) return;

    CudaLineSegment l = lines[line_idx];

    float dy = l.y1 - l.y0;
    float dx = l.x1 - l.x0;
    float thickness = l.thickness;

    int x_start_proj = (int)fminf(l.x0, l.x1) - (int)ceilf(thickness) - 2;
    int x_end_proj = (int)fmaxf(l.x0, l.x1) + (int)ceilf(thickness) + 2;
    int x_start = fmaxf(0, x_start_proj);
    int x_end = fminf(width - 1, x_end_proj);

    if (fabsf(dy) > 1.0f) {
        float t = (float)(y - l.y0) / dy;
        float x_proj = l.x0 + t * dx;
        int buffer = (int)ceilf(thickness) + 2;
        x_start = fmaxf(x_start, (int)floorf(x_proj - buffer));
        x_end = fminf(x_end, (int)ceilf(x_proj + buffer));
    }

    char* row = (char*)gpu_data + y * rowbytes;
    GpuPixel* pixels = (GpuPixel*)row;

    float thickness_sq = thickness * thickness;

    for (int x = x_start; x <= x_end; ++x) {
        float d_sq = get_segment_dist_sq((float)x, (float)y, l.x0, l.y0, l.x1, l.y1);
        if (d_sq <= thickness_sq) {
            float dist = sqrtf(d_sq);
            float weight = 1.0f;
            if (dist > thickness - 1.0f) {
                weight = 1.0f - (dist - (thickness - 1.0f));
            }
            blend_pixel(&pixels[x], l.r, l.g, l.b, l.a * weight);
        }
    }
}

__global__ void draw_triangles_batched_kernel(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    const CudaTriangle* tris,
    int num_tris,
    const CudaTriangleTask* tasks,
    int num_tasks
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_tasks) return;

    CudaTriangleTask task = tasks[idx];
    int tri_idx = task.triangle_index;
    int y = task.y;

    if (tri_idx < 0 || tri_idx >= num_tris) return;
    if (y < 0 || y >= height) return;

    CudaTriangle t = tris[tri_idx];

    float x_min_val = (float)width;
    float x_max_val = -1.0f;
    int intersects = 0;

    // Edge 0-1
    if ((t.y0 <= (float)y && t.y1 >= (float)y) || (t.y1 <= (float)y && t.y0 >= (float)y)) {
        if (fabsf(t.y1 - t.y0) > 0.0001f) {
            float t_val = ((float)y - t.y0) / (t.y1 - t.y0);
            float x_val = t.x0 + t_val * (t.x1 - t.x0);
            x_min_val = fminf(x_min_val, x_val);
            x_max_val = fmaxf(x_max_val, x_val);
            intersects++;
        }
    }
    // Edge 1-2
    if ((t.y1 <= (float)y && t.y2 >= (float)y) || (t.y2 <= (float)y && t.y1 >= (float)y)) {
        if (fabsf(t.y2 - t.y1) > 0.0001f) {
            float t_val = ((float)y - t.y1) / (t.y2 - t.y1);
            float x_val = t.x1 + t_val * (t.x2 - t.x1);
            x_min_val = fminf(x_min_val, x_val);
            x_max_val = fmaxf(x_max_val, x_val);
            intersects++;
        }
    }
    // Edge 2-0
    if ((t.y2 <= (float)y && t.y0 >= (float)y) || (t.y0 <= (float)y && t.y2 >= (float)y)) {
        if (fabsf(t.y0 - t.y2) > 0.0001f) {
            float t_val = ((float)y - t.y2) / (t.y0 - t.y2);
            float x_val = t.x2 + t_val * (t.x0 - t.x2);
            x_min_val = fminf(x_min_val, x_val);
            x_max_val = fmaxf(x_max_val, x_val);
            intersects++;
        }
    }

    if (intersects > 0) {
        int x_start = fmaxf(0.0f, x_min_val);
        int x_end = fminf((float)(width - 1), x_max_val);

        char* row = (char*)gpu_data + y * rowbytes;
        GpuPixel* pixels = (GpuPixel*)row;
        for (int x = x_start; x <= x_end; ++x) {
            blend_pixel(&pixels[x], t.r, t.g, t.b, t.a);
        }
    }
}

void cuda_draw_lines_batched(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    const CudaLineSegment* d_lines,
    int num_lines,
    const CudaScanlineTask* d_tasks,
    int num_tasks,
    void* stream
) {
    if (num_tasks <= 0) return;

    int threads = 128;
    int blocks = (num_tasks + threads - 1) / threads;

    cudaStream_t custream = (cudaStream_t)stream;

    draw_lines_batched_kernel<<<blocks, threads, 0, custream>>>(
        gpu_data, width, height, rowbytes,
        d_lines, num_lines, d_tasks, num_tasks
    );
}

void cuda_draw_triangles_batched(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    const CudaTriangle* d_tris,
    int num_tris,
    const CudaTriangleTask* d_tasks,
    int num_tasks,
    void* stream
) {
    if (num_tasks <= 0) return;

    int threads = 128;
    int blocks = (num_tasks + threads - 1) / threads;

    cudaStream_t custream = (cudaStream_t)stream;

    draw_triangles_batched_kernel<<<blocks, threads, 0, custream>>>(
        gpu_data, width, height, rowbytes,
        d_tris, num_tris, d_tasks, num_tasks
    );
}

} // namespace aetk::effect::draw

#include <aetk/effect/gpu/kernels/swizzle.cu>

