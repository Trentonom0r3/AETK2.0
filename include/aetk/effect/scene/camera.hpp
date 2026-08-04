#pragma once

#include <aetk/core/suite.hpp>
#include <aetk/effect/context/context.hpp>
#include <AE_EffectCBSuites.h>
#include <AE_AdvEffectSuites.h>
#include <AE_GeneralPlug.h>

namespace aetk::effect {

/**
 * @brief Represents the After Effects composition camera during an effect render.
 * 
 * @details Fetches the 3D projection matrix using AEGP_PFInterfaceSuite.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, querying 3D composition cameras and retrieving their spatial transforms during effect rendering requires manual lookup of `AEGP_PFInterfaceSuite1`, structuring raw temporal time markers (`A_Time`), and passing multiple raw pointer parameters to `AEGP_GetEffectCameraMatrix`. `aetk::effect::camera` isolates this complex lookup sequence in a single modern OOP constructor, instantly fetching projection matrices, image planes, and focal distances cleanly.
 *
 * @warning <b>Memory & Lifecycles:</b> The camera wrapper is an inspector class that queries temporary spatial properties from the After Effects host. It does not own the scene lifecycle. Callers must check `is_valid()` before utilizing the projection matrices, as 2D compositions do not contain active camera layers.
 */
class camera {
public:
    /**
     * @brief Queries composition camera metrics for the active context.
     * 
     * If no camera exists, `is_valid()` will return false.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces procedural camera matrix queries with type-safe constructors.
     *
     * @warning <b>Memory & Lifecycles:</b> Binds temporal context frames. Check `is_valid()` to verify camera existence.
     *
     * @param ctx Bounded rendering context.
     */
    explicit camera(const context& ctx) {
        auto suite = core::suite<AEGP_PFInterfaceSuite1, core::fixed_string(kAEGPPFInterfaceSuite), kAEGPPFInterfaceSuiteVersion1>();
        
        // Manually construct A_Time (initially with effect time)
        A_Time t;
        t.value = ctx.in_data_ptr()->current_time;
        t.scale = ctx.in_data_ptr()->time_scale;
        
        // Convert to composition time
        A_Time comp_time;
        A_Err time_err = suite->AEGP_ConvertEffectToCompTime(
            ctx.in_data_ptr()->effect_ref,
            ctx.in_data_ptr()->current_time,
            ctx.in_data_ptr()->time_scale,
            &comp_time
        );
        if (time_err == A_Err_NONE) {
            t = comp_time;
        }

        // Try to get the active 3D camera layer first
        AEGP_LayerH camera_layerH = nullptr;
        A_Err cam_err = suite->AEGP_GetEffectCamera(
            ctx.in_data_ptr()->effect_ref,
            &t,
            &camera_layerH
        );
        
        if (cam_err == A_Err_NONE && camera_layerH) {
            auto layer_suite = core::suite<AEGP_LayerSuite8, core::fixed_string(kAEGPLayerSuite), kAEGPLayerSuiteVersion8>();
            A_Matrix4 cam_to_world{};
            A_Err xform_err = layer_suite->AEGP_GetLayerToWorldXformFromView(
                camera_layerH,
                &t,
                &t,
                &cam_to_world
            );
            if (xform_err == A_Err_NONE) {
                // Apply flipZ to camera's local-to-world matrix (negate row 2 / Z axis)
                cam_to_world.mat[2][0] *= -1.0;
                cam_to_world.mat[2][1] *= -1.0;
                cam_to_world.mat[2][2] *= -1.0;
                cam_to_world.mat[2][3] *= -1.0;

                // Compute world-to-camera matrix by inverting cam_to_world (rigid-body transpose inversion)
                // Transpose 3x3 rotation part
                m_matrix.mat[0][0] = cam_to_world.mat[0][0];
                m_matrix.mat[0][1] = cam_to_world.mat[1][0];
                m_matrix.mat[0][2] = cam_to_world.mat[2][0];
                m_matrix.mat[0][3] = 0.0;
                
                m_matrix.mat[1][0] = cam_to_world.mat[0][1];
                m_matrix.mat[1][1] = cam_to_world.mat[1][1];
                m_matrix.mat[1][2] = cam_to_world.mat[2][1];
                m_matrix.mat[1][3] = 0.0;
                
                m_matrix.mat[2][0] = cam_to_world.mat[0][2];
                m_matrix.mat[2][1] = cam_to_world.mat[1][2];
                m_matrix.mat[2][2] = cam_to_world.mat[2][2];
                m_matrix.mat[2][3] = 0.0;
                
                // t_inv = -R^T * t
                double tx = cam_to_world.mat[3][0];
                double ty = cam_to_world.mat[3][1];
                double tz = cam_to_world.mat[3][2];
                
                m_matrix.mat[3][0] = -(m_matrix.mat[0][0] * tx + m_matrix.mat[1][0] * ty + m_matrix.mat[2][0] * tz);
                m_matrix.mat[3][1] = -(m_matrix.mat[0][1] * tx + m_matrix.mat[1][1] * ty + m_matrix.mat[2][1] * tz);
                m_matrix.mat[3][2] = -(m_matrix.mat[0][2] * tx + m_matrix.mat[1][2] * ty + m_matrix.mat[2][2] * tz);
                m_matrix.mat[3][3] = 1.0;
                
                // Fetch Zoom for focal distance
                m_dist_to_plane = 0.0;
                auto stream_suite = core::suite<AEGP_StreamSuite6, core::fixed_string(kAEGPStreamSuite), kAEGPStreamSuiteVersion6>();
                AEGP_StreamVal2 stream_val{};
                AEGP_StreamType stream_type{};
                A_Err val_err = stream_suite->AEGP_GetLayerStreamValue(
                    camera_layerH,
                    AEGP_LayerStream_ZOOM,
                    AEGP_LTimeMode_CompTime,
                    &t,
                    FALSE,
                    &stream_val,
                    &stream_type
                );
                if (val_err == A_Err_NONE) {
                    m_dist_to_plane = stream_val.one_d;
                } else {
                    // Fallback to default camera distance if zoom stream query fails
                    AEGP_CompH compH = nullptr;
                    A_Err layer_comp_err = layer_suite->AEGP_GetLayerParentComp(camera_layerH, &compH);
                    if (layer_comp_err == A_Err_NONE && compH) {
                        auto camera_suite = core::suite<AEGP_CameraSuite2, core::fixed_string(kAEGPCameraSuite), kAEGPCameraSuiteVersion2>();
                        camera_suite->AEGP_GetDefaultCameraDistanceToImagePlane(compH, &m_dist_to_plane);
                    }
                }
                
                m_width = ctx.in_data_ptr()->width;
                m_height = ctx.in_data_ptr()->height;
                m_valid = true;
            }
        } else {
            // Fallback for default composition camera
            A_Err err = suite->AEGP_GetEffectCameraMatrix(
                ctx.in_data_ptr()->effect_ref,
                &t,
                &m_matrix,
                &m_dist_to_plane,
                &m_width,
                &m_height
            );
            if (err == A_Err_NONE) {
                m_valid = true;
            }
        }
    }

    /**
     * @brief Verify if a valid camera was found.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern OOP properties replacing raw C struct pointers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return True if valid camera exists.
     */
    bool is_valid() const { return m_valid; }

    /**
     * @brief Returns the raw 4x4 Transformation Matrix.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern OOP properties replacing raw C struct pointers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Raw 4x4 transform matrix.
     */
    const A_Matrix4& get_matrix() const { return m_matrix; }

    /**
     * @brief Distance from the focal point to the image plane.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern OOP properties replacing raw C struct pointers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Focal distance.
     */
    double get_focal_distance() const { return m_dist_to_plane; }

    /**
     * @brief Bounded width of the camera image plane.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern OOP properties replacing raw C struct pointers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Plane width.
     */
    int16_t get_image_plane_width() const { return m_width; }

    /**
     * @brief Bounded height of the camera image plane.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern OOP properties replacing raw C struct pointers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Plane height.
     */
    int16_t get_image_plane_height() const { return m_height; }

private:
    A_Matrix4 m_matrix{};
    A_FpLong  m_dist_to_plane = 0.0;
    A_short   m_width = 0;
    A_short   m_height = 0;
    bool      m_valid = false;
};

} // namespace aetk::effect
