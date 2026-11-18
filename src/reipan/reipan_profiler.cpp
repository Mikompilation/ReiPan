#include "reipan_profiler.h"

#include "io/reipan_paths.h"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
{
enum ProfileMetric
{
    PROFILE_METRIC_FRAME = 0,
    PROFILE_METRIC_WORKLOAD,
    PROFILE_METRIC_GAME_CPU,
    PROFILE_METRIC_VBLANK_WAIT,
    PROFILE_METRIC_SGD_CPU,
    PROFILE_METRIC_BILLBOARD_CPU,
    PROFILE_METRIC_BILLBOARD_HOST_BRIDGE,
    PROFILE_METRIC_SKY_CPU,
    PROFILE_METRIC_MAPPUT_SORT,
    PROFILE_METRIC_MAPPUT_CALLBACK,
    PROFILE_METRIC_ROOM_LOAD,
    PROFILE_METRIC_ROOM_INIT,
    PROFILE_METRIC_ROOM_FURN,
    PROFILE_METRIC_ROOM_MODEL_INIT,
    PROFILE_METRIC_ROOM_REGISTRATION,
    PROFILE_METRIC_MESH_CPU,
    PROFILE_METRIC_MESH_LIGHTING,
    PROFILE_METRIC_MESH_SKINNING,
    PROFILE_METRIC_GS_UPLOAD,
    PROFILE_METRIC_TEXTURE_INVALIDATE,
    PROFILE_METRIC_TEXTURE_MISS,
    PROFILE_METRIC_RENDERER,
    PROFILE_METRIC_RENDERER_UPLOAD,
    PROFILE_METRIC_RENDERER_RECORD,
    PROFILE_METRIC_RENDERER_ACQUIRE,
    PROFILE_METRIC_RENDERER_SUBMIT,
    PROFILE_METRIC_GPU_IDLE_WAIT,
    PROFILE_METRIC_COUNT
};

struct ProfileFrame
{
    uint64_t start_counter = 0;
    uint64_t renderer_start_counter = 0;
    uint64_t phase_ticks[REIPAN_PROFILE_PHASE_COUNT]{};
    uint64_t counters[REIPAN_PROFILER_COUNTER_COUNT]{};
    uint64_t vblank_wait_ticks = 0;
    uint64_t gpu_idle_wait_ticks = 0;
    uint64_t present_wait_ticks = 0;
    int gpu_idle_waits = 0;
    int swapchain_acquire_attempts = 0;
    int swapchain_unavailable = 0;
    int vblank_drained = 0;
    int vblank_wait_calls = 0;
    int vblank_deadline_misses = 0;
    int vblank_target = 0;
};

struct ProfileMetricWindow
{
    uint64_t total_ticks = 0;
    uint64_t max_ticks = 0;
};

struct ProfileCounterWindow
{
    uint64_t total = 0;
    uint64_t max = 0;
};

struct ProfileWindow
{
    ProfileMetricWindow metrics[PROFILE_METRIC_COUNT]{};
    ProfileCounterWindow counters[REIPAN_PROFILER_COUNTER_COUNT]{};
    int frame_count = 0;
    int gpu_idle_waits = 0;
    int swapchain_acquire_attempts = 0;
    int swapchain_unavailable = 0;
    int vblank_drained = 0;
    int vblank_wait_calls = 0;
    int vblank_deadline_misses = 0;
    int vblank_target = 0;
};

ProfileFrame g_frame;
ProfileWindow g_window;
uint64_t g_next_frame_start;

/* TEMPORARY DIAGNOSTIC -- stutter attribution.  See reipan_profiler.h. */
char g_frame_note[512];
double g_stutter_threshold_ms = -1.0;
uint64_t g_stutter_frame_index;

double StutterThresholdMs()
{
    if (g_stutter_threshold_ms < 0.0)
    {
        const char *value = SDL_getenv("REIPAN_STUTTER_MS");
        g_stutter_threshold_ms = value != nullptr ? SDL_atof(value) : 0.0;
        if (g_stutter_threshold_ms < 0.0)
        {
            g_stutter_threshold_ms = 0.0;
        }
    }
    return g_stutter_threshold_ms;
}

void ReportStutterFrame(uint64_t frame_ticks, uint64_t workload_ticks,
                        uint64_t frequency)
{
    const double threshold = StutterThresholdMs();
    g_stutter_frame_index++;
    if (threshold <= 0.0 || frequency == 0)
    {
        return;
    }
    /* Threshold on the workload, not the paced frame time: the loop already
     * sleeps to a 30 Hz vblank, so every frame's wall time is ~33 ms and would
     * trip any useful threshold. */
    const double frame_ms =
        (double)frame_ticks * 1000.0 / (double)frequency;
    const double workload_ms =
        (double)workload_ticks * 1000.0 / (double)frequency;
    if (workload_ms < threshold)
    {
        return;
    }

    const double scale = 1000.0 / (double)frequency;

    /* In the user directory, not the working directory: the latter is
     * whatever the launcher, IDE or debugger happened to set, so the log
     * used to appear somewhere different depending on how the game was
     * started -- and nowhere writable at all once it is installed. */
    char path[1024];
    if (ReiPan_PathPrepareUser("reipan_stutter.log", path, sizeof(path)) == 0)
    {
        return;
    }

    FILE *fp = std::fopen(path, "a");
    if (fp == nullptr)
    {
        return;
    }
    std::fprintf(fp,
                 "frame %llu  work %8.2f  paced %8.2f  "
                 "sgd %7.2f  meshcpu %7.2f  meshlit %7.2f  meshskin %7.2f  "
                 "texmiss %7.2f  gsupload %7.2f  "
                 "roomload %7.2f  roominit %7.2f  roomfurn %7.2f  "
                 "roommdl %7.2f  roomreg %7.2f  "
                 "rupload %7.2f  rrecord %7.2f  racquire %7.2f  "
                 "rsubmit %7.2f  gpuidle %7.2f  pwait %7.2f | %s\n",
                 (unsigned long long)g_stutter_frame_index, workload_ms,
                 frame_ms,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_SGD_CPU] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_MESH_CPU] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_MESH_LIGHTING] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_MESH_SKINNING] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_TEXTURE_MISS] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_GS_UPLOAD] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_ROOM_LOAD] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_ROOM_INIT] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_ROOM_FURN] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_ROOM_MODEL_INIT] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_ROOM_REGISTRATION] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_RENDERER_UPLOAD] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_RENDERER_RECORD] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_RENDERER_ACQUIRE] * scale,
                 (double)g_frame.phase_ticks[REIPAN_PROFILE_RENDERER_SUBMIT] * scale,
                 (double)g_frame.gpu_idle_wait_ticks * scale,
                 (double)g_frame.present_wait_ticks * scale,
                 g_frame_note);
    std::fclose(fp);
}

bool ValidPhase(ReiPanProfilePhase phase)
{
    return (int)phase >= 0 && (int)phase < REIPAN_PROFILE_PHASE_COUNT;
}

bool ValidCounter(ReiPanProfilerCounter counter)
{
    return (int)counter >= 0 &&
           (int)counter < REIPAN_PROFILER_COUNTER_COUNT;
}

void AddPhaseTicks(ReiPanProfilePhase phase, uint64_t ticks)
{
    if (g_frame.start_counter != 0 && ValidPhase(phase))
    {
        g_frame.phase_ticks[(int)phase] += ticks;
    }
}

void AddMetric(ProfileMetric metric, uint64_t ticks)
{
    ProfileMetricWindow &window = g_window.metrics[(int)metric];
    window.total_ticks += ticks;
    window.max_ticks = std::max(window.max_ticks, ticks);
}

double AverageMs(ProfileMetric metric, uint64_t frequency)
{
    if (frequency == 0 || g_window.frame_count <= 0)
    {
        return 0.0;
    }
    return (double)g_window.metrics[(int)metric].total_ticks * 1000.0 /
           ((double)frequency * (double)g_window.frame_count);
}

double MaxMs(ProfileMetric metric, uint64_t frequency)
{
    if (frequency == 0)
    {
        return 0.0;
    }
    return (double)g_window.metrics[(int)metric].max_ticks * 1000.0 /
           (double)frequency;
}

double CounterPerFrame(ReiPanProfilerCounter counter)
{
    if (g_window.frame_count <= 0 || !ValidCounter(counter))
    {
        return 0.0;
    }
    return (double)g_window.counters[(int)counter].total /
           (double)g_window.frame_count;
}

uint64_t CounterMax(ReiPanProfilerCounter counter)
{
    return ValidCounter(counter)
        ? g_window.counters[(int)counter].max : 0;
}

void RecordFrame(uint64_t frame_end)
{
    const uint64_t renderer_start = g_frame.renderer_start_counter;
    if (g_frame.start_counter == 0 ||
        renderer_start < g_frame.start_counter || frame_end < renderer_start)
    {
        g_frame = {};
        return;
    }

    const uint64_t vblank_ticks = g_frame.vblank_wait_ticks;
    const uint64_t pre_renderer_ticks =
        renderer_start - g_frame.start_counter;
    const uint64_t game_cpu_ticks = pre_renderer_ticks > vblank_ticks
        ? pre_renderer_ticks - vblank_ticks : 0;
    /* The frame-smoothing waits are wall time the renderer chose to spend
     * doing nothing, so they are not renderer work.  Same treatment as the
     * V-blank wait above. */
    const uint64_t renderer_wall_ticks = frame_end - renderer_start;
    const uint64_t renderer_ticks =
        renderer_wall_ticks > g_frame.present_wait_ticks
            ? renderer_wall_ticks - g_frame.present_wait_ticks
            : 0;

    AddMetric(PROFILE_METRIC_FRAME, frame_end - g_frame.start_counter);
    AddMetric(PROFILE_METRIC_WORKLOAD, game_cpu_ticks + renderer_ticks);
    AddMetric(PROFILE_METRIC_GAME_CPU, game_cpu_ticks);
    AddMetric(PROFILE_METRIC_VBLANK_WAIT, vblank_ticks);
    AddMetric(PROFILE_METRIC_SGD_CPU,
              g_frame.phase_ticks[REIPAN_PROFILE_SGD_CPU]);
    AddMetric(PROFILE_METRIC_BILLBOARD_CPU,
              g_frame.phase_ticks[REIPAN_PROFILE_BILLBOARD_CPU]);
    AddMetric(PROFILE_METRIC_BILLBOARD_HOST_BRIDGE,
              g_frame.phase_ticks[REIPAN_PROFILE_BILLBOARD_HOST_BRIDGE]);
    AddMetric(PROFILE_METRIC_SKY_CPU,
              g_frame.phase_ticks[REIPAN_PROFILE_SKY_CPU]);
    AddMetric(PROFILE_METRIC_MAPPUT_SORT,
              g_frame.phase_ticks[REIPAN_PROFILE_MAPPUT_SORT]);
    AddMetric(PROFILE_METRIC_MAPPUT_CALLBACK,
              g_frame.phase_ticks[REIPAN_PROFILE_MAPPUT_CALLBACK]);
    AddMetric(PROFILE_METRIC_ROOM_LOAD,
              g_frame.phase_ticks[REIPAN_PROFILE_ROOM_LOAD]);
    AddMetric(PROFILE_METRIC_ROOM_INIT,
              g_frame.phase_ticks[REIPAN_PROFILE_ROOM_INIT]);
    AddMetric(PROFILE_METRIC_ROOM_FURN,
              g_frame.phase_ticks[REIPAN_PROFILE_ROOM_FURN]);
    AddMetric(PROFILE_METRIC_ROOM_MODEL_INIT,
              g_frame.phase_ticks[REIPAN_PROFILE_ROOM_MODEL_INIT]);
    AddMetric(PROFILE_METRIC_ROOM_REGISTRATION,
              g_frame.phase_ticks[REIPAN_PROFILE_ROOM_REGISTRATION]);
    AddMetric(PROFILE_METRIC_MESH_CPU,
              g_frame.phase_ticks[REIPAN_PROFILE_MESH_CPU]);
    AddMetric(PROFILE_METRIC_MESH_LIGHTING,
              g_frame.phase_ticks[REIPAN_PROFILE_MESH_LIGHTING]);
    AddMetric(PROFILE_METRIC_MESH_SKINNING,
              g_frame.phase_ticks[REIPAN_PROFILE_MESH_SKINNING]);
    AddMetric(PROFILE_METRIC_GS_UPLOAD,
              g_frame.phase_ticks[REIPAN_PROFILE_GS_UPLOAD]);
    AddMetric(PROFILE_METRIC_TEXTURE_INVALIDATE,
              g_frame.phase_ticks[REIPAN_PROFILE_TEXTURE_INVALIDATE]);
    AddMetric(PROFILE_METRIC_TEXTURE_MISS,
              g_frame.phase_ticks[REIPAN_PROFILE_TEXTURE_MISS]);
    AddMetric(PROFILE_METRIC_RENDERER, renderer_ticks);
    AddMetric(PROFILE_METRIC_RENDERER_UPLOAD,
              g_frame.phase_ticks[REIPAN_PROFILE_RENDERER_UPLOAD]);
    AddMetric(PROFILE_METRIC_RENDERER_RECORD,
              g_frame.phase_ticks[REIPAN_PROFILE_RENDERER_RECORD]);
    AddMetric(PROFILE_METRIC_RENDERER_ACQUIRE,
              g_frame.phase_ticks[REIPAN_PROFILE_RENDERER_ACQUIRE]);
    AddMetric(PROFILE_METRIC_RENDERER_SUBMIT,
              g_frame.phase_ticks[REIPAN_PROFILE_RENDERER_SUBMIT]);
    AddMetric(PROFILE_METRIC_GPU_IDLE_WAIT, g_frame.gpu_idle_wait_ticks);

    for (int i = 0; i < REIPAN_PROFILER_COUNTER_COUNT; i++)
    {
        ProfileCounterWindow &counter = g_window.counters[i];
        const uint64_t value = g_frame.counters[i];
        counter.total += value;
        counter.max = std::max(counter.max, value);
    }

    g_window.frame_count++;
    g_window.gpu_idle_waits += g_frame.gpu_idle_waits;
    g_window.swapchain_acquire_attempts +=
        g_frame.swapchain_acquire_attempts;
    g_window.swapchain_unavailable += g_frame.swapchain_unavailable;
    g_window.vblank_drained += g_frame.vblank_drained;
    g_window.vblank_wait_calls += g_frame.vblank_wait_calls;
    g_window.vblank_deadline_misses += g_frame.vblank_deadline_misses;
    g_window.vblank_target = g_frame.vblank_target;
    /* TEMPORARY DIAGNOSTIC -- must read g_frame before it is cleared. */
    ReportStutterFrame(frame_end - g_frame.start_counter,
                       game_cpu_ticks + renderer_ticks,
                       (uint64_t)SDL_GetPerformanceFrequency());
    g_frame = {};
}

#define REIPAN_PROFILE_STATS(name, phase)             \
    out->name##_ms = AverageMs(phase, frequency);     \
    out->name##_max_ms = MaxMs(phase, frequency)

#define REIPAN_PROFILE_COUNTER_STATS(name, counter)                 \
    out->name.per_frame = CounterPerFrame(counter);                 \
    out->name.max = CounterMax(counter)
}

extern "C" {

void ReiPan_ProfilerReset(void)
{
    g_frame = {};
    g_window = {};
    g_next_frame_start = 0;
}

void ReiPan_ProfilerBeginFrame(void)
{
    g_frame = {};
    g_frame.start_counter = g_next_frame_start != 0
        ? g_next_frame_start : ReiPan_ProfilerNow();
    g_next_frame_start = 0;
}

void ReiPan_ProfilerBeginRenderer(void)
{
    if (g_frame.start_counter != 0 &&
        g_frame.renderer_start_counter == 0)
    {
        g_frame.renderer_start_counter = ReiPan_ProfilerNow();
    }
}

void ReiPan_ProfilerEndFrame(void)
{
    const uint64_t frame_end = ReiPan_ProfilerNow();
    RecordFrame(frame_end);

    /* Charge phase-tree cleanup after EndFrame and setup before the following
     * BeginFrame to the next logical frame instead of dropping that work. */
    g_next_frame_start = frame_end;
}

/* TEMPORARY DIAGNOSTIC -- stutter attribution.  See reipan_profiler.h. */
void ReiPan_ProfilerSetFrameNote(const char *text)
{
    if (text == nullptr)
    {
        g_frame_note[0] = '\0';
        return;
    }
    SDL_strlcpy(g_frame_note, text, sizeof(g_frame_note));
}

uint64_t ReiPan_ProfilerNow(void)
{
    return (uint64_t)SDL_GetPerformanceCounter();
}

uint64_t ReiPan_ProfilerBeginPhase(ReiPanProfilePhase phase)
{
    if (g_frame.start_counter == 0 || !ValidPhase(phase))
    {
        return 0;
    }
    return ReiPan_ProfilerNow();
}

void ReiPan_ProfilerEndPhase(ReiPanProfilePhase phase,
                             uint64_t start_counter)
{
    if (start_counter == 0 || g_frame.start_counter == 0 ||
        start_counter < g_frame.start_counter || !ValidPhase(phase))
    {
        return;
    }
    const uint64_t end_counter = ReiPan_ProfilerNow();
    if (end_counter >= start_counter)
    {
        AddPhaseTicks(phase, end_counter - start_counter);
    }
}

void ReiPan_ProfilerAddCounter(ReiPanProfilerCounter counter,
                               uint64_t amount)
{
    if (g_frame.start_counter != 0 && ValidCounter(counter))
    {
        g_frame.counters[(int)counter] += amount;
    }
}

void ReiPan_ProfilerRecordGpuIdleWait(uint64_t wait_ticks)
{
    if (g_frame.start_counter != 0)
    {
        g_frame.gpu_idle_wait_ticks += wait_ticks;
        g_frame.gpu_idle_waits++;
    }
}

void ReiPan_ProfilerRecordPresentWait(uint64_t wait_ticks)
{
    if (g_frame.start_counter != 0)
    {
        g_frame.present_wait_ticks += wait_ticks;
    }
}

void ReiPan_ProfilerRecordSwapchainAcquire(int unavailable)
{
    if (g_frame.start_counter != 0)
    {
        g_frame.swapchain_acquire_attempts++;
        if (unavailable)
        {
            g_frame.swapchain_unavailable++;
        }
    }
}

void ReiPan_ProfilerRecordVblankPacing(uint64_t wait_ticks,
                                       int drained_vblanks,
                                       int wait_calls,
                                       int target_vblanks)
{
    if (g_frame.start_counter == 0)
    {
        return;
    }
    g_frame.vblank_wait_ticks += wait_ticks;
    g_frame.vblank_drained += std::max(0, drained_vblanks);
    g_frame.vblank_wait_calls += std::max(0, wait_calls);
    g_frame.vblank_target = std::max(0, target_vblanks);
    if (target_vblanks > 0 && drained_vblanks >= target_vblanks)
    {
        g_frame.vblank_deadline_misses++;
    }
}

void ReiPan_ProfilerConsumeStats(ReiPanProfilerStats *out)
{
    if (out == nullptr)
    {
        return;
    }

    std::memset(out, 0, sizeof(*out));
    const uint64_t frequency = (uint64_t)SDL_GetPerformanceFrequency();
    REIPAN_PROFILE_STATS(frame, PROFILE_METRIC_FRAME);
    REIPAN_PROFILE_STATS(workload, PROFILE_METRIC_WORKLOAD);
    REIPAN_PROFILE_STATS(game_cpu, PROFILE_METRIC_GAME_CPU);
    REIPAN_PROFILE_STATS(vblank_wait, PROFILE_METRIC_VBLANK_WAIT);
    REIPAN_PROFILE_STATS(sgd_cpu, PROFILE_METRIC_SGD_CPU);
    REIPAN_PROFILE_STATS(billboard_cpu, PROFILE_METRIC_BILLBOARD_CPU);
    REIPAN_PROFILE_STATS(billboard_host_bridge,
                         PROFILE_METRIC_BILLBOARD_HOST_BRIDGE);
    REIPAN_PROFILE_STATS(sky_cpu, PROFILE_METRIC_SKY_CPU);
    REIPAN_PROFILE_STATS(mapput_sort, PROFILE_METRIC_MAPPUT_SORT);
    REIPAN_PROFILE_STATS(mapput_callback,
                         PROFILE_METRIC_MAPPUT_CALLBACK);
    REIPAN_PROFILE_STATS(room_load, PROFILE_METRIC_ROOM_LOAD);
    REIPAN_PROFILE_STATS(room_init, PROFILE_METRIC_ROOM_INIT);
    REIPAN_PROFILE_STATS(room_furn, PROFILE_METRIC_ROOM_FURN);
    REIPAN_PROFILE_STATS(room_model_init,
                         PROFILE_METRIC_ROOM_MODEL_INIT);
    REIPAN_PROFILE_STATS(room_registration,
                         PROFILE_METRIC_ROOM_REGISTRATION);
    REIPAN_PROFILE_STATS(mesh_cpu, PROFILE_METRIC_MESH_CPU);
    REIPAN_PROFILE_STATS(mesh_lighting, PROFILE_METRIC_MESH_LIGHTING);
    REIPAN_PROFILE_STATS(mesh_skinning, PROFILE_METRIC_MESH_SKINNING);
    REIPAN_PROFILE_STATS(gs_upload, PROFILE_METRIC_GS_UPLOAD);
    REIPAN_PROFILE_STATS(texture_invalidate,
                         PROFILE_METRIC_TEXTURE_INVALIDATE);
    REIPAN_PROFILE_STATS(texture_miss, PROFILE_METRIC_TEXTURE_MISS);
    REIPAN_PROFILE_STATS(renderer, PROFILE_METRIC_RENDERER);
    REIPAN_PROFILE_STATS(renderer_upload, PROFILE_METRIC_RENDERER_UPLOAD);
    REIPAN_PROFILE_STATS(renderer_record, PROFILE_METRIC_RENDERER_RECORD);
    REIPAN_PROFILE_STATS(renderer_acquire, PROFILE_METRIC_RENDERER_ACQUIRE);
    REIPAN_PROFILE_STATS(renderer_submit, PROFILE_METRIC_RENDERER_SUBMIT);
    REIPAN_PROFILE_STATS(gpu_idle_wait, PROFILE_METRIC_GPU_IDLE_WAIT);

    out->gpu_idle_waits = g_window.gpu_idle_waits;
    out->swapchain_acquire_attempts =
        g_window.swapchain_acquire_attempts;
    out->swapchain_unavailable = g_window.swapchain_unavailable;
    out->vblank_drained = g_window.vblank_drained;
    out->vblank_wait_calls = g_window.vblank_wait_calls;
    out->vblank_deadline_misses = g_window.vblank_deadline_misses;
    out->vblank_target = g_window.vblank_target;
    REIPAN_PROFILE_COUNTER_STATS(
        billboard_requests,
        REIPAN_PROFILER_COUNTER_BILLBOARD_REQUESTS);
    REIPAN_PROFILE_COUNTER_STATS(
        billboard_queued,
        REIPAN_PROFILER_COUNTER_BILLBOARD_QUEUED);
    REIPAN_PROFILE_COUNTER_STATS(
        billboard_rejected,
        REIPAN_PROFILER_COUNTER_BILLBOARD_REJECTED);
    REIPAN_PROFILE_COUNTER_STATS(
        billboard_suppressed,
        REIPAN_PROFILER_COUNTER_BILLBOARD_SUPPRESSED);
    REIPAN_PROFILE_COUNTER_STATS(
        billboard_commands,
        REIPAN_PROFILER_COUNTER_BILLBOARD_COMMANDS);
    REIPAN_PROFILE_COUNTER_STATS(
        billboard_vertices,
        REIPAN_PROFILER_COUNTER_BILLBOARD_VERTICES);
    REIPAN_PROFILE_COUNTER_STATS(
        billboard_stream_bytes,
        REIPAN_PROFILER_COUNTER_BILLBOARD_STREAM_BYTES);
    REIPAN_PROFILE_COUNTER_STATS(
        billboard_compatible_joins,
        REIPAN_PROFILER_COUNTER_BILLBOARD_COMPATIBLE_JOINS);
    REIPAN_PROFILE_COUNTER_STATS(
        billboard_texture_lookups,
        REIPAN_PROFILER_COUNTER_BILLBOARD_TEXTURE_LOOKUPS);
    REIPAN_PROFILE_COUNTER_STATS(
        billboard_texture_misses,
        REIPAN_PROFILER_COUNTER_BILLBOARD_TEXTURE_MISSES);
    REIPAN_PROFILE_COUNTER_STATS(
        billboard_legacy_skips,
        REIPAN_PROFILER_COUNTER_BILLBOARD_LEGACY_SKIPS);
    REIPAN_PROFILE_COUNTER_STATS(
        sky_commands,
        REIPAN_PROFILER_COUNTER_SKY_COMMANDS);
    REIPAN_PROFILE_COUNTER_STATS(
        sky_vertices,
        REIPAN_PROFILER_COUNTER_SKY_VERTICES);
    REIPAN_PROFILE_COUNTER_STATS(
        sky_stream_bytes,
        REIPAN_PROFILER_COUNTER_SKY_STREAM_BYTES);
    REIPAN_PROFILE_COUNTER_STATS(
        sky_compatible_joins,
        REIPAN_PROFILER_COUNTER_SKY_COMPATIBLE_JOINS);
    REIPAN_PROFILE_COUNTER_STATS(
        sky_points_transformed,
        REIPAN_PROFILER_COUNTER_SKY_POINTS_TRANSFORMED);
    REIPAN_PROFILE_COUNTER_STATS(
        sky_cells_tested,
        REIPAN_PROFILER_COUNTER_SKY_CELLS_TESTED);
    REIPAN_PROFILE_COUNTER_STATS(
        sky_visible_cells,
        REIPAN_PROFILER_COUNTER_SKY_VISIBLE_CELLS);
    REIPAN_PROFILE_COUNTER_STATS(
        sky_horizon_quads,
        REIPAN_PROFILER_COUNTER_SKY_HORIZON_QUADS);
    REIPAN_PROFILE_COUNTER_STATS(
        sky_dome_suppressed,
        REIPAN_PROFILER_COUNTER_SKY_DOME_SUPPRESSED);
    REIPAN_PROFILE_COUNTER_STATS(
        sky_horizon_suppressed,
        REIPAN_PROFILER_COUNTER_SKY_HORIZON_SUPPRESSED);
    REIPAN_PROFILE_COUNTER_STATS(
        mapput_candidates,
        REIPAN_PROFILER_COUNTER_MAPPUT_CANDIDATES);
    REIPAN_PROFILE_COUNTER_STATS(
        mapput_drawable,
        REIPAN_PROFILER_COUNTER_MAPPUT_DRAWABLE);
    REIPAN_PROFILE_COUNTER_STATS(
        mapput_comparisons,
        REIPAN_PROFILER_COUNTER_MAPPUT_COMPARISONS);
    REIPAN_PROFILE_COUNTER_STATS(
        mapput_callbacks,
        REIPAN_PROFILER_COUNTER_MAPPUT_CALLBACKS);
    REIPAN_PROFILE_COUNTER_STATS(
        mapput_object_draws,
        REIPAN_PROFILER_COUNTER_MAPPUT_OBJECT_DRAWS);
    g_window = {};
}

}

#undef REIPAN_PROFILE_STATS
#undef REIPAN_PROFILE_COUNTER_STATS
