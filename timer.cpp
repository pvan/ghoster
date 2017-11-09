

static bool global_set_timer_resolution = false;
static void GlobalSet1MsResolution()
{
    if (timeBeginPeriod(1) != TIMERR_NOERROR) {
        char err[256];
        sprintf(err, "Unable to set resolution of Sleep to 1ms");
        OutputDebugString(err);
    }
}

struct Timer
{
    i64 starting_ticks;
    i64 ticks_per_second;  // set via QueryPerformanceFrequency on Start()
    i64 ticks_last_frame;
    bool started = false;
    void Start()
    {
        if (!global_set_timer_resolution) GlobalSet1MsResolution();
        started = true;
        LARGE_INTEGER now;    // .QuadPart to get number as int64
        LARGE_INTEGER freq;
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&freq);
        starting_ticks = now.QuadPart;
        ticks_per_second = freq.QuadPart;
    }
    void Stop()
    {
        started = false;
    }
    i64 TicksNow()
    {
        LARGE_INTEGER ticks_now;
        QueryPerformanceCounter(&ticks_now);
        return ticks_now.QuadPart;
    }
    i64 TicksElapsedSinceStart()
    {
        if (!started) return 0;
        return TicksNow() - starting_ticks;
    }
    double MsElapsedBetween(i64 old_ticks, i64 ticks_now)
    {
        int64_t ticks_elapsed = ticks_now - old_ticks;
        ticks_elapsed *= 1000; // tip from msdn: covert to ms before to help w/ precision
        double delta_ms = (double)ticks_elapsed / (double)ticks_per_second;
        return delta_ms;
    }
    double MsElapsedSince(i64 old_ticks)
    {
        return MsElapsedBetween(old_ticks, TicksNow());
    }
    double MsSinceStart()
    {
        if (!started) {
            OutputDebugString("Timer: Tried to get time without starting first.\n");
            return 0;
        }
        return MsElapsedSince(starting_ticks);
    }

    // these feel a little state-y
    double MsSinceLastFrame()
    {
        return MsElapsedSince(ticks_last_frame);
    }
    void EndFrame()
    {
        ticks_last_frame = TicksNow();
    }

};


struct Stopwatch
{
    Timer timer;
    bool paused = false;
    i64 ticks_elapsed_at_pause;
    i64 TicksElapsed()
    {
        if (!timer.started)
            return 0;
        if (paused)
            return ticks_elapsed_at_pause;
        return timer.TicksElapsedSinceStart();
    }
    void ResetCompletely()
    {
        timer.started = false;
        paused = false;
        ticks_elapsed_at_pause = 0;
    }
    void Start()
    {
        if (timer.started)
        {
            if (paused)
            {
                // OutputDebugString("Stopwatch: Unpausing.");
                // restart and subtract our already elapsed time
                timer.Start();
                timer.starting_ticks -= ticks_elapsed_at_pause;
            }
            else
            {
                OutputDebugString("Stopwatch: tried starting an already running stopwatch.");
            }
        }
        else
        {
            // OutputDebugString("Stopwatch: Starting for the first time.");
            timer.Start();
        }
        paused = false;
    }
    void Pause()
    {
        ticks_elapsed_at_pause = TicksElapsed();
        paused = true; // set after getting ticks or we'll get old ticks_elap value
    }
    double MsElapsed()
    {
        return (double)(TicksElapsed()*1000) / (double)timer.ticks_per_second;
    }
};

