#pragma once

#include <aetk/aegp/dom/comp.hpp>

namespace aetk::aegp {

/**
 * @brief Represents a standard Audio/Video layer (AVLayer) in After Effects.
 */
class av_layer : public layer {
public:
    using layer::layer;

    explicit av_layer(const layer& ly) : layer(ly) {
        if (ly && ly.get_object_type() != AEGP_ObjectType_AV) {
            this->m_handle = nullptr;
        }
    }

    av_layer& operator=(const layer& ly) {
        if (ly && ly.get_object_type() == AEGP_ObjectType_AV) {
            this->m_handle = ly.get();
        } else {
            this->m_handle = nullptr;
        }
        return *this;
    }

    owned_stream anchor_point() const { return get_stream(AEGP_LayerStream_ANCHORPOINT); }
    owned_stream position() const { return get_stream(AEGP_LayerStream_POSITION); }
    owned_stream scale() const { return get_stream(AEGP_LayerStream_SCALE); }
    owned_stream rotation() const { return get_stream(AEGP_LayerStream_ROTATION); }
    owned_stream opacity() const { return get_stream(AEGP_LayerStream_OPACITY); }
};

/**
 * @brief Represents a Camera layer in After Effects.
 */
class camera_layer : public layer {
public:
    using layer::layer;

    explicit camera_layer(const layer& ly) : layer(ly) {
        if (ly && ly.get_object_type() != AEGP_ObjectType_CAMERA) {
            this->m_handle = nullptr;
        }
    }

    camera_layer& operator=(const layer& ly) {
        if (ly && ly.get_object_type() == AEGP_ObjectType_CAMERA) {
            this->m_handle = ly.get();
        } else {
            this->m_handle = nullptr;
        }
        return *this;
    }

    owned_stream zoom() const { return get_stream(AEGP_LayerStream_ZOOM); }
    owned_stream depth_of_field() const { return get_stream(AEGP_LayerStream_DEPTH_OF_FIELD); }
    owned_stream focus_distance() const { return get_stream(AEGP_LayerStream_FOCUS_DISTANCE); }
    owned_stream aperture() const { return get_stream(AEGP_LayerStream_APERTURE); }
    owned_stream blur_level() const { return get_stream(AEGP_LayerStream_BLUR_LEVEL); }
};

/**
 * @brief Represents a Light layer in After Effects.
 */
class light_layer : public layer {
public:
    using layer::layer;

    explicit light_layer(const layer& ly) : layer(ly) {
        if (ly && ly.get_object_type() != AEGP_ObjectType_LIGHT) {
            this->m_handle = nullptr;
        }
    }

    light_layer& operator=(const layer& ly) {
        if (ly && ly.get_object_type() == AEGP_ObjectType_LIGHT) {
            this->m_handle = ly.get();
        } else {
            this->m_handle = nullptr;
        }
        return *this;
    }

    owned_stream intensity() const { return get_stream(AEGP_LayerStream_INTENSITY); }
    owned_stream color() const { return get_stream(AEGP_LayerStream_COLOR); }
    owned_stream cone_angle() const { return get_stream(AEGP_LayerStream_CONE_ANGLE); }
    owned_stream cone_feather() const { return get_stream(AEGP_LayerStream_CONE_FEATHER); }
};

/**
 * @brief Represents a Text layer in After Effects.
 */
class text_layer : public layer {
public:
    using layer::layer;

    explicit text_layer(const layer& ly) : layer(ly) {
        if (ly && ly.get_object_type() != AEGP_ObjectType_TEXT) {
            this->m_handle = nullptr;
        }
    }

    text_layer& operator=(const layer& ly) {
        if (ly && ly.get_object_type() == AEGP_ObjectType_TEXT) {
            this->m_handle = ly.get();
        } else {
            this->m_handle = nullptr;
        }
        return *this;
    }

    owned_stream text_document() const { return get_stream(AEGP_LayerStream_SOURCE_TEXT); }
};

/**
 * @brief Represents a Vector shape layer in After Effects.
 */
class vector_layer : public layer {
public:
    using layer::layer;

    explicit vector_layer(const layer& ly) : layer(ly) {
        if (ly && ly.get_object_type() != AEGP_ObjectType_VECTOR) {
            this->m_handle = nullptr;
        }
    }

    vector_layer& operator=(const layer& ly) {
        if (ly && ly.get_object_type() == AEGP_ObjectType_VECTOR) {
            this->m_handle = ly.get();
        } else {
            this->m_handle = nullptr;
        }
        return *this;
    }
};

} // namespace aetk::aegp
