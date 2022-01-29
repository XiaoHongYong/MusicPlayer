// MLCmd.cpp: implementation of the MLCmd class.
//
//////////////////////////////////////////////////////////////////////

#include "MPlayerApp.h"
#include "MLCmd.h"

#define DEFINE_CMD_ID(uid, idc)        { #uid, uid, idc, nullptr },
#define DEFINE_CMD_ID2(uid, idc, tooltip)        { #uid, uid, idc, tooltip },

UIObjectIDDefinition g_uidDefinition[] = 
{
    { "ID_TOPMOST", CMD_TOPMOST, IDC_SETTOPMOST, _TLM("Toggle always on top") },
    { "ID_CLICK_THROUGH", CMD_CLICK_THROUGH, IDC_CLICK_THROUGH, _TLM("Mouse click through") },
    { "ID_RATE_LYR", CMD_RATE_LYR, IDC_RATE_LYRICS, _TLM("rate these lyrics") },
    { "ID_PLAY", CMD_PLAY, IDC_PLAY, _TLM("play") },
    { "ID_PLAYPAUSE", CMD_PLAYPAUSE, IDC_PLAYPAUSE, _TLM("play/pause") },
    { "ID_PAUSE", CMD_PAUSE, IDC_PAUSE, _TLM("pause") },
    { "ID_STOP", CMD_STOP, IDC_STOP, _TLM("stop") },
    { "ID_BACKWARD", CMD_BACKWARD, IDC_BACKWARD2SEC, _TLM("Backward") },
    { "ID_FORWARD", CMD_FORWARD, IDC_FORWARD2SEC, _TLM("Forward") },
    { "ID_J_PREV_LINE", CMD_J_PREV_LINE, 0, _TLM("Jump to previous line of lyrics") },
    { "ID_J_NEXT_LINE", CMD_J_NEXT_LINE, 0, _TLM("Jump to next line of lyrics") },

    { "ID_FONT_SIZE_INC", CMD_FONT_SIZE_INC, 0, _TLM("increase font size") },
    { "ID_FONT_SIZE_DEC", CMD_FONT_SIZE_DEC, 0, _TLM("decrease font size") },
    { "ID_CLR_PREV_HUE", CMD_CLR_PREV_HUE, 0, _TLM("Previous Color") },
    { "ID_CLR_NEXT_HUE", CMD_CLR_NEXT_HUE, 0, _TLM("next Color") },
    { "ID_LYR_HIGH_CLR_LIST", CMD_LYR_HIGH_CLR_LIST, 0, _TLM("Highlight lyrics color") },
    { "ID_MENU_STATIC_LYR", CMD_MENU_STATIC_LYR, 0, _TLM("Menu") },

    { "ID_LYR_SCROLL_ENABLE_RECORD",     CMD_LYR_SCROLL_ENABLE_RECORD, 0, _TLM("enable/Disable record lyrics scrolling actions") },
    { "ID_LYR_SCROLL_ENABLE_REPLAY",     CMD_LYR_SCROLL_ENABLE_REPLAY, 0, _TLM("enable/Disable replay lyrics scrolling actions") },
    { "ID_LYR_SCROLL_MENU",     CMD_LYR_SCROLL_MENU, 0, _TLM("Menu") },

    { "ID_TB_LYR_SYNC", ID_TB_LYR_SYNC, 0, nullptr },
    { "ID_TB_LYR_TXT", ID_TB_LYR_TXT, 0, nullptr },
    { "ID_TB_LYR_EDIT", ID_TB_LYR_EDIT, 0, nullptr },

    { "ID_TOGGLE_LYR_EDIT_TOOLBAR", CMD_TOGGLE_LYR_EDIT_TOOLBAR, 0, _TLM("Toggle Extended Toolbar Visible") },
    { "ID_INSERTTAG", CMD_INSERTTAG, IDC_INSERT_TAG, _TLM("Synchronize the current line") },
    { "ID_DEL_TAG", CMD_DEL_TAG, 0, _TLM("remove Timestamp of the current line") },
    { "ID_INSERTTAG_DOWN", CMD_INSERTTAG_DOWN, IDC_INSERT_TAG_AND_MOVE_DOWN, _TLM("Synchronize the current line and move to next line") },
    { "ID_EDIT_HELP", CMD_EDIT_HELP, 0, _TLM("Editor help") },
    { "ID_JUMP", CMD_JUMP, IDC_JUMP_TO_CURRENT_LINE, _TLM("Jump to current line") },
    { "ID_FORWARD_REMAIN_LINES", CMD_FORWARD_REMAIN_LINES, IDC_FORWARD_REMAIN_LINES, _TLM("Forward the remaining lyrics 0.2s") },
    { "ID_BACKWARD_REMAIN_LINES", CMD_BACKWARD_REMAIN_LINES, IDC_BACKWARD_REMAIN_LINES, _TLM("Backward the remaining lyrics 0.2s") },
    { "ID_FORWARD_CUR_LINE", CMD_FORWARD_CUR_LINE, IDC_FORWARD_CUR_LINE, _TLM("Forward the timestamp of this line 0.2s") },
    { "ID_BACKWARD_CUR_LINE", CMD_BACKWARD_CUR_LINE, IDC_BACKWARD_CUR_LINE, _TLM("Backward the timestamp of this line 0.2s") },
    { "ID_AUTO_FILL_LYR_INFO", CMD_AUTO_FILL_LYR_INFO, IDC_AUTO_FILL_LYRICS_INFO, _TLM("Auto fill artist, album and title info") },
    { "ID_LYR_EDITOR", CMD_LYR_EDITOR, IDC_LYR_EDITOR, _TLM("Lyrics Editor") },
    { "ID_EXTERNAL_LYR_EDIT", CMD_EXTERNAL_LYR_EDIT, IDC_EXTERNAL_LYR_EDIT, _TLM("Edit with external lyrics editor") },
    { "ID_SAVE_LYR_IN_SONG_FILE", CMD_SAVE_LYR_IN_SONG_FILE, IDC_EMBEDDED_LYRICS, _TLM("save lyrics in song file") },

    { "ID_EDIT_UNDO", CMD_EDIT_UNDO, IDC_EDIT_UNDO, _TLM("undo (Ctrl+Z)") },
    { "ID_EDIT_REDO", CMD_EDIT_REDO, IDC_EDIT_REDO, _TLM("redo (Ctrl+Y)") },
    { "ID_EDIT_CUT", CMD_EDIT_CUT, IDC_EDIT_CUT, _TLM("cut (Ctrl+X)") },
    { "ID_EDIT_COPY", CMD_EDIT_COPY, IDC_EDIT_COPY, _TLM("copy (Ctrl+C)") },
    { "ID_EDIT_PASTE", CMD_EDIT_PASTE, IDC_EDIT_PASTE, _TLM("Paste (Ctrl+V)") },
    { "ID_EDIT_DELETE", CMD_EDIT_DELETE, IDC_EDIT_DELETE, _TLM("delete") },
    { "ID_EDIT_FIND", CMD_EDIT_FIND, IDC_EDIT_FIND, _TLM("Find") },
    { "ID_EDIT_FINDNEXT", CMD_EDIT_FINDNEXT, 0, _TLM("Find next") },
    { "ID_EDIT_REPLACE", CMD_EDIT_REPLACE, IDC_EDIT_REPLACE, _TLM("Replace") },
    { "ID_EDIT_LYR_TAG", CMD_EDIT_LYR_TAG, IDC_EDIT_LYR_TAG, _TLM("Edit Lyrics Tag") },

    { "CMD_REMOVE_ALL_TAG", CMD_REMOVE_ALL_TAG, IDC_REMOVE_ALL_TAG, _TLM("remove All Timestamps") },
    { "CMD_REMOVE_BLANK_LINE", CMD_REMOVE_BLANK_LINE, IDC_REMOVE_BLANK_LINE, _TLM("remove All Blank Lines") },
    { "CMD_TRIM_WHITESPACE", CMD_TRIM_WHITESPACE, IDC_TRIM_SPACE, _TLM("Trim Leading and Trailing Whitespace of All Lines") },
    { "CMD_REMOVE_UNSYNC_LINES", CMD_REMOVE_UNSYNC_LINES, IDC_REMOVE_UNSYNC_LINE, _TLM("remove All Unsynchronized Lines") },
    { "CMD_CAPITALIZE_LEADING_LETTER", CMD_CAPITALIZE_LEADING_LETTER, IDC_CAPITALIZE_LEADING_LETTER, _TLM("Capitalize the Leading Letter of All Lines") },
    { "CMD_LYRICS_TO_LOWERCASE", CMD_LYRICS_TO_LOWERCASE, IDC_TO_LOWER_CASE, _TLM("Convert All Lyrics Characters to Lower Case") },

    { "ID_NEW_LRC", CMD_NEW_LRC, IDC_NEW_LYRICS, _TLM("new") },
    { "ID_OPEN_LRC", CMD_OPEN_LRC, IDC_OPEN_LYRICS, _TLM("open lyrics") },
    { "ID_SAVE_LRC", CMD_SAVE_LRC, IDC_SAVE_LYRICS, _TLM("save lyrics") },
    { "ID_SAVE_LRC_AS", CMD_SAVE_LRC_AS, IDC_SAVE_LYRICS_AS, _TLM("save lyrics as") },
    { "ID_UPLOAD_LYR", CMD_UPLOAD_LYR, IDC_UPLOAD_LYRICS, _TLM("upload Lyrics...") },

    { "ID_SEARCH_LYRICS", CMD_OPEN_LRC, IDC_OPEN_LYRICS, _TLM("search Lyrics") },
    { "ID_BACKWARD_LYRICS", CMD_BACKWARD_LYRICS, 0, _TLM("Lyrics Backward 0.5 Sec (Up)") },
    { "ID_FORWARD_LYRICS", CMD_FORWARD_LYRICS, 0, _TLM("Lyrics Forward 0.5 Sec (Down)") },
    { "ID_DISPLAY_OPT", CMD_DISPLAY_OPT, IDC_LYR_DISPLAY_OPT, _TLM("Lyrics Display Options") },
    { "ID_HELP", CMD_HELP, IDC_ML_HELP, _TLM("Help and Support") },
    { "ID_PREVIOUS", CMD_PREVIOUS, IDC_PREV, _TLM("Previous Track") },
    { "ID_NEXT", CMD_NEXT, IDC_NEXT, _TLM("next Track") },

    { "ID_SEEK", CMD_SEEK, 0, "seek" },
    { "ID_VOL_INC", CMD_VOL_INC, 0, "increase Volume" },
    { "ID_VOL_DEC", CMD_VOL_DEC, 0, "decrease Volume" },
    { "ID_VOLUME", CMD_VOLUME, 0, _TLM("Volume") },
    { "ID_MUTE", CMD_MUTE, 0, "Mute" },
    { "ID_PLAYLIST", ID_PLAYLIST, 0, nullptr },
    { "ID_SHUFFLE", CMD_SHUFFLE, IDC_SHUFFLE, "Toggle Shuffle" },
    { "ID_LOOP", CMD_LOOP, 0, "Toggle Repeat" },
    DEFINE_CMD_ID( CMD_LOOP_OFF, IDC_REPEAT_OFF )
    DEFINE_CMD_ID( CMD_LOOP_ALL, IDC_REPEAT_ALL )
    DEFINE_CMD_ID( CMD_LOOP_TRACK, IDC_REPEAT_TRACK )
    { "ID_EQ", CMD_EQ, IDC_EQ, "Equalizer" },
    { "ID_RATE", CMD_RATE, 0, "rate this song" },
    { "ID_TXT_TITLE", CMD_TXT_TITLE, 0, nullptr },
    { "ID_TXT_POS", CMD_TXT_POS, 0, nullptr },
    { "ID_TXT_VOL", CMD_TXT_VOL, 0, nullptr },
    { "ID_TXT_STATE", CMD_TXT_STATE, 0, nullptr },
    { "ID_TXT_INFO", CMD_TXT_INFO, 0, nullptr },
    { "ID_STEREO_STAT", ID_STEREO_STAT, 0, nullptr },

    // playlist commands
    { "ID_PL_UP", CMD_PL_UP, IDC_MOVE_UP, "Up" },
    { "ID_PL_DOWN", CMD_PL_DOWN, IDC_MOVE_DOWN, "Down" },
    { "ID_PL_ADD_FILE", CMD_PL_ADD_FILE, IDC_ADDFILE, "add File" },
    { "ID_PL_ADD_DIR", CMD_PL_ADD_DIR, IDC_ADDDIR, "add Folder" },
    { "ID_PL_ADD_URL", CMD_PL_ADD_URL, 0, "add Url" },
    { "ID_PL_OPEN_FILE", CMD_PL_OPEN_FILE, IDC_OPENFILE, "open File" },
    { "ID_PL_OPEN_DIR", CMD_PL_OPEN_DIR, IDC_OPENDIR, "open Folder" },
    { "ID_PL_OPEN_URL", CMD_PL_OPEN_URL, 0, "open Url" },
    { "ID_PL_DEL", CMD_PL_DEL, IDC_REMOVE, "delete" },
    { "ID_PL_PROPERTY", CMD_PL_PROPERTY, IDC_MEDIA_INFO, "Property" },
    { "ID_PL_NEW", CMD_PL_NEW, IDC_NEW_PL, "new Playlist" },
    { "ID_PL_LOAD", CMD_PL_LOAD, 0, "load Playlist" },
    { "ID_PL_SAVE", CMD_PL_SAVE, IDC_SAVE_PL, "save Playlist" },
    { "ID_PL_LIST", CMD_PL_LIST, 0, nullptr },

    // Media Guide View
    { "ID_MG_TREE_GUIDE", CMD_MG_TREE_GUIDE, 0, nullptr },
    { "ID_MG_MEDIA_LIST", CMD_MG_MEDIA_LIST, 0, nullptr },

    { "ID_ML_GUIDE", CMD_ML_GUIDE, 0, nullptr },
    { "ID_ML_BACK", CMD_ML_BACK, 0, "back" },
    { "ID_ML_MEDIA_LIST", ID_ML_MEDIA_LIST, 0, nullptr },

    { "ID_AD_TXT_LINK", ID_AD_TXT_LINK, 0, nullptr },
    { "ID_AD_IMG_LINK", ID_AD_IMG_LINK, 0, nullptr },

    { "ID_ALBUMART", ID_ALBUMART, 0, nullptr },

    { "CMD_TOGGLE_MP", CMD_TOGGLE_MP, 0, _TLM("Bring to front/Hide $Product$") },
    { "CMD_PREFERENCES", CMD_PREFERENCES, IDC_PREFERENCE, _TLM("Preferences") },
    { "CMD_RELOAD_LYR", CMD_RELOAD_LYR, 0, _TLM("Reload lyrics") },

    { "CMD_FLOATING_LYRICS", CMD_FLOATING_LYRICS, IDC_FLOATING_LYRICS, _TLM("Floating Lyrics") },

    DEFINE_CMD_ID( CMD_RATE_LYR_1, IDC_RATE_LYR_1 )
    DEFINE_CMD_ID( CMD_RATE_LYR_2, IDC_RATE_LYR_2 )
    DEFINE_CMD_ID( CMD_RATE_LYR_3, IDC_RATE_LYR_3 )
    DEFINE_CMD_ID( CMD_RATE_LYR_4, IDC_RATE_LYR_4 )
    DEFINE_CMD_ID( CMD_RATE_LYR_5, IDC_RATE_LYR_5 )
    DEFINE_CMD_ID( CMD_LDO_KARAOKE, IDC_LDO_KARAOKE )
    DEFINE_CMD_ID( CMD_ABOUT, IDC_ABOUT )
    DEFINE_CMD_ID( CMD_ADJUST_HUE, IDC_ADJUST_HUE )
    DEFINE_CMD_ID( CMD_BR_ALBUM_ART, IDC_BR_ALBUM_ART )
    DEFINE_CMD_ID( CMD_HELP_STATIC_LYR, IDC_HELP_STATIC_LYR )
    DEFINE_CMD_ID( CMD_LOGIN_VIA_IE, IDC_LOGIN_VIA_IE )
    DEFINE_CMD_ID( CMD_APPLY_ACCOUNT, IDC_APPLY_ACCOUNT )
    DEFINE_CMD_ID( CMD_EMAIL, IDC_EMAIL )
    DEFINE_CMD_ID( CMD_WEBHOME, IDC_WEBHOME )
    DEFINE_CMD_ID( CMD_ANTIAlIAS, IDC_ANTIAlIAS )
    DEFINE_CMD_ID( CMD_LDO_NORMAL, IDC_LDO_NORMAL )
    DEFINE_CMD_ID( CMD_LDO_FADE_IN, IDC_LDO_FADE_IN )
    DEFINE_CMD_ID( CMD_LDO_FADEOUT_BG, IDC_LDO_FADEOUT_BG )
    DEFINE_CMD_ID( CMD_LDO_AUTO, IDC_LDO_AUTO )
    DEFINE_CMD_ID( CMD_LDS_MULTI_LINE, IDC_LDS_MULTI_LINE )
    DEFINE_CMD_ID( CMD_LDS_STATIC_TXT, IDC_LDS_STATIC_TXT )
    DEFINE_CMD_ID( CMD_LDS_TWO_LINE, IDC_LDS_TWO_LINE )
    DEFINE_CMD_ID( CMD_LDS_SINGLE_LINE, IDC_LDS_SINGLE_LINE )
    DEFINE_CMD_ID( CMD_LDS_VOBSUB, IDC_LDS_VOBSUB )
    DEFINE_CMD_ID2( CMD_NO_SUITTABLE_LYRICS, IDC_NOT_MATCH, _TLM("&No Suitable Lyrics for the Song File") )
    DEFINE_CMD_ID2( CMD_INSTRUMENTAL_MUSIC, IDC_INSTRUMENTAL_MUSIC, _TLM("Instrumental Music, No Lyrics") )
    DEFINE_CMD_ID2( CMD_SEARCH_LYR_SUGGESTIONS, 0, _TLM("Lyrics search Suggestions") )
//    DEFINE_CMD_ID2( , , _TLM("") )
//    DEFINE_CMD_ID( ,  )

    { nullptr, 0, 0, nullptr },
};
