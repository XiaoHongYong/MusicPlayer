#pragma once

#define MP_SKIN_IDS     \
DEFINE_CMD_ID_TIP(ID_TOPMOST,           _TLM("Toggle always on top"))               \
DEFINE_CMD_ID_TIP(ID_CLICK_THROUGH,     _TLM("Mouse click through"))                \
DEFINE_CMD_ID_TIP(ID_RATE_LYR,          _TLM("Rate these lyrics"))                  \
DEFINE_CMD_ID_TIP(ID_PLAY,              _TLM("Play"))                               \
DEFINE_CMD_ID_TIP(ID_PLAYPAUSE,         _TLM("Play/Pause"))                         \
DEFINE_CMD_ID_TIP(ID_PAUSE,             _TLM("Pause"))                              \
DEFINE_CMD_ID_TIP(ID_STOP,              _TLM("Stop"))                               \
DEFINE_CMD_ID_TIP(ID_BACKWARD,          _TLM("Backward"))                           \
DEFINE_CMD_ID_TIP(ID_FORWARD,           _TLM("Forward"))                            \
DEFINE_CMD_ID_TIP(ID_J_PREV_LINE,       _TLM("Jump to previous line of lyrics"))    \
DEFINE_CMD_ID_TIP(ID_J_NEXT_LINE,       _TLM("Jump to next line of lyrics"))        \
\
DEFINE_CMD_ID_TIP(ID_FONT_SIZE_INC,     _TLM("Increase font size"))                 \
DEFINE_CMD_ID_TIP(ID_FONT_SIZE_DEC,     _TLM("Decrease font size"))                 \
DEFINE_CMD_ID_TIP(ID_CLR_PREV_HUE,      _TLM("Previous color"))                     \
DEFINE_CMD_ID_TIP(ID_CLR_NEXT_HUE,      _TLM("Next color"))                         \
DEFINE_CMD_ID_TIP(ID_LYR_HIGH_CLR_LIST, _TLM("Highlight lyrics color"))             \
DEFINE_CMD_ID_TIP(ID_MENU_STATIC_LYR,   _TLM("Menu"))                               \
\
DEFINE_CMD_ID(ID_TB_LYR_SYNC)                                                       \
DEFINE_CMD_ID(ID_TB_LYR_TXT)                                                        \
DEFINE_CMD_ID(ID_TB_LYR_EDIT)                                                       \
\
DEFINE_CMD_ID_TIP(ID_TOGGLE_LYR_EDIT_TOOLBAR, _TLM("Toggle extended toolbar visible")) \
DEFINE_CMD_ID_TIP(ID_INSERTTAG,         _TLM("Synchronize the current line"))        \
DEFINE_CMD_ID_TIP(ID_DEL_TAG,           _TLM("Remove timestamp of the current line"))\
DEFINE_CMD_ID_TIP(ID_INSERTTAG_DOWN,    _TLM("Synchronize the current line and move to next line"))\
DEFINE_CMD_ID_TIP(ID_EDIT_HELP,         _TLM("Editor help"))                         \
DEFINE_CMD_ID_TIP(ID_JUMP_TO_CUR_LINE,  _TLM("Jump to current line"))                \
DEFINE_CMD_ID(ID_FORWARD_REMAIN_LINES)                                              \
DEFINE_CMD_ID(ID_BACKWARD_REMAIN_LINES)                                             \
DEFINE_CMD_ID_TIP(ID_FORWARD_CUR_LINE, _TLM("Forward the timestamp of this line 0.2s"))   \
DEFINE_CMD_ID_TIP(ID_BACKWARD_CUR_LINE, _TLM("Backward the timestamp of this line 0.2s")) \
DEFINE_CMD_ID_TIP(ID_AUTO_FILL_LYR_INFO, _TLM("Auto fill artist, album and title info"))  \
DEFINE_CMD_ID_TIP(ID_LYR_EDITOR, _TLM("Lyrics Editor"))                                   \
DEFINE_CMD_ID_TIP(ID_EXTERNAL_LYR_EDIT, _TLM("Edit with external lyrics editor"))         \
DEFINE_CMD_ID_TIP(ID_SAVE_LYR_IN_SONG_FILE, _TLM("save lyrics in song file"))             \
\
DEFINE_CMD_ID_TIP(ID_EDIT_FIND,         _TLM("Find"))               \
DEFINE_CMD_ID_TIP(ID_EDIT_FINDNEXT,     _TLM("Find next"))          \
DEFINE_CMD_ID_TIP(ID_EDIT_REPLACE,      _TLM("Replace"))            \
DEFINE_CMD_ID_TIP(ID_EDIT_LYR_TAG,      _TLM("Edit lyrics tag"))    \
\
DEFINE_CMD_ID(ID_REMOVE_ALL_TAG)                                    \
DEFINE_CMD_ID(ID_REMOVE_BLANK_LINE)                                 \
DEFINE_CMD_ID(ID_TRIM_WHITESPACE)                                   \
DEFINE_CMD_ID(ID_REMOVE_UNSYNC_LINES)                               \
DEFINE_CMD_ID(ID_CAPITALIZE_LEADING_LETTER)                         \
DEFINE_CMD_ID(ID_LYRICS_TO_LOWERCASE)                               \
\
DEFINE_CMD_ID_TIP(ID_NEW_LRC,           _TLM("New"))                \
DEFINE_CMD_ID_TIP(ID_OPEN_LRC,          _TLM("Open lyrics"))        \
DEFINE_CMD_ID_TIP(ID_SAVE_LRC,          _TLM("Save lyrics"))        \
DEFINE_CMD_ID_TIP(ID_SAVE_LRC_AS,       _TLM("Save lyrics as"))     \
DEFINE_CMD_ID_TIP(ID_UPLOAD_LYR,        _TLM("Upload lyrics..."))   \
\
DEFINE_CMD_ID_TIP(ID_BACKWARD_LYRICS,   _TLM("Backward lyrics 0.5 second")) \
DEFINE_CMD_ID_TIP(ID_FORWARD_LYRICS,    _TLM("Forward lyrics 0.5 second"))  \
DEFINE_CMD_ID_TIP(ID_DISPLAY_OPT,       _TLM("Lyrics display options"))     \
DEFINE_CMD_ID_TIP(ID_HELP,              _TLM("Help and support"))   \
DEFINE_CMD_ID_TIP(ID_PREVIOUS,          _TLM("Previous track"))     \
DEFINE_CMD_ID_TIP(ID_NEXT,              _TLM("Next track"))         \
\
DEFINE_CMD_ID(ID_SEEK)                                              \
DEFINE_CMD_ID_TIP(ID_VOL_INC,           _TLM("Increase volume"))    \
DEFINE_CMD_ID_TIP(ID_VOL_DEC,           _TLM("Decrease volume"))    \
DEFINE_CMD_ID_TIP(ID_VOLUME,            _TLM("Volume"))             \
DEFINE_CMD_ID_TIP(ID_MUTE,              _TLM("Mute"))               \
DEFINE_CMD_ID(ID_PLAYLIST)                                          \
DEFINE_CMD_ID_TIP(ID_SHUFFLE,           _TLM("Toggle shuffle"))     \
DEFINE_CMD_ID_TIP(ID_LOOP,              _TLM("Toggle repeat"))      \
DEFINE_CMD_ID(ID_LOOP_ALL)                                          \
DEFINE_CMD_ID(ID_LOOP_TRACK)                                        \
DEFINE_CMD_ID(ID_LOOP_OFF)                                          \
DEFINE_CMD_ID_TIP(ID_EQ,                _TLM("Equalizer"))          \
DEFINE_CMD_ID_TIP(ID_RATE,              _TLM("Rate this song"))     \
DEFINE_CMD_ID(ID_TXT_TITLE)                                         \
DEFINE_CMD_ID(ID_TXT_POS)                                           \
DEFINE_CMD_ID(ID_TXT_VOL)                                           \
DEFINE_CMD_ID(ID_TXT_STATE)                                         \
DEFINE_CMD_ID(ID_TXT_INFO)                                          \
DEFINE_CMD_ID(ID_STEREO_STAT)                                       \
\
DEFINE_CMD_ID_TIP(ID_PL_UP,             _TLM("Move up"))            \
DEFINE_CMD_ID_TIP(ID_PL_DOWN,           _TLM("Move down"))          \
DEFINE_CMD_ID_TIP(ID_PL_ADD_FILE,       _TLM("Add file"))           \
DEFINE_CMD_ID_TIP(ID_PL_ADD_DIR,        _TLM("Add folder"))         \
DEFINE_CMD_ID_TIP(ID_PL_ADD_URL,        _TLM("Add url"))            \
DEFINE_CMD_ID_TIP(ID_PL_OPEN_FILE,      _TLM("Open file"))          \
DEFINE_CMD_ID_TIP(ID_PL_OPEN_DIR,       _TLM("Open folder"))        \
DEFINE_CMD_ID_TIP(ID_PL_OPEN_URL,       _TLM("Open url"))           \
DEFINE_CMD_ID_TIP(ID_PL_DEL,            _TLM("Delete"))             \
DEFINE_CMD_ID_TIP(ID_PL_PROPERTY,       _TLM("Property"))           \
DEFINE_CMD_ID_TIP(ID_PL_NEW,            _TLM("New playlist"))       \
DEFINE_CMD_ID_TIP(ID_PL_LOAD,           _TLM("Load playlist"))      \
DEFINE_CMD_ID_TIP(ID_PL_SAVE,           _TLM("Save playlist"))      \
DEFINE_CMD_ID(ID_PL_LIST)                                           \
\
DEFINE_CMD_ID(ID_MG_TREE_GUIDE)                                     \
DEFINE_CMD_ID(ID_MG_MEDIA_LIST)                                     \
DEFINE_CMD_ID(ID_ML_GUIDE)                                          \
DEFINE_CMD_ID(ID_ML_BACK)                                           \
DEFINE_CMD_ID(ID_ML_MEDIA_LIST)                                     \
DEFINE_CMD_ID(ID_ALBUMART)                                          \
DEFINE_CMD_ID_TIP(ID_TOGGLE_MP, _TLM("Bring to front/Hide $Product$"))\
DEFINE_CMD_ID_TIP(ID_SHOW_MAIN_WND,     _TLM("Show main window"))   \
DEFINE_CMD_ID_TIP(ID_PREFERENCES,       _TLM("Preferences"))        \
DEFINE_CMD_ID_TIP(ID_RELOAD_LYR,        _TLM("Reload lyrics"))      \
DEFINE_CMD_ID_TIP(ID_FLOATING_LYRICS,   _TLM("Floating Lyrics"))    \
DEFINE_CMD_ID_TIP(ID_OPEN_MEDIA_CENTER, _TLM("Media Center"))       \
\
DEFINE_CMD_ID(ID_RATE_LYR_1)                                        \
DEFINE_CMD_ID(ID_RATE_LYR_2)                                        \
DEFINE_CMD_ID(ID_RATE_LYR_3)                                        \
DEFINE_CMD_ID(ID_RATE_LYR_4)                                        \
DEFINE_CMD_ID(ID_RATE_LYR_5)                                        \
DEFINE_CMD_ID(ID_LDO_KARAOKE)                                       \
DEFINE_CMD_ID(ID_ABOUT)                                             \
DEFINE_CMD_ID(ID_ADJUST_HUE)                                        \
DEFINE_CMD_ID(ID_BR_ALBUM_ART)                                      \
DEFINE_CMD_ID(ID_LOGIN_VIA_IE)                                      \
DEFINE_CMD_ID(ID_APPLY_ACCOUNT)                                     \
DEFINE_CMD_ID(ID_EMAIL)                                             \
DEFINE_CMD_ID(ID_WEBHOME)                                           \
DEFINE_CMD_ID(ID_LDO_AUTO)                                          \
DEFINE_CMD_ID(ID_LDO_NORMAL)                                        \
DEFINE_CMD_ID(ID_LDO_FADE_IN)                                       \
DEFINE_CMD_ID(ID_LDO_FADEOUT_BG)                                    \
DEFINE_CMD_ID(ID_LDS_MULTI_LINE)                                    \
DEFINE_CMD_ID(ID_LDS_STATIC_TXT)                                    \
DEFINE_CMD_ID(ID_LDS_TWO_LINE)                                      \
DEFINE_CMD_ID(ID_LDS_SINGLE_LINE)                                   \
DEFINE_CMD_ID(ID_LDS_VOBSUB)                                        \
DEFINE_CMD_ID_TIP(ID_NO_SUITTABLE_LYRICS, _TLM("&No suitable lyrics for the Song")) \
DEFINE_CMD_ID_TIP(ID_INSTRUMENTAL_MUSIC, _TLM("Instrumental music, no lyrics")) \
DEFINE_CMD_ID_TIP(ID_SEARCH_LYR_SUGGESTIONS, _TLM("Lyrics search suggestions")) \
DEFINE_CMD_ID(ID_REMOVE_FROM_LIBRARY)                               \
DEFINE_CMD_ID(ID_REPLACE_NOW_PLAYING_WITH_SELECTED)                 \
DEFINE_CMD_ID(ID_REPLACE_NOW_PLAYING_WITH_RESULTS)                  \
DEFINE_CMD_ID(ID_ADD_RESULTS_TO_PLAYLIST_NEW)                       \
DEFINE_CMD_ID(ID_ADD_SELECTED_TO_PLAYLIST_NEW)                      \
DEFINE_CMD_ID(ID_ADD_RESULTS_TO_NOW_PLAYING)                        \
DEFINE_CMD_ID(ID_ADD_SELECTED_TO_NOW_PLAYING)                       \
DEFINE_CMD_ID(ID_SHOW_IN_FINDER)                                    \
DEFINE_CMD_ID(ID_PLAY_SELECTED_FILE)                                \
\
DEFINE_CMD_ID(ID_ADD_PIC)                                           \
DEFINE_CMD_ID(ID_DEL_PIC)                                           \
DEFINE_CMD_ID(ID_SAVE_PIC_AS)                                       \
DEFINE_CMD_ID(ID_DELETE)                                            \
DEFINE_CMD_ID(ID_RENAME)                                            \
DEFINE_CMD_ID(ID_NO_PROMPT)                                         \
DEFINE_CMD_ID(ID_DEL_FILE)                                          \
DEFINE_CMD_ID(ID_BROWSE)                                            \
DEFINE_CMD_ID(ID_QUEUE_UP)                                          \
DEFINE_CMD_ID(ID_QUEUE_UP_ALL)                                      \
DEFINE_CMD_ID(ID_NOWPLAYING)                                        \
DEFINE_CMD_ID(ID_ADD_DIR_TO_ML)                                     \
DEFINE_CMD_ID(ID_ADD_FILE_TO_ML)                                    \
DEFINE_CMD_ID(ID_REMOVE_FROM_ML)                                    \
DEFINE_CMD_ID(ID_EDITOR_LYR_COLOR)                                  \
DEFINE_CMD_ID(ID_EDITOR_HIGH_COLOR)                                 \
DEFINE_CMD_ID(ID_TAG_COLOR)                                         \
DEFINE_CMD_ID(ID_EDIT_LINE_COLOR)                                   \
DEFINE_CMD_ID(ID_EDITOR_BG_COLOR)                                   \
DEFINE_CMD_ID(ID_OPEN_PL)                                           \
DEFINE_CMD_ID(ID_CLEAR)                                             \
DEFINE_CMD_ID(ID_PIC_ACTIONS)                                       \
DEFINE_CMD_ID(ID_ENC_DEFAULT)                                       \
DEFINE_CMD_ID(ID_ENC_UNICODE)                                       \
DEFINE_CMD_ID(ID_ENC_UTF8)                                          \
DEFINE_CMD_ID(ID_ENC_BALTIC)                                        \
DEFINE_CMD_ID(ID_ENC_WEST_EUROPE)                                   \
DEFINE_CMD_ID(ID_ENC_CENTRAL_EUROPE)                                \
DEFINE_CMD_ID(ID_ENC_GREEK)                                         \
DEFINE_CMD_ID(ID_ENC_CYRILLIC)                                      \
DEFINE_CMD_ID(ID_ENC_TURKISH)                                       \
DEFINE_CMD_ID(ID_ENC_VIETNAMESE)                                    \
DEFINE_CMD_ID(ID_ENC_KOREAN)                                        \
DEFINE_CMD_ID(ID_ENC_ARABIC)                                        \
DEFINE_CMD_ID(ID_ENC_HEBREW)                                        \
DEFINE_CMD_ID(ID_ENC_THAI)                                          \
DEFINE_CMD_ID(ID_ENC_JAPANESE)                                      \
DEFINE_CMD_ID(ID_ENC_CHT)                                           \
DEFINE_CMD_ID(ID_ENC_CHS)                                           \
DEFINE_CMD_ID(ID_ENC_LATIN9_ISO)                                    \
\
DEFINE_CMD_ID(ID_SET_OPAQUE_100)                                    \
DEFINE_CMD_ID(ID_SET_OPAQUE_90)                                     \
DEFINE_CMD_ID(ID_SET_OPAQUE_80)                                     \
DEFINE_CMD_ID(ID_SET_OPAQUE_70)                                     \
DEFINE_CMD_ID(ID_SET_OPAQUE_60)                                     \
DEFINE_CMD_ID(ID_SET_OPAQUE_50)                                     \
DEFINE_CMD_ID(ID_SET_OPAQUE_40)                                     \
DEFINE_CMD_ID(ID_SET_OPAQUE_30)                                     \
DEFINE_CMD_ID(ID_SET_OPAQUE_20)                                     \
DEFINE_CMD_ID(ID_SET_OPAQUE_10)                                     \
\
DEFINE_CMD_ID(ID_NEW_MENU_POS)                                      \
DEFINE_CMD_ID(ID_SHOW_ERR_RESULT)                                   \
DEFINE_CMD_ID(ID_DL_ALBUM_ART)                                      \

#undef DEFINE_CMD_ID
#define DEFINE_CMD_ID(uid) uid,

#undef DEFINE_CMD_ID_TIP
#define DEFINE_CMD_ID_TIP(uid, tooltip)   uid,

//
// Command ID of UIObject in SkinWnd:
//
enum ML_CUSTOM_COMMAND {
    _START_                 = ID_BASE_END,

    ID_ADD_RESULTS_TO_PLAYLIST_START,
    ID_ADD_RESULTS_TO_PLAYLIST_END = ID_ADD_RESULTS_TO_PLAYLIST_START + 10,
    ID_ADD_SELECTED_TO_PLAYLIST_START,
    ID_ADD_SELECTED_TO_PLAYLIST_END = ID_ADD_SELECTED_TO_PLAYLIST_START + 10,

    ID_SKIN_START,
    ID_SKIN_END = ID_SKIN_START + 20,

    MP_SKIN_IDS
};

extern UIObjectIDDefinition g_uidDefinition[];
