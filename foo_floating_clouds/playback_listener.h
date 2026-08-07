#pragma once

#include "stdafx.h"

// ============================================================================
// PlaybackListener - foobar2000 playback callback interface
// ============================================================================

class FloatingCloudsWindow; // forward decl

class PlaybackListener : public play_callback_impl_base
{
public:
    PlaybackListener(FloatingCloudsWindow* window);
    ~PlaybackListener();

    // play_callback overrides
    void on_playback_starting(play_control::t_track_command p_command, bool p_paused) override;
    void on_playback_new_track(metadb_handle_ptr p_track) override;
    void on_playback_stop(play_control::t_stop_reason p_reason) override;
    void on_playback_seek(double p_time) override;
    void on_playback_pause(bool p_state) override;
    void on_playback_edited(metadb_handle_ptr p_track) override;
    void on_playback_dynamic_info(const file_info& p_info) override;
    void on_playback_dynamic_info_track(const file_info& p_info) override;
    void on_playback_time(double p_time) override;
    void on_volume_change(float p_new_val) override;

private:
    void update_track_info(metadb_handle_ptr p_track);
    void update_album_art(metadb_handle_ptr p_track);
    void update_lyrics(metadb_handle_ptr p_track);
    
    FloatingCloudsWindow* m_window;
    
    // Titleformat objects for formatting
    service_ptr_t<titleformat_object> m_script_title;
    service_ptr_t<titleformat_object> m_script_artist;
    service_ptr_t<titleformat_object> m_script_album;
};