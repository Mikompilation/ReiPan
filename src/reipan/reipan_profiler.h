#ifndef REIPAN_PROFILER_H
#define REIPAN_PROFILER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ReiPanProfilePhase
{
    REIPAN_PROFILE_SGD_CPU = 0,
    REIPAN_PROFILE_BILLBOARD_CPU,
    REIPAN_PROFILE_BILLBOARD_HOST_BRIDGE,
    REIPAN_PROFILE_SKY_CPU,
    REIPAN_PROFILE_MAPPUT_SORT,
    REIPAN_PROFILE_MAPPUT_CALLBACK,
    REIPAN_PROFILE_ROOM_LOAD,
    REIPAN_PROFILE_ROOM_INIT,
    REIPAN_PROFILE_ROOM_FURN,
    REIPAN_PROFILE_ROOM_MODEL_INIT,
    REIPAN_PROFILE_ROOM_REGISTRATION,
    REIPAN_PROFILE_MESH_CPU,
    REIPAN_PROFILE_MESH_LIGHTING,
    REIPAN_PROFILE_MESH_SKINNING,
    REIPAN_PROFILE_GS_UPLOAD,
    REIPAN_PROFILE_TEXTURE_INVALIDATE,
    REIPAN_PROFILE_TEXTURE_MISS,
    REIPAN_PROFILE_RENDERER_UPLOAD,
    REIPAN_PROFILE_RENDERER_RECORD,
    REIPAN_PROFILE_RENDERER_ACQUIRE,
    REIPAN_PROFILE_RENDERER_SUBMIT,
    REIPAN_PROFILE_PHASE_COUNT
} ReiPanProfilePhase;

typedef enum ReiPanProfilerCounter
{
    REIPAN_PROFILER_COUNTER_BILLBOARD_REQUESTS = 0,
    REIPAN_PROFILER_COUNTER_BILLBOARD_QUEUED,
    REIPAN_PROFILER_COUNTER_BILLBOARD_REJECTED,
    REIPAN_PROFILER_COUNTER_BILLBOARD_SUPPRESSED,
    REIPAN_PROFILER_COUNTER_BILLBOARD_COMMANDS,
    REIPAN_PROFILER_COUNTER_BILLBOARD_VERTICES,
    REIPAN_PROFILER_COUNTER_BILLBOARD_STREAM_BYTES,
    REIPAN_PROFILER_COUNTER_BILLBOARD_COMPATIBLE_JOINS,
    REIPAN_PROFILER_COUNTER_BILLBOARD_TEXTURE_LOOKUPS,
    REIPAN_PROFILER_COUNTER_BILLBOARD_TEXTURE_MISSES,
    REIPAN_PROFILER_COUNTER_BILLBOARD_LEGACY_SKIPS,
    REIPAN_PROFILER_COUNTER_SKY_COMMANDS,
    REIPAN_PROFILER_COUNTER_SKY_VERTICES,
    REIPAN_PROFILER_COUNTER_SKY_STREAM_BYTES,
    REIPAN_PROFILER_COUNTER_SKY_COMPATIBLE_JOINS,
    REIPAN_PROFILER_COUNTER_SKY_POINTS_TRANSFORMED,
    REIPAN_PROFILER_COUNTER_SKY_CELLS_TESTED,
    REIPAN_PROFILER_COUNTER_SKY_VISIBLE_CELLS,
    REIPAN_PROFILER_COUNTER_SKY_HORIZON_QUADS,
    REIPAN_PROFILER_COUNTER_SKY_DOME_SUPPRESSED,
    REIPAN_PROFILER_COUNTER_SKY_HORIZON_SUPPRESSED,
    REIPAN_PROFILER_COUNTER_MAPPUT_CANDIDATES,
    REIPAN_PROFILER_COUNTER_MAPPUT_DRAWABLE,
    REIPAN_PROFILER_COUNTER_MAPPUT_COMPARISONS,
    REIPAN_PROFILER_COUNTER_MAPPUT_CALLBACKS,
    REIPAN_PROFILER_COUNTER_MAPPUT_OBJECT_DRAWS,
    REIPAN_PROFILER_COUNTER_COUNT
} ReiPanProfilerCounter;

typedef struct ReiPanProfilerCounterStats
{
    double per_frame;
    uint64_t max;
} ReiPanProfilerCounterStats;

typedef struct ReiPanProfilerStats
{
    double frame_ms;
    double frame_max_ms;
    double workload_ms;
    double workload_max_ms;
    double game_cpu_ms;
    double game_cpu_max_ms;
    double vblank_wait_ms;
    double vblank_wait_max_ms;
    double sgd_cpu_ms;
    double sgd_cpu_max_ms;
    double billboard_cpu_ms;
    double billboard_cpu_max_ms;
    double billboard_host_bridge_ms;
    double billboard_host_bridge_max_ms;
    double sky_cpu_ms;
    double sky_cpu_max_ms;
    double mapput_sort_ms;
    double mapput_sort_max_ms;
    double mapput_callback_ms;
    double mapput_callback_max_ms;
    double room_load_ms;
    double room_load_max_ms;
    double room_init_ms;
    double room_init_max_ms;
    double room_furn_ms;
    double room_furn_max_ms;
    double room_model_init_ms;
    double room_model_init_max_ms;
    double room_registration_ms;
    double room_registration_max_ms;
    double mesh_cpu_ms;
    double mesh_cpu_max_ms;
    double mesh_lighting_ms;
    double mesh_lighting_max_ms;
    double mesh_skinning_ms;
    double mesh_skinning_max_ms;
    double gs_upload_ms;
    double gs_upload_max_ms;
    double texture_invalidate_ms;
    double texture_invalidate_max_ms;
    double texture_miss_ms;
    double texture_miss_max_ms;
    double renderer_ms;
    double renderer_max_ms;
    double renderer_upload_ms;
    double renderer_upload_max_ms;
    double renderer_record_ms;
    double renderer_record_max_ms;
    double renderer_acquire_ms;
    double renderer_acquire_max_ms;
    double renderer_submit_ms;
    double renderer_submit_max_ms;
    double gpu_idle_wait_ms;
    double gpu_idle_wait_max_ms;
    int gpu_idle_waits;
    int swapchain_acquire_attempts;
    int swapchain_unavailable;
    int vblank_drained;
    int vblank_wait_calls;
    int vblank_deadline_misses;
    int vblank_target;
    ReiPanProfilerCounterStats billboard_requests;
    ReiPanProfilerCounterStats billboard_queued;
    ReiPanProfilerCounterStats billboard_rejected;
    ReiPanProfilerCounterStats billboard_suppressed;
    ReiPanProfilerCounterStats billboard_commands;
    ReiPanProfilerCounterStats billboard_vertices;
    ReiPanProfilerCounterStats billboard_stream_bytes;
    ReiPanProfilerCounterStats billboard_compatible_joins;
    ReiPanProfilerCounterStats billboard_texture_lookups;
    ReiPanProfilerCounterStats billboard_texture_misses;
    ReiPanProfilerCounterStats billboard_legacy_skips;
    ReiPanProfilerCounterStats sky_commands;
    ReiPanProfilerCounterStats sky_vertices;
    ReiPanProfilerCounterStats sky_stream_bytes;
    ReiPanProfilerCounterStats sky_compatible_joins;
    ReiPanProfilerCounterStats sky_points_transformed;
    ReiPanProfilerCounterStats sky_cells_tested;
    ReiPanProfilerCounterStats sky_visible_cells;
    ReiPanProfilerCounterStats sky_horizon_quads;
    ReiPanProfilerCounterStats sky_dome_suppressed;
    ReiPanProfilerCounterStats sky_horizon_suppressed;
    ReiPanProfilerCounterStats mapput_candidates;
    ReiPanProfilerCounterStats mapput_drawable;
    ReiPanProfilerCounterStats mapput_comparisons;
    ReiPanProfilerCounterStats mapput_callbacks;
    ReiPanProfilerCounterStats mapput_object_draws;
} ReiPanProfilerStats;

/* The profiler follows the renderer/game loop and is intentionally
 * main-thread-only. */
void ReiPan_ProfilerReset(void);
void ReiPan_ProfilerBeginFrame(void);
void ReiPan_ProfilerBeginRenderer(void);
void ReiPan_ProfilerEndFrame(void);

uint64_t ReiPan_ProfilerNow(void);
uint64_t ReiPan_ProfilerBeginPhase(ReiPanProfilePhase phase);
void ReiPan_ProfilerEndPhase(ReiPanProfilePhase phase,
                             uint64_t start_counter);
void ReiPan_ProfilerAddCounter(ReiPanProfilerCounter counter,
                               uint64_t amount);

/* TEMPORARY DIAGNOSTIC -- stutter attribution.
 *
 * Set REIPAN_STUTTER_MS in the environment to a millisecond threshold; every
 * logical frame whose wall time exceeds it appends one fully attributed line
 * to reipan_stutter.log.  The renderer publishes its per-frame cache/texture
 * counters through ReiPan_ProfilerSetFrameNote() so that the dump can say
 * *which* cold work the slow frame did, not just how long it took.  Costs one
 * comparison per frame when the variable is unset. */
void ReiPan_ProfilerSetFrameNote(const char *text);

void ReiPan_ProfilerRecordGpuIdleWait(uint64_t wait_ticks);

/* Time the renderer spent deliberately idle, spacing the presents of one
 * logical frame a CRTC field apart.  Subtracted from the renderer total the
 * same way the V-blank wait is subtracted from the game's, so `renderer_ms`
 * keeps meaning work done and the workload budget stays a budget. */
void ReiPan_ProfilerRecordPresentWait(uint64_t wait_ticks);
void ReiPan_ProfilerRecordSwapchainAcquire(int unavailable);
void ReiPan_ProfilerRecordVblankPacing(uint64_t wait_ticks,
                                       int drained_vblanks,
                                       int wait_calls,
                                       int target_vblanks);

/* Converts the accumulated window to milliseconds per logical frame and
 * resets it.  Maxima are the worst individual logical frame in the window. */
void ReiPan_ProfilerConsumeStats(ReiPanProfilerStats *out);

#ifdef __cplusplus
}

class ReiPanProfileScope
{
public:
    explicit ReiPanProfileScope(ReiPanProfilePhase phase,
                                bool enabled = true)
        : phase_(phase),
          start_(enabled ? ReiPan_ProfilerBeginPhase(phase) : 0)
    {
    }

    ~ReiPanProfileScope()
    {
        if (start_ != 0)
        {
            ReiPan_ProfilerEndPhase(phase_, start_);
        }
    }

    ReiPanProfileScope(const ReiPanProfileScope &) = delete;
    ReiPanProfileScope &operator=(const ReiPanProfileScope &) = delete;

private:
    ReiPanProfilePhase phase_;
    uint64_t start_;
};
#endif

#endif
