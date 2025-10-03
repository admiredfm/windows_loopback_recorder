/*
** Embedded libsamplerate implementation for Windows Loopback Recorder
** Simplified but functional implementation of key libsamplerate APIs
*/

#include "samplerate.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

// Internal state structure
struct SRC_STATE_tag {
    int converter_type;
    int channels;
    double src_ratio;
    bool ratio_set;

    // Linear interpolation state
    float* last_sample;
    double position;

    int error;

    SRC_STATE_tag(int type, int ch) :
        converter_type(type), channels(ch), src_ratio(1.0),
        ratio_set(false), position(0.0), error(0) {
        last_sample = new float[channels];
        memset(last_sample, 0, channels * sizeof(float));
    }

    ~SRC_STATE_tag() {
        delete[] last_sample;
    }
};

// Error messages
static const char* error_messages[] = {
    "No error.",
    "Memory allocation failed.",
    "Bad SRC_STATE.",
    "Bad data pointer.",
    "Bad data pointer.",
    "Internal error : no private data.",
    "SRC ratio outside [1/256, 256] range.",
    "Bad internal state.",
    "Bad converter.",
    "Bad channel count.",
    "Internal error."
};

// Simple linear interpolation resampler
static int linear_resample(SRC_STATE* state, SRC_DATA* data) {
    if (!state || !data) return SRC_ERR_BAD_DATA_PTR;

    const float* input = data->data_in;
    float* output = data->data_out;

    long input_frames = data->input_frames;
    long output_frames = data->output_frames;
    double ratio = data->src_ratio;

    if (ratio <= 0.0) return SRC_ERR_BAD_SRC_RATIO;

    long frames_used = 0;
    long frames_gen = 0;

    double step = 1.0 / ratio;

    while (frames_gen < output_frames && state->position < input_frames - 1) {
        long index = static_cast<long>(state->position);
        double fraction = state->position - index;

        if (index + 1 >= input_frames) break;

        // Linear interpolation for each channel
        for (int ch = 0; ch < state->channels; ch++) {
            float sample1 = input[index * state->channels + ch];
            float sample2 = input[(index + 1) * state->channels + ch];
            output[frames_gen * state->channels + ch] =
                static_cast<float>(sample1 * (1.0 - fraction) + sample2 * fraction);
        }

        frames_gen++;
        state->position += step;
    }

    // Update position for next call
    state->position -= input_frames;
    if (state->position < 0) state->position = 0;

    frames_used = std::min(input_frames, static_cast<long>(input_frames));

    data->input_frames_used = frames_used;
    data->output_frames_gen = frames_gen;

    return SRC_ERR_NO_ERROR;
}

// Public API implementations
extern "C" {

SRC_STATE* src_new(int converter_type, int channels, int* error) {
    if (channels < 1 || channels > 16) {
        if (error) *error = SRC_ERR_BAD_CHANNEL_COUNT;
        return nullptr;
    }

    if (converter_type < 0 || converter_type > SRC_LINEAR) {
        if (error) *error = SRC_ERR_BAD_CONVERTER;
        return nullptr;
    }

    SRC_STATE* state = new (std::nothrow) SRC_STATE_tag(converter_type, channels);
    if (!state) {
        if (error) *error = SRC_ERR_MALLOC_FAILED;
        return nullptr;
    }

    if (error) *error = SRC_ERR_NO_ERROR;
    return state;
}

SRC_STATE* src_delete(SRC_STATE* state) {
    delete state;
    return nullptr;
}

int src_process(SRC_STATE* state, SRC_DATA* data) {
    if (!state) return SRC_ERR_BAD_STATE;
    if (!data) return SRC_ERR_BAD_DATA;
    if (!data->data_in || !data->data_out) return SRC_ERR_BAD_DATA_PTR;

    if (data->src_ratio <= 0.0) return SRC_ERR_BAD_SRC_RATIO;

    state->src_ratio = data->src_ratio;

    // Use linear interpolation for all converter types (simplified)
    return linear_resample(state, data);
}

int src_simple(SRC_DATA* data, int converter_type, int channels) {
    int error;
    SRC_STATE* state = src_new(converter_type, channels, &error);
    if (!state) return error;

    int result = src_process(state, data);

    src_delete(state);
    return result;
}

const char* src_get_name(int converter_type) {
    switch (converter_type) {
        case SRC_SINC_BEST_QUALITY: return "Best Sinc Interpolator";
        case SRC_SINC_MEDIUM_QUALITY: return "Medium Sinc Interpolator";
        case SRC_SINC_FASTEST: return "Fastest Sinc Interpolator";
        case SRC_ZERO_ORDER_HOLD: return "Zero Order Hold";
        case SRC_LINEAR: return "Linear Interpolator";
        default: return nullptr;
    }
}

const char* src_get_description(int converter_type) {
    return src_get_name(converter_type);
}

const char* src_get_version(void) {
    return "Embedded libsamplerate 1.0.0";
}

int src_set_ratio(SRC_STATE* state, double new_ratio) {
    if (!state) return SRC_ERR_BAD_STATE;
    if (new_ratio <= 0.0) return SRC_ERR_BAD_SRC_RATIO;

    state->src_ratio = new_ratio;
    state->ratio_set = true;
    return SRC_ERR_NO_ERROR;
}

int src_get_channels(SRC_STATE* state) {
    if (!state) return -1;
    return state->channels;
}

int src_reset(SRC_STATE* state) {
    if (!state) return SRC_ERR_BAD_STATE;

    state->position = 0.0;
    memset(state->last_sample, 0, state->channels * sizeof(float));
    state->error = SRC_ERR_NO_ERROR;

    return SRC_ERR_NO_ERROR;
}

int src_is_valid_ratio(double ratio) {
    return (ratio >= (1.0/256.0) && ratio <= 256.0) ? SRC_TRUE : SRC_FALSE;
}

int src_error(SRC_STATE* state) {
    if (!state) return SRC_ERR_BAD_STATE;
    return state->error;
}

const char* src_strerror(int error) {
    if (error < 0 || error >= SRC_ERR_MAX_ERROR) {
        return "Unknown error.";
    }
    return error_messages[error];
}

// Callback API (simplified stubs)
SRC_STATE* src_callback_new(src_callback_t func, int converter_type,
                           int channels, int* error, void* cb_data) {
    // Simplified: just return regular converter
    return src_new(converter_type, channels, error);
}

long src_callback_read(SRC_STATE* state, double src_ratio,
                      long frames, float* data) {
    // Simplified stub
    if (!state || !data) return -1;
    return 0;
}

} // extern "C"