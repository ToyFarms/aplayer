#include "libprocessing.h"

static const float stage1_coeff_a[] = {
    1.0f,
    -1.69065929318241f,
    0.73248077421585f,
};

static const float stage1_coeff_b[] = {
    1.53512485958697f,
    -2.69169618940638f,
    1.19839281085285f,
};

static const float stage2_coeff_a[] = {
    1.0f,
    -1.99004745483398f,
    0.99007225036621f,
};

static const float stage2_coeff_b[] = {
    1.0f,
    -2.0f,
    1.0f,
};

static void lfilter(const float *b, int b_size, const float *a, int a_size, const float *x, int x_size, float **out_y)
{
    for (int i = 0; i < x_size; i++)
    {
        for (int j = 0; j < b_size && i - j >= 0; j++)
            (*out_y)[i] += x[i - j] * b[j];

        for (int j = 1; j < a_size && i - j >= 0; j++)
            (*out_y)[i] -= (*out_y)[i - j] * a[j];

        if (a_size > 0 && a[0] != 0.0f)
            (*out_y)[i] /= a[0];
    }
}

static const float channel_gains[] = {1.0f, 1.0f, 1.0f, 1.41f, 1.41f};
static const float abs_threshold = -70.0f;
static const float block_overlap = 0.75f;
static const float block_step = 1.0f - block_overlap;

float calculate_loudness(float *samples,
                         int num_channel,
                         int num_sample,
                         int sample_rate,
                         float block_size_ms)
{
    /*
    reference:
        - https://github.com/csteinmetz1/pyloudnorm/blob/master/pyloudnorm/meter.py
        - https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.lfilter.html
        - https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf
    */
    // TODO: Make this more efficient

    float block_size_s = block_size_ms / 1000.0f;

    if (num_sample < block_size_s * sample_rate)
        return -INFINITY;

    float *samples_copy = (float *)malloc(num_sample * num_channel * sizeof(float));

    float *filtered = (float *)malloc(num_sample * sizeof(float));
    for (int ch = 0; ch < num_channel; ch++)
    {
        lfilter(stage1_coeff_b, 3, stage1_coeff_a, 3, samples + (ch * num_sample), num_sample, &filtered);
        memcpy(samples_copy + (ch * num_sample), filtered, num_sample);

        lfilter(stage2_coeff_b, 3, stage2_coeff_a, 3, samples + (ch * num_sample), num_sample, &filtered);
        memcpy(samples_copy + (ch * num_sample), filtered, num_sample);
    }

    float sample_len_secs = (float)num_sample / (float)sample_rate;
    int block_len = (int)roundf(((sample_len_secs - block_size_s) / (block_size_s * block_step)) + 1);
    float *block_buffer = (float *)malloc(block_len * num_channel * sizeof(float));

    for (int ch = 0; ch < num_channel; ch++)
    {
        for (int block_idx = 0; block_idx < block_len; block_idx++)
        {
            int lower_bound = (int)(block_size_s * (block_idx * block_step) * sample_rate);
            int upper_bound = (int)(block_size_s * (block_idx * block_step + 1) * sample_rate);

            float square_sum = 0.0f;

            for (int sample = lower_bound; sample < upper_bound; sample++)
            {
                float sample_value = samples_copy[(ch * num_sample) + sample];
                square_sum += sample_value * sample_value;
            }

            block_buffer[(ch * block_len) + block_idx] = (1.0f / (block_size_s * sample_rate)) * square_sum;
        }
    }

    free(samples_copy);

    float *LKFS_block = (float *)malloc(block_len * sizeof(float));

    for (int block_idx = 0; block_idx < block_len; block_idx++)
    {
        float weighted_block = 0.0f;
        for (int ch = 0; ch < num_channel; ch++)
        {
            weighted_block += channel_gains[ch] * block_buffer[(ch * block_len) + block_idx];
        }
        LKFS_block[block_idx] = -0.691f + 10.0f * log10f(weighted_block);
    }

    float *LKFS_ch_gated_avg = (float *)malloc(num_channel * sizeof(float));

    for (int ch = 0; ch < num_channel; ch++)
    {
        float sum = 0.0f;
        int added = 0;
        for (int block_idx = 0; block_idx < block_len; block_idx++)
        {
            if (LKFS_block[block_idx] >= abs_threshold)
            {
                sum += block_buffer[(ch * block_len) + block_idx];
                added++;
            }
        }
        LKFS_ch_gated_avg[ch] = added > 0 ? sum / (float)added : 0.0f;
    }

    float LKFS_sum = 0.0f;
    for (int ch = 0; ch < num_channel; ch++)
    {
        LKFS_sum += channel_gains[ch] * LKFS_ch_gated_avg[ch];
    }
    float relative_threshold = -0.691f + 10.0f * log10f(LKFS_sum) - 10.0f;

    for (int ch = 0; ch < num_channel; ch++)
    {
        float sum = 0.0f;
        int added = 0;
        for (int block_idx = 0; block_idx < block_len; block_idx++)
        {
            if (LKFS_block[block_idx] > relative_threshold && LKFS_block[block_idx] > abs_threshold)
            {
                sum += block_buffer[(ch * block_len) + block_idx];
                added++;
            }
        }
        LKFS_ch_gated_avg[ch] = added > 0 ? sum / (float)added : 0.0f;
    }

    LKFS_sum = 0.0f;
    for (int ch = 0; ch < num_channel; ch++)
    {
        LKFS_sum += channel_gains[ch] * LKFS_ch_gated_avg[ch];
    }
    float LUFS = -0.691f + 10.0f * log10f(LKFS_sum);

    free(block_buffer);
    free(LKFS_block);
    free(LKFS_ch_gated_avg);

    return LUFS;
}