#include "sound.hpp"
#include "sound_data.hpp"
#include "ui_state.hpp"
#include <Windows.h>
#include <mmsystem.h>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <array>

#pragma comment(lib, "winmm.lib")

namespace
{
    constexpr int SAMPLE_RATE = 44100;
    constexpr int BUFFER_SAMPLES = 882; // 20 ms chunk
    constexpr int NUM_BUFFERS = 3;

    struct active_voice
    {
        const int16_t* samples = nullptr;
        int length = 0;
        int cursor = 0;
        float volume = 1.0f;
    };

    struct sound_sample_view
    {
        const int16_t* samples = nullptr;
        int length = 0;
    };

    struct audio_engine
    {
        std::atomic<bool> initialized{ false };
        std::atomic<bool> running{ false };
        HWAVEOUT h_wave_out = nullptr;
        HANDLE wake_event = nullptr;
        HANDLE buffer_event = nullptr;
        std::thread worker;

        std::mutex voice_mutex;
        std::vector<active_voice> voices;

        std::array<sound_sample_view, 7> sound_tables{};
        std::array<WAVEHDR, NUM_BUFFERS> headers{};
        std::array<std::array<int16_t, BUFFER_SAMPLES>, NUM_BUFFERS> buffer_data{};

        std::atomic<uint64_t> last_slider_tick_ms{ 0 };

        void load_sound_tables()
        {
            sound_tables[static_cast<int>(gui::sound::type::click)] = {
                gui::sound::data::sound_click,
                static_cast<int>(gui::sound::data::sound_click_len)
            };
            sound_tables[static_cast<int>(gui::sound::type::toggle_on)] = {
                gui::sound::data::sound_toggle_on,
                static_cast<int>(gui::sound::data::sound_toggle_on_len)
            };
            sound_tables[static_cast<int>(gui::sound::type::toggle_off)] = {
                gui::sound::data::sound_toggle_off,
                static_cast<int>(gui::sound::data::sound_toggle_off_len)
            };
            sound_tables[static_cast<int>(gui::sound::type::tab_switch)] = {
                gui::sound::data::sound_tab_switch,
                static_cast<int>(gui::sound::data::sound_tab_switch_len)
            };
            sound_tables[static_cast<int>(gui::sound::type::open_gui)] = {
                gui::sound::data::sound_open_gui,
                static_cast<int>(gui::sound::data::sound_open_gui_len)
            };
            sound_tables[static_cast<int>(gui::sound::type::close_gui)] = {
                gui::sound::data::sound_close_gui,
                static_cast<int>(gui::sound::data::sound_close_gui_len)
            };
            sound_tables[static_cast<int>(gui::sound::type::slider_tick)] = {
                gui::sound::data::sound_slider_tick,
                static_cast<int>(gui::sound::data::sound_slider_tick_len)
            };
        }

        void run_audio_loop()
        {
            int current_buffer = 0;
            int idle_silent_buffers = 0;

            while (running.load(std::memory_order_relaxed))
            {
                // If idle for more than 4 buffers, sleep on wake_event to save CPU
                if (idle_silent_buffers >= 4)
                {
                    HANDLE events[2] = { wake_event, buffer_event };
                    WaitForMultipleObjects(2, events, FALSE, 200);
                    if (!running.load(std::memory_order_relaxed))
                        break;
                }

                WAVEHDR& hdr = headers[current_buffer];

                // Wait until the buffer is free to write
                if ((hdr.dwFlags & WHDR_PREPARED) && !(hdr.dwFlags & WHDR_DONE))
                {
                    WaitForSingleObject(buffer_event, 10);
                }

                if (hdr.dwFlags & WHDR_PREPARED)
                {
                    waveOutUnprepareHeader(h_wave_out, &hdr, sizeof(WAVEHDR));
                }

                auto& buffer = buffer_data[current_buffer];
                bool has_audio = false;

                {
                    std::lock_guard<std::mutex> lock(voice_mutex);
                    if (!voices.empty())
                    {
                        std::fill(buffer.begin(), buffer.end(), 0);
                        has_audio = true;

                        for (auto it = voices.begin(); it != voices.end();)
                        {
                            const int remaining_samples = it->length - it->cursor;
                            const int to_mix = std::min(BUFFER_SAMPLES, remaining_samples);

                            for (int s = 0; s < to_mix; ++s)
                            {
                                const int32_t current_val = buffer[s];
                                const int32_t voice_val = static_cast<int32_t>(it->samples[it->cursor + s] * it->volume);
                                buffer[s] = static_cast<int16_t>(std::clamp(current_val + voice_val, -32768, 32767));
                            }

                            it->cursor += to_mix;
                            if (it->cursor >= it->length)
                            {
                                it = voices.erase(it);
                            }
                            else
                            {
                                ++it;
                            }
                        }
                    }
                }

                if (has_audio)
                {
                    idle_silent_buffers = 0;
                }
                else
                {
                    std::fill(buffer.begin(), buffer.end(), 0);
                    idle_silent_buffers++;
                }

                hdr.lpData = reinterpret_cast<LPSTR>(buffer.data());
                hdr.dwBufferLength = BUFFER_SAMPLES * sizeof(int16_t);
                hdr.dwFlags = 0;

                if (waveOutPrepareHeader(h_wave_out, &hdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR)
                {
                    waveOutWrite(h_wave_out, &hdr, sizeof(WAVEHDR));
                }

                current_buffer = (current_buffer + 1) % NUM_BUFFERS;
            }
        }
    };

    audio_engine g_audio;
}

void gui::sound::init()
{
    // Temporarily disabled per user request
    return;
}

void gui::sound::shutdown()
{
    if (!g_audio.initialized.load())
        return;

    g_audio.running.store(false);
    if (g_audio.wake_event)
        SetEvent(g_audio.wake_event);
    if (g_audio.buffer_event)
        SetEvent(g_audio.buffer_event);

    if (g_audio.worker.joinable())
        g_audio.worker.join();

    if (g_audio.h_wave_out)
    {
        waveOutReset(g_audio.h_wave_out);
        for (int i = 0; i < NUM_BUFFERS; ++i)
        {
            if (g_audio.headers[i].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(g_audio.h_wave_out, &g_audio.headers[i], sizeof(WAVEHDR));
        }
        waveOutClose(g_audio.h_wave_out);
        g_audio.h_wave_out = nullptr;
    }

    if (g_audio.wake_event)
    {
        CloseHandle(g_audio.wake_event);
        g_audio.wake_event = nullptr;
    }
    if (g_audio.buffer_event)
    {
        CloseHandle(g_audio.buffer_event);
        g_audio.buffer_event = nullptr;
    }

    g_audio.initialized.store(false);
}

void gui::sound::play(type sound_type)
{
    // Temporarily disabled per user request
    return;
}

void gui::sound::play_slider_tick()
{
    return;
}

void gui::sound::set_volume(float volume)
{
    auto& prefs = gui::aegis::get_preferences();
    prefs.sound_volume = std::clamp(volume, 0.0f, 1.0f);
    gui::aegis::mark_preferences_dirty();
}

void gui::sound::set_enabled(bool enabled)
{
    auto& prefs = gui::aegis::get_preferences();
    prefs.sound_effects = enabled;
    gui::aegis::mark_preferences_dirty();
}

bool gui::sound::is_enabled()
{
    return gui::aegis::get_preferences().sound_effects;
}

float gui::sound::get_volume()
{
    return gui::aegis::get_preferences().sound_volume;
}
