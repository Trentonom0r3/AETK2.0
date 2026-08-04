#include <aetk/aegp/dom/world.hpp>
#include <aetk/effect.hpp>
#include <aetk/effect/draw/canvas.hpp>
#include <aetk/effect/ui.hpp>
#include <aetk/effect/pixel/accessor.hpp>
#include <aetk/effect/pixel/tensor_view.hpp>
#include <algorithm>
#include <sstream>

#if defined(AETK_ENABLE_CUDA)
#include <cuda_runtime.h>
#endif

using namespace aetk::effect;
using namespace aetk::aegp;
using namespace aetk::effect::ui;

// --- Generic Tint Filter for Doc Verification (Test 16) ---
template <typename PixelT, bool IsBGRA>
void apply_test_suite_tint(const smart_world& src, smart_world& dst, float tint_r, float tint_g, float tint_b) {
    using accessor = aetk::effect::pixel_accessor<PixelT, IsBGRA>;
    using ChannelT = typename accessor::channel_type;
    
    auto src_view = src.tensor_view<ChannelT>();
    auto dst_view = dst.tensor_view<ChannelT>();
    
    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            const PixelT* src_px = reinterpret_cast<const PixelT*>(&src_view(y, x, 0));
            PixelT* dst_px = reinterpret_cast<PixelT*>(&dst_view(y, x, 0));
            
            pixel_transaction<PixelT, IsBGRA> tx(src_px, dst_px);
            tx.color.red   *= tint_r;
            tx.color.green *= tint_g;
            tx.color.blue  *= tint_b;
        }
    }
}

// Parameter IDs
#define TEST_INPUT 0

class test_suite_plugin : public plugin<test_suite_plugin> {
public:
    static void on_global_setup(const global_setup_context& ctx) {
        AETK_START_TRACE("aetk_test_suite");
        ctx.enable_smart_render();
        ctx.enable_mfr();
        ctx.enable_custom_ui();
        ctx.enable_gpu_rendering(); // Required for PF_GPUDeviceSuite1 (pinned/CUDA tensor
                                    // allocation)
    }

    static void on_params_setup(const params_setup_context& ctx) {
        ctx.register_custom_ui(PF_CustomEFlag_EFFECT);
        ui::add_widget<slider<float>>(ctx, "Radius Scale", "Radius", 0.0f, 100.0f, 50.0f);
    }

    static void on_pre_render(const pre_render_context& ctx) {
        ctx.checkout_layer(TEST_INPUT, TEST_INPUT);
    }

    static void on_smart_render(const smart_render_context& ctx) {
        auto input = ctx.checkout_pixels(TEST_INPUT);
        auto output = ctx.checkout_output();
        auto pix_fmt = input.pixel_format();
        auto str = pixel_format_str(pix_fmt);
        AETK_LOG_INFO("Pixel format: {}", str);
        bool all_tests_passed = true;
        std::stringstream log;

        log << "AETK Test Suite Running...\n";

        // ----------------------------------------------------
        // Test 1: smart_world::zeros / zeros_like / empty / empty_like
        // ----------------------------------------------------
        try {
            // zeros (cleared to transparent black)
            auto w_zeros = smart_world::zeros(ctx.in_data_ptr(), 64, 48, 8);
            if (!w_zeros) {
                log << "[-] Test 1: zeros allocation failed\n";
                all_tests_passed = false;
            } else if (w_zeros.width() != 64 || w_zeros.height() != 48) {
                log << "[-] Test 1: zeros dimensions mismatch\n";
                all_tests_passed = false;
            } else {
                auto px = w_zeros.get_pixel(32, 24);
                if (px.alpha > 0.001f || px.red > 0.001f) {
                    log << "[-] Test 1: zeros is not cleared\n";
                    all_tests_passed = false;
                } else {
                    log << "[+] Test 1: zeros allocation and clearing passed\n";
                }
            }

            // zeros_like
            if (w_zeros) {
                auto w_zeros_like = smart_world::zeros_like(w_zeros);
                if (!w_zeros_like || w_zeros_like.width() != 64
                    || w_zeros_like.height() != 48
                    || w_zeros_like.pixel_format() != w_zeros.pixel_format()) {
                    log << "[-] Test 1: zeros_like failed\n";
                    all_tests_passed = false;
                } else {
                    log << "[+] Test 1: zeros_like passed\n";
                }
            }

            // empty / empty_like (uninitialized)
            auto w_empty = smart_world::empty(ctx.in_data_ptr(), 32, 32, 16);
            if (!w_empty || w_empty.pixel_format() != PF_PixelFormat_ARGB64) {
                log << "[-] Test 1: empty 16-bit failed\n";
                all_tests_passed = false;
            } else {
                auto w_empty_like = smart_world::empty_like(w_empty);
                if (!w_empty_like || w_empty_like.width() != 32
                    || w_empty_like.height() != 32
                    || w_empty_like.pixel_format() != PF_PixelFormat_ARGB64) {
                    log << "[-] Test 1: empty_like failed\n";
                    all_tests_passed = false;
                } else {
                    log << "[+] Test 1: empty/empty_like passed\n";
                }
            }
        } catch (const std::exception& e) {
            log << "[-] Test 1 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 2: aegp_world wrapping & smart_world aegp_world constructor
        // ----------------------------------------------------
        try {
            aegp_world aegp_w = aegp_world::create(
                AEGP_WorldType_8, 16, 24, 0, ctx.in_data_ptr()->pica_basicP);
            if (!aegp_w) {
                log << "[-] Test 2: aegp_world::create failed\n";
                all_tests_passed = false;
            } else {
                if (aegp_w.width() != 16 || aegp_w.height() != 24) {
                    log << "[-] Test 2: aegp_world dimensions incorrect\n";
                    all_tests_passed = false;
                } else {
                    // Test conversion constructor
                    smart_world effect_w(aegp_w, ctx.in_data_ptr());
                    if (!effect_w || effect_w.width() != 16 || effect_w.height() != 24) {
                        log << "[-] Test 2: smart_world conversion constructor failed\n";
                        all_tests_passed = false;
                    } else {
                        log << "[+] Test 2: aegp_world wrapping and constructor passed\n";
                    }
                }
            }
        } catch (const std::exception& e) {
            log << "[-] Test 2 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 3: format mismatch copy_to fallback
        // ----------------------------------------------------
        try {
            auto src_8 = smart_world::zeros(ctx.in_data_ptr(), 10, 10, 8);
            src_8.fill(aetk::core::color<>(1.0, 0.5, 0.25, 0.75));

            auto dst_16 = smart_world::zeros(ctx.in_data_ptr(), 10, 10, 16);
            src_8.copy_to(dst_16);

            auto px = dst_16.get_pixel(5, 5);
            // Verify pixel colors match within float epsilons
            if (std::abs(px.red - 0.5) > 0.02 || std::abs(px.green - 0.25) > 0.02
                || std::abs(px.blue - 0.75) > 0.02) {
                log << "[-] Test 3: format mismatch copy failed. Got color: R=" << px.red
                    << " G=" << px.green << " B=" << px.blue << "\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 3: format mismatch copy passed\n";
            }
        } catch (const std::exception& e) {
            log << "[-] Test 3 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 4: fill fallback
        // ----------------------------------------------------
        try {
            auto src_8 = smart_world::zeros(ctx.in_data_ptr(), 10, 10, 8);
            src_8.fill(aetk::core::color<>(1.0, 1.0, 1.0, 1.0));
            auto px = src_8.get_pixel(0, 0);
            if (px.red < 0.99f || px.green < 0.99f || px.blue < 0.99f) {
                log << "[-] Test 4: fill failed\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 4: fill passed\n";
            }
        } catch (const std::exception& e) {
            log << "[-] Test 4 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 5: Double-Dispatch visitor template correctness
        // ----------------------------------------------------
        try {
            int format_count = 0;
            visit_pixel_format(
                PF_PixelFormat_ARGB32, false, [&]<typename PixelT, bool IsBGRA>() {
                    if constexpr (std::is_same_v<PixelT, PF_Pixel8> && !IsBGRA) {
                        format_count++;
                    }
                });
            visit_pixel_format(
                PF_PixelFormat_ARGB64, true, [&]<typename PixelT, bool IsBGRA>() {
                    if constexpr (std::is_same_v<PixelT, PF_Pixel16> && IsBGRA) {
                        format_count++;
                    }
                });
            visit_pixel_format(
                PF_PixelFormat_ARGB128, false, [&]<typename PixelT, bool IsBGRA>() {
                    if constexpr (std::is_same_v<PixelT, PF_PixelFloat> && !IsBGRA) {
                        format_count++;
                    }
                });

            if (format_count != 3) {
                log << "[-] Test 5: Double-Dispatch visitor template correctness "
                       "failed\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 5: Double-Dispatch visitor template correctness "
                       "passed\n";
            }
        } catch (const std::exception& e) {
            log << "[-] Test 5 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 6: Raw C++ struct memory swizzle mapping
        // ----------------------------------------------------
        try {
            PF_Pixel8 px_mem { };
            aetk::core::color<> c(
                0.8, 0.2, 0.4, 0.6); // Alpha=0.8, Red=0.2, Green=0.4, Blue=0.6
            pixel_accessor<PF_Pixel8, true>::write(&px_mem, c);

            if (std::abs((int)px_mem.alpha - 153) > 1
                || std::abs((int)px_mem.red - 102) > 1
                || std::abs((int)px_mem.green - 51) > 1
                || std::abs((int)px_mem.blue - 204) > 1) {
                log << "[-] Test 6: Raw C++ struct memory swizzle mapping failed. Got: A="
                    << (int)px_mem.alpha << " R=" << (int)px_mem.red
                    << " G=" << (int)px_mem.green << " B=" << (int)px_mem.blue << "\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 6: Raw C++ struct memory swizzle mapping passed\n";
            }
        } catch (const std::exception& e) {
            log << "[-] Test 6 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 7: Accessor read/write accuracy
        // ----------------------------------------------------
        try {
            PF_Pixel8 px8 { };
            aetk::core::color<> c8(1.0, 0.5, 0.25, 0.75);
            pixel_accessor<PF_Pixel8, false>::write(&px8, c8);
            auto c8_read = pixel_accessor<PF_Pixel8, false>::read(&px8);

            PF_Pixel16 px16 { };
            aetk::core::color<> c16(0.9, 0.1, 0.3, 0.7);
            pixel_accessor<PF_Pixel16, false>::write(&px16, c16);
            auto c16_read = pixel_accessor<PF_Pixel16, false>::read(&px16);

            PF_PixelFloat px32 { };
            aetk::core::color<> c32(0.5, 0.6, 0.7, 0.8);
            pixel_accessor<PF_PixelFloat, false>::write(&px32, c32);
            auto c32_read = pixel_accessor<PF_PixelFloat, false>::read(&px32);

            bool ok = true;
            if (std::abs(c8_read.red - 0.5) > 0.01
                || std::abs(c8_read.green - 0.25) > 0.01
                || std::abs(c8_read.blue - 0.75) > 0.01) {
                log << "[-] Test 7: 8-bit read/write mismatch\n";
                ok = false;
            }
            if (std::abs(c16_read.red - 0.1) > 0.001
                || std::abs(c16_read.green - 0.3) > 0.001
                || std::abs(c16_read.blue - 0.7) > 0.001) {
                log << "[-] Test 7: 16-bit read/write mismatch\n";
                ok = false;
            }
            if (std::abs(c32_read.red - 0.6) > 0.0001
                || std::abs(c32_read.green - 0.7) > 0.0001
                || std::abs(c32_read.blue - 0.8) > 0.0001) {
                log << "[-] Test 7: 32-bit read/write mismatch\n";
                ok = false;
            }

            if (ok) {
                log << "[+] Test 7: Accessor read/write accuracy passed\n";
            } else {
                all_tests_passed = false;
            }
        } catch (const std::exception& e) {
            log << "[-] Test 7 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 8: Converters logic
        // ----------------------------------------------------
        try {
            auto src_8 = smart_world::zeros(ctx.in_data_ptr(), 4, 4, 8);
            src_8.fill(aetk::core::color<>(1.0, 0.2, 0.4, 0.6));

            std::vector<float> buffer(4 * 4 * 4, 0.0f);
            smart_world::convert_options opts;
            opts.normalize = true;
            src_8.to(smart_world::color_format::RGBA, buffer.data(), opts);

            bool ok = true;
            for (int i = 0; i < 16; i++) {
                if (std::abs(buffer[i * 4 + 0] - 0.2f) > 0.01f
                    || std::abs(buffer[i * 4 + 1] - 0.4f) > 0.01f
                    || std::abs(buffer[i * 4 + 2] - 0.6f) > 0.01f
                    || std::abs(buffer[i * 4 + 3] - 1.0f) > 0.01f) {
                    ok = false;
                    break;
                }
            }

            if (ok) {
                log << "[+] Test 8: Converters logic passed\n";
            } else {
                log << "[-] Test 8: Converters logic failed\n";
                all_tests_passed = false;
            }
        } catch (const std::exception& e) {
            log << "[-] Test 8 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 9: Vector drawing shapes
        // ----------------------------------------------------
        try {
            auto src_8 = smart_world::zeros(ctx.in_data_ptr(), 10, 10, 8);
            aetk::effect::draw::pixel(
                src_8, 5, 5, aetk::core::color<>(1.0, 1.0, 0.0, 0.0));
            auto px = src_8.get_pixel(5, 5);

            if (px.red < 0.99f || px.green > 0.01f || px.blue > 0.01f) {
                log << "[-] Test 9: Vector drawing shapes failed. Got R=" << px.red
                    << " G=" << px.green << " B=" << px.blue << "\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 9: Vector drawing shapes passed\n";
            }
        } catch (const std::exception& e) {
            log << "[-] Test 9 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 10: CPU tensor allocation, zeros factory, and view indexing
        // ----------------------------------------------------
        try {
            auto t = aetk::effect::zeros<float, 3>({ 4, 4, 3 });
            if (t.shape(0) != 4 || t.shape(1) != 4 || t.shape(2) != 3) {
                log << "[-] Test 10: CPU tensor shape mismatch\n";
                all_tests_passed = false;
            } else {
                auto v = t.view();
                v(1, 2, 0) = 12.34f;
                v(3, 1, 2) = 56.78f;

                if (std::abs(v(1, 2, 0) - 12.34f) > 0.001f
                    || std::abs(v(3, 1, 2) - 56.78f) > 0.001f) {
                    log << "[-] Test 10: CPU tensor view read/write mismatch\n";
                    all_tests_passed = false;
                } else {
                    log << "[+] Test 10: CPU tensor allocation and view indexing "
                           "passed\n";
                }
            }
        } catch (const std::exception& e) {
            log << "[-] Test 10 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 11: Pinned CPU memory and CUDA Device memory transfer (.to)
        // Note: Requires PF_GPUDeviceSuite1 — plugin must have GPU rendering
        // enabled in its PiPL (OUT_FLAGS2 GPU_RENDER_F32). CUDA device must
        // also be present at runtime; skips gracefully if neither is available.
        // ----------------------------------------------------
        try {
#if defined(AETK_ENABLE_CUDA)
            int dev_count = 0;
            if (cudaGetDeviceCount(&dev_count) != cudaSuccess || dev_count <= 0) {
                log << "[+] Test 11: No CUDA device found at runtime (skipped)\n";
            } else {
                // Pinned host allocation via AllocateHostMemory (AE-tracked)
                auto t_pinned = aetk::effect::zeros_pinned<float, 3>({ 4, 4, 3 }, ctx);
                auto v_pinned = t_pinned.view();
                v_pinned(2, 2, 1) = 99.9f;

                // High-speed DMA transfer to CUDA device memory
                auto t_cuda = t_pinned.to<aetk::effect::device_kind::cuda>();

                // Transfer back to CPU pageable to verify round-trip contents
                auto t_back = t_cuda.to<aetk::effect::device_kind::cpu>();
                auto v_back = t_back.view();

                if (std::abs(v_back(2, 2, 1) - 99.9f) > 0.001f) {
                    log << "[-] Test 11: Pinned memory to CUDA to CPU transfer failed\n";
                    all_tests_passed = false;
                } else {
                    log << "[+] Test 11: Pinned memory to CUDA to CPU transfer passed\n";
                }
            }
#else
            log << "[+] Test 11: CUDA not enabled in build (skipped)\n";
#endif
        } catch (const std::exception& e) {
            log << "[-] Test 11 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 12: Export tensor back to smart_world (.copy_to / to_world)
        // ----------------------------------------------------
        try {
            // Allocate a tensor with channel values
            auto t_src = aetk::effect::zeros<float, 3>({ 16, 16, 4 }); // ARGB
            auto v_src = t_src.view();
            // Write some colors (normalize)
            for (size_t y = 0; y < 16; ++y) {
                for (size_t x = 0; x < 16; ++x) {
                    v_src(y, x, 0) = 1.0f; // Alpha
                    v_src(y, x, 1) = 0.5f; // Red
                    v_src(y, x, 2) = 0.2f; // Green
                    v_src(y, x, 3) = 0.8f; // Blue
                }
            }

            // Export to a new 8bpc smart_world
            auto world_dst = t_src.to_world(ctx, 8);
            if (!world_dst || world_dst.width() != 16 || world_dst.height() != 16) {
                log << "[-] Test 12: exported world invalid\n";
                all_tests_passed = false;
            } else {
                auto px = world_dst.get_pixel(8, 8);
                if (std::abs(px.red - 0.5f) > 0.05f || std::abs(px.green - 0.2f) > 0.05f
                    || std::abs(px.blue - 0.8f) > 0.05f) {
                    log << "[-] Test 12: exported world color values mismatch. Got R="
                        << px.red << " G=" << px.green << " B=" << px.blue << "\n";
                    all_tests_passed = false;
                } else {
                    log << "[+] Test 12: Export tensor to smart_world passed\n";
                }
            }
        } catch (const std::exception& e) {
            log << "[-] Test 12 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }
        // ----------------------------------------------------
        // Test 13: CPU Pixel Format and Device conversions / dynamic depth conversions
        // ----------------------------------------------------
        try {
            // Let's create an 8bpc world
            auto w8 = smart_world(ctx.in_data_ptr(), 4, 4, 8, false);
            // Write distinct color components to (0,0) under tkuint8 range
            w8.set_pixel<aetk::core::pixel_range::tkuint8>(0, 0,
                aetk::core::color<aetk::core::pixel_range::tkuint8>(255, 100, 150, 200));

            // Test 8 -> 16 conversion using bitwise math validation
            // Convert to 16bpc:
            auto w16 = w8.to<aetk::core::pixel_range::tkuint8>(PF_PixelFormat_ARGB64);

            // Read back using tkuint8 range: should match exactly!
            auto c16_u8 = w16.get_pixel<aetk::core::pixel_range::tkuint8>(0, 0);

            // Expected values
            int expected_r16 = (100 << 7) + (100 >> 1) + (100 >> 7);
            int expected_g16 = (150 << 7) + (150 >> 1) + (150 >> 7);
            int expected_b16 = (200 << 7) + (200 >> 1) + (200 >> 7);

            auto raw_16 = reinterpret_cast<const PF_Pixel16*>(w16.ptr()->data);
            if (raw_16->red != expected_r16 || raw_16->green != expected_g16
                || raw_16->blue != expected_b16) {
                log << "[-] Test 13: 8 -> 16 bitwise write mismatch. Got R="
                    << raw_16->red << " (expected " << expected_r16 << ")\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 13: 8 -> 16 bitwise write verified\n";
            }

            int expected_r8_read = ((raw_16->red << 8) - raw_16->red) >> 15;
            if (static_cast<int>(c16_u8.red) != expected_r8_read) {
                log << "[-] Test 13: 16 -> 8 bitwise read mismatch. Got " << c16_u8.red
                    << "\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 13: 16 -> 8 bitwise read verified\n";
            }

            // Test 16 -> 8 conversion:
            auto w8_back = w16.to<aetk::core::pixel_range::tkuint8>(PF_PixelFormat_ARGB32);
            auto c8_back_u8 = w8_back.get_pixel<aetk::core::pixel_range::tkuint8>(0, 0);
            if (static_cast<int>(c8_back_u8.red) != expected_r8_read) {
                log << "[-] Test 13: 16 -> 8 conversion color mismatch\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 13: 16 -> 8 conversion verified\n";
            }

            // Test 32 -> 8 conversion
            auto w32 = smart_world(ctx.in_data_ptr(), 4, 4, 32, false);
            w32.set_pixel<aetk::core::pixel_range::tkuint8>(0, 0,
                aetk::core::color<aetk::core::pixel_range::tkuint8>(255, 128, 64, 192));
            auto w8_from_32 = w32.to<aetk::core::pixel_range::tkuint8>(PF_PixelFormat_ARGB32);
            auto c8_from_32
                = w8_from_32.get_pixel<aetk::core::pixel_range::tkuint8>(0, 0);
            if (std::abs(c8_from_32.red - 128.0) > 1.0
                || std::abs(c8_from_32.green - 64.0) > 1.0) {
                log << "[-] Test 13: 32 -> 8 conversion mismatch\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 13: 32 -> 8 conversion verified\n";
            }

            // Test 8 -> 16 float conversion (default Range template parameter)
            auto w16_float_default = w8.to(PF_PixelFormat_ARGB64);
            auto c16_float_default = w16_float_default.get_pixel(0, 0); // Defaults to tkfloat
            if (std::abs(c16_float_default.red - (100.0 / 255.0)) > 0.0001) {
                log << "[-] Test 13: 8 -> 16 default float conversion mismatch. Got " << c16_float_default.red << "\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 13: 8 -> 16 default float conversion verified\n";
            }

            // Test 8 -> 16 float conversion (explicit Range template parameter)
            auto w16_float_explicit = w8.to<aetk::core::pixel_range::tkfloat>(PF_PixelFormat_ARGB64);
            auto c16_float_explicit = w16_float_explicit.get_pixel<aetk::core::pixel_range::tkfloat>(0, 0);
            if (std::abs(c16_float_explicit.red - (100.0 / 255.0)) > 0.0001) {
                log << "[-] Test 13: 8 -> 16 explicit float conversion mismatch\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 13: 8 -> 16 explicit float conversion verified\n";
            }

            // Test no-op conversions
            auto w8_view = smart_world(w8.ptr(), ctx.in_data_ptr(), smart_world::ownership::NONE);
            auto w8_noop = w8_view.to<aetk::core::pixel_range::tkuint8>(PF_PixelFormat_ARGB32);
            if (w8_noop.get_ownership() != smart_world::ownership::NONE
                || w8_noop.ptr() != w8.ptr()) {
                log << "[-] Test 13: 8 -> 8 no-op conversion failed to bypass copy. "
                       "Ownership: "
                     << static_cast<int>(w8_noop.get_ownership()) << "\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 13: 8 -> 8 no-op copy bypass verified\n";
            }

            auto w16_view = smart_world(w16.ptr(), ctx.in_data_ptr(), smart_world::ownership::NONE);
            auto w16_noop = w16_view.to<aetk::core::pixel_range::tkuint8>(PF_PixelFormat_ARGB64);
            if (w16_noop.get_ownership() != smart_world::ownership::NONE
                || w16_noop.ptr() != w16.ptr()) {
                log << "[-] Test 13: 16 -> 16 no-op conversion failed to bypass copy\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 13: 16 -> 16 no-op copy bypass verified\n";
            }

            auto w32_view = smart_world(w32.ptr(), ctx.in_data_ptr(), smart_world::ownership::NONE);
            auto w32_noop = w32_view.to<aetk::core::pixel_range::tkuint8>(PF_PixelFormat_ARGB128);
            if (w32_noop.get_ownership() != smart_world::ownership::NONE
                || w32_noop.ptr() != w32.ptr()) {
                log << "[-] Test 13: 32 -> 32 no-op conversion failed to bypass copy\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 13: 32 -> 32 no-op copy bypass verified\n";
            }

        } catch (const std::exception& e) {
            log << "[-] Test 13 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 14: Deep-copy cloning (.clone() method)
        // ----------------------------------------------------
        try {
            auto original = smart_world::zeros(ctx.in_data_ptr(), 8, 8, 8);
            original.set_pixel<aetk::core::pixel_range::tkuint8>(2, 3,
                aetk::core::color<aetk::core::pixel_range::tkuint8>(255, 120, 130, 140));

            // Perform clone
            auto duplicated = original.clone();

            if (!duplicated) {
                log << "[-] Test 14: clone returned empty/invalid smart_world\n";
                all_tests_passed = false;
            } else if (duplicated.width() != original.width() || duplicated.height() != original.height()
                || duplicated.pixel_format() != original.pixel_format()) {
                log << "[-] Test 14: clone dimensions or format mismatch\n";
                all_tests_passed = false;
            } else {
                // Verify pixel contents are copied correctly
                auto orig_px = original.get_pixel<aetk::core::pixel_range::tkuint8>(2, 3);
                auto dup_px = duplicated.get_pixel<aetk::core::pixel_range::tkuint8>(2, 3);
                if (dup_px.red != orig_px.red || dup_px.green != orig_px.green || dup_px.blue != orig_px.blue) {
                    log << "[-] Test 14: cloned pixel color mismatch\n";
                    all_tests_passed = false;
                } else {
                    // Modify clone and verify original is not mutated
                    duplicated.set_pixel<aetk::core::pixel_range::tkuint8>(2, 3,
                        aetk::core::color<aetk::core::pixel_range::tkuint8>(255, 10, 20, 30));
                    auto orig_px_after = original.get_pixel<aetk::core::pixel_range::tkuint8>(2, 3);
                    auto dup_px_after = duplicated.get_pixel<aetk::core::pixel_range::tkuint8>(2, 3);

                    if (orig_px_after.red != orig_px.red) {
                        log << "[-] Test 14: mutating clone mutated the original world (not a deep copy)\n";
                        all_tests_passed = false;
                    } else if (dup_px_after.red != 10) {
                        log << "[-] Test 14: mutating clone failed to update clone pixel\n";
                        all_tests_passed = false;
                    } else {
                        log << "[+] Test 14: deep-copy clone verified\n";
                    }
                }
            }
        } catch (const std::exception& e) {
            log << "[-] Test 14 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 15: Custom UI Widget Parameter Verification (Doc verification)
        // ----------------------------------------------------
        try {
            auto scale_param = ctx.param<arbitrary_param<ui::slider_data<float>>>("Radius Scale");
            float radius = scale_param.value()->value;
            if (radius < 0.0f || radius > 100.0f) {
                log << "[-] Test 15: custom UI parameter slider value out of bounds (" << radius << ")\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 15: custom UI parameter query verified (value=" << radius << ")\n";
            }
        } catch (const std::exception& e) {
            log << "[-] Test 15 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        // ----------------------------------------------------
        // Test 16: Dynamic Bit Depth Dispatching & Swizzling (apply_tint)
        // ----------------------------------------------------
        try {
            auto w_src = smart_world::zeros(ctx.in_data_ptr(), 4, 4, 8);
            auto w_dst = smart_world::zeros(ctx.in_data_ptr(), 4, 4, 8);
            w_src.fill(aetk::core::color<>(1.0, 1.0, 1.0, 1.0)); // Fill with white

            visit_pixel_format(w_src.pixel_format(), w_src.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
                apply_test_suite_tint<PixelT, IsBGRA>(w_src, w_dst, 0.5f, 0.2f, 0.8f);
            });

            auto px = w_dst.get_pixel(2, 2);
            if (std::abs(px.red - 0.5f) > 0.05f || std::abs(px.green - 0.2f) > 0.05f || std::abs(px.blue - 0.8f) > 0.05f) {
                log << "[-] Test 16: dynamic bit depth tinting failed. Got R=" << px.red << " G=" << px.green << " B=" << px.blue << "\n";
                all_tests_passed = false;
            } else {
                log << "[+] Test 16: dynamic bit depth dispatching & swizzling (apply_tint) verified\n";
            }
        } catch (const std::exception& e) {
            log << "[-] Test 16 threw exception: " << e.what() << "\n";
            all_tests_passed = false;
        }

        AETK_LOG_INFO(log.str());
        // ----------------------------------------------------
        // Output result visual reporting
        // ----------------------------------------------------

        if (all_tests_passed) {
            output.fill(aetk::core::color<>(1.0, 0.0, 1.0, 0.0)); // Green for Pass
        } else {
            output.fill(aetk::core::color<>(1.0, 1.0, 0.0, 0.0)); // Red for Fail
            throw aetk::core::exception(
                PF_Err_OUT_OF_MEMORY, "AETK Test Suite Failed! Check logs.");
        }
    }
};

AETK_EFFECT_MAIN(test_suite_plugin)
