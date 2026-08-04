#pragma once

namespace aetk::effect::draw {

#ifdef AETK_ENABLE_CUDA

// Core CUDA drawing functions.
// These are implemented in cuda_canvas_kernels.cu.
// Color values are unpacked to floats to avoid compiling complex C++ type headers with NVCC.

void cuda_draw_pixel(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    int x,
    int y,
    float r, float g, float b, float a
);

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
);

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
);

void cuda_draw_dot_marker(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    int cx,
    int cy,
    int size,
    float r, float g, float b, float a
);

void cuda_multiply_pixels(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    float factor,
    void* stream = nullptr
);

void cuda_fill_pixels(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    float r, float g, float b, float a,
    int rect_left, int rect_top, int rect_right, int rect_bottom,
    void* stream = nullptr
);

struct CudaLineSegment {
    float x0, y0;
    float x1, y1;
    float r, g, b, a;
    float thickness;
};

struct CudaScanlineTask {
    int line_index;
    int y;
};

struct CudaTriangle {
    float x0, y0;
    float x1, y1;
    float x2, y2;
    float r, g, b, a;
};

struct CudaTriangleTask {
    int triangle_index;
    int y;
};

void cuda_draw_lines_batched(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    const CudaLineSegment* d_lines,
    int num_lines,
    const CudaScanlineTask* d_tasks,
    int num_tasks,
    void* stream = nullptr
);

void cuda_draw_triangles_batched(
    void* gpu_data,
    int width,
    int height,
    int rowbytes,
    const CudaTriangle* d_tris,
    int num_tris,
    const CudaTriangleTask* d_tasks,
    int num_tasks,
    void* stream = nullptr
);

#endif // AETK_ENABLE_CUDA

} // namespace aetk::effect::draw
