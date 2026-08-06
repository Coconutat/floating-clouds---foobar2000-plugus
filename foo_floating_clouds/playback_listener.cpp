#include "stdafx.h"
#include "playback_listener.h"
#include "floating_window.h"

// ============================================================================
// PlaybackListener implementation
// ============================================================================

PlaybackListener::PlaybackListener(FloatingCloudsWindow* window)
    : play_callback_impl_base(
        flag_on_playback_starting | flag_on_playback_new_track | 
        flag_on_playback_stop | flag_on_playback_seek |
        flag_on_playback_pause | flag_on_playback_edited |
        flag_on_playback_dynamic_info | flag_on_playback_dynamic_info_track |
        flag_on_playback_time | flag_on_volume_change)
    , m_window(window)
{
    // Compile titleformat scripts for formatting track info
    auto compiler = titleformat_compiler::get();
    
    // Try to compile with standard formatting
    compiler->compile(m_script_title, "%title%");
    compiler->compile(m_script_artist, "%artist%");
    compiler->compile(m_script_album, "%album%");
}

PlaybackListener::~PlaybackListener()
{
}

void PlaybackListener::on_playback_starting(play_control::t_track_command p_command, bool p_paused)
{
    // Playback is starting, nothing to do yet
}

void PlaybackListener::on_playback_new_track(metadb_handle_ptr p_track)
{
    update_track_info(p_track);
    update_album_art(p_track);
}

void PlaybackListener::on_playback_stop(play_control::t_stop_reason p_reason)
{
    m_window->on_playback_stop();
}

void PlaybackListener::on_playback_seek(double p_time)
{
    m_window->on_playback_time(p_time);
}

void PlaybackListener::on_playback_pause(bool p_state)
{
    m_window->on_playback_pause(p_state);
}

void PlaybackListener::on_playback_edited(metadb_handle_ptr p_track)
{
    update_track_info(p_track);
}

void PlaybackListener::on_playback_dynamic_info(const file_info& p_info)
{
    // VBR bitrate changes etc - not needed for now
}

void PlaybackListener::on_playback_dynamic_info_track(const file_info& p_info)
{
    // Stream title changes - trigger a refresh
    auto api = playback_control::get();
    metadb_handle_ptr track;
    if (api->get_now_playing(track)) {
        update_track_info(track);
    }
}

void PlaybackListener::on_playback_time(double p_time)
{
    m_window->on_playback_time(p_time);
}

void PlaybackListener::on_volume_change(float p_new_val)
{
    m_window->on_volume_change(p_new_val);
}

void PlaybackListener::update_track_info(metadb_handle_ptr p_track)
{
    pfc::string8 title, artist, album;
    
    // Format the track info using titleformat
    if (m_script_title.is_valid()) {
        p_track->format_title(NULL, title, m_script_title, NULL);
    }
    if (title.is_empty()) {
        // Fallback: use file name
        title = pfc::string_filename(p_track->get_path());
    }
    
    if (m_script_artist.is_valid()) {
        p_track->format_title(NULL, artist, m_script_artist, NULL);
    }
    if (artist.is_empty()) {
        artist = "Unknown Artist";
    }
    
    if (m_script_album.is_valid()) {
        p_track->format_title(NULL, album, m_script_album, NULL);
    }
    
    // Notify window on main thread. Capture the window pointer by value (NOT
    // `this`) and verify it is still the live instance, so a queued callback
    // that runs after the window was destroyed cannot crash during shutdown.
    fb2k::inMainThread([window = m_window, title = pfc::string8(title), artist = pfc::string8(artist), album = pfc::string8(album), track = p_track]() {
        if (!FloatingCloudsWindow::is_current(window)) return;
        // Need to load album art on main thread
        album_art_data_ptr art;
        try {
            auto mgr = album_art_manager_v2::get();
            if (mgr.is_valid()) {
                metadb_handle_list items;
                items.add_item(track);
                pfc::list_t<GUID> ids;
                ids.add_item(album_art_ids::cover_front);
                
                auto extractor = mgr->open(items, ids, fb2k::noAbort);
                if (extractor.is_valid()) {
                    art = extractor->query(album_art_ids::cover_front, fb2k::noAbort);
                }
            }
        } catch (...) {
            // Album art not found, that's fine
        }
        
        window->on_playback_new_track(title, artist, album, art);
    });
}

void PlaybackListener::update_album_art(metadb_handle_ptr p_track)
{
    // Album art is loaded in update_track_info via the main thread callback
    // This is intentionally empty - the art is loaded along with track info
}