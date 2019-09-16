#ifndef FCODER_RYANB_BINDINGS
#define FCODER_RYANB_BINDINGS

#define EXTRA_KEYWORDS "4coder_ryanb_keywords.h"
#define EXTRA_PREPROPS "4coder_ryanb_preprops.h"

#include "4coder_default_include.cpp"

bool openAll = false;

/////////////////////////////////////////////////////////////////////////////
// CUSTOM HOOKS                                                            //
/////////////////////////////////////////////////////////////////////////////

START_HOOK_SIG(ryanb_start) {
    // default
    named_maps = named_maps_values;
    named_map_count = ArrayCount(named_maps_values);
    default_4coder_initialize(app);

    // custom
    Buffer_Identifier buffer = buffer_identifier(literal("*scratch*"));
    default_4coder_one_panel(app, buffer);

    begin_notepad_mode(app);
    change_theme(app, literal("DREADWARE"));

    Face_Description description = { };
    char fontName[] = "Telegrama Render";
    memset(description.font.name, 0, _countof(description.font.name));
    memcpy(description.font.name, fontName, _countof(fontName));
    description.pt_size = 14;
    description.hinting = true;
    change_global_face_by_description(app, description, true);

    global_config.enable_code_wrapping = false;
    global_config.enable_virtual_whitespace = false;

    parse_extension_line_to_extension_list(make_lit_string(".h.inl.cpp"), &global_config.code_exts);

    return (0);
}

OPEN_FILE_HOOK_SIG(ryanb_open_file) {
    default_file_settings(app, buffer_id);

    if (openAll) {
        openAll = false;
        open_all_code(app);
    }

    return (0);
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM COMMANDS                                                         //
/////////////////////////////////////////////////////////////////////////////

CUSTOM_COMMAND_SIG(ryanb_build) {
    save_all_dirty_buffers(app);

    View_Summary view = get_active_view(app, AccessAll);
    View_Summary buildView = get_view(app, build_footer_panel_view_id, AccessAll);
    if (!buildView.exists) {
        buildView = open_view(app, &view, ViewSplit_Bottom);
        new_view_settings(app, &buildView);
        view_set_split_proportion(app, &buildView, 0.4f);
        view_set_passive(app, &buildView, true);
        build_footer_panel_view_id = buildView.view_id;
    }

    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    Buffer_Summary buildBuffer = get_buffer(app, buildView.buffer_id, AccessAll);

    execute_standard_build(app, &buildView, &buffer);
    set_active_view(app, &buildView);
}
CUSTOM_COMMAND_SIG(ryanb_close_brackets) {
    View_Summary view = get_active_view(app, AccessOpen);

    int32_t position = view.cursor.pos;
    uint8_t characters[] = "{\n\n}";

    write_character_parameter(app, characters, _countof(characters) - 1);
    auto_tab_line_at_cursor(app);
    view_set_cursor(app, &view, seek_pos(position + 2), true);
    auto_tab_line_at_cursor(app);
}
CUSTOM_COMMAND_SIG(ryanb_command_prompt) {
    bool32 result;
    char commandSpace[255];

    Query_Bar command;
    command.prompt = make_lit_string("Command: ");
    command.string = make_fixed_width_string(commandSpace);
    result = query_user_string(app, &command);
    end_query_bar(app, &command, 0);
    if (!result) return;

    if (match(command.string, "build")) {
        ryanb_build(app);
    } else {
        Query_Bar error;
        error.prompt = make_lit_string("Unknown Command (press any key to return)");
        error.string = null_string;
        start_query_bar(app, &error, 0);
        get_user_input(app, EventOnAnyKey, 0);
        end_query_bar(app, &error, 0);
    }
}
CUSTOM_COMMAND_SIG(ryanb_duplicate_line) {
    duplicate_line(app);
    move_down(app);
}
CUSTOM_COMMAND_SIG(ryanb_goto_line) {
    goto_line(app);
    center_view(app);
}
CUSTOM_COMMAND_SIG(ryanb_interactive_open_all) {
    openAll = true;
    close_all_code(app);
    interactive_open(app);
}
CUSTOM_COMMAND_SIG(ryanb_kill_buffer) {
    View_Summary view = get_active_view(app, AccessAll);
    if (view.view_id == build_footer_panel_view_id) {
        close_build_footer_panel(app);
    } else {
        kill_buffer(app);
    }
}
CUSTOM_COMMAND_SIG(ryanb_kill_to_end_of_line) {
    View_Summary view = get_active_view(app, AccessOpen);

    int pos2 = view.cursor.pos;
    seek_end_of_line(app);
    refresh_view(app, &view);
    int pos1 = view.cursor.pos;

    Range range = make_range(pos1, pos2);
    if (pos1 == pos2) {
        range.max += 1;
    }

    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
    buffer_replace_range(app, &buffer, range.min, range.max, 0, 0);
    auto_tab_line_at_cursor(app);
}
CUSTOM_COMMAND_SIG(ryanb_move_line_down) {
    move_line_down(app);
    auto_tab_line_at_cursor(app);
}
CUSTOM_COMMAND_SIG(ryanb_move_line_up) {
    move_line_up(app);
    auto_tab_line_at_cursor(app);
}
CUSTOM_COMMAND_SIG(ryanb_page_down) {
    page_down(app);
    center_view(app);
}
CUSTOM_COMMAND_SIG(ryanb_page_up) {
    page_up(app);
    center_view(app);
}
CUSTOM_COMMAND_SIG(ryanb_seek_whitespace_up) {
    seek_whitespace_up_end_line(app);
    center_view(app);
}
CUSTOM_COMMAND_SIG(ryanb_seek_whitespace_down) {
    seek_whitespace_down_end_line(app);
    center_view(app);
}

/////////////////////////////////////////////////////////////////////////////
// MAIN BINDINGS FUNCTION                                                  //
/////////////////////////////////////////////////////////////////////////////

extern "C" int32_t get_bindings(void* data, int32_t size) {
    Bind_Helper context_ = begin_bind_helper(data, size);
    Bind_Helper *context = &context_;

    // initialize defaults
    set_all_default_hooks(context);
    default_keys(context);

    // set custom hooks
    set_start_hook(context, ryanb_start);
    set_open_file_hook(context, ryanb_open_file);

    // set custom bindings
    begin_map(context, mapid_global);
    {
        bind(context, key_esc, MDFR_NONE, close_build_footer_panel);      // esc              : close open build panel
        bind(context, key_f4,  MDFR_ALT,  exit_4coder);                   // alt + f4        : try to quit
        bind(context, key_f5,  MDFR_ALT,  open_color_tweaker);            // alt + f5        : open theme selection prompt
        bind(context, key_f11, MDFR_NONE, toggle_fullscreen);             // f11              : toggle full screen
        bind(context, key_f12, MDFR_NONE, ryanb_command_prompt);          // f12              : open custom command prompt
        bind(context, ',',     MDFR_CTRL, change_active_panel_backwards); // ctrl + ,         : change active panel bakcwards
        bind(context, '.',     MDFR_CTRL, change_active_panel);           // ctrl + .         : change active panel
        bind(context, '+',     MDFR_CTRL, open_panel_vsplit);             // ctrl + +         : open split panel
        bind(context, '-',     MDFR_CTRL, close_panel);                   // ctrl + -         : close split panel
        bind(context, 'o',     MDFR_CTRL, interactive_open);              // ctrl + o         : open file prompt
        bind(context, 'w',     MDFR_CTRL, interactive_switch_buffer);     // ctrl + w         : switch buffer prompt
        bind(context, 'P',     MDFR_CTRL, ryanb_command_prompt);          // ctrl + shift + p : open custom command prompt
        bind(context, 'q',     MDFR_CTRL, exit_4coder);                   // ctrl + q         : try to quit
        bind(context, 'b',     MDFR_CTRL, ryanb_build);                   // ctrl + b         : execute build and open build panel
    }
    end_map(context);

    begin_map(context, mapid_file);
    {
        bind_vanilla_keys(context, write_character);

        bind(context, key_f2,        MDFR_CTRL,  rename_file_query);          // ctrl + f2        : rename file prompt
        bind(context, key_del,       MDFR_SHIFT, delete_line);                // shift + del      : delete line
        bind(context, key_end,       MDFR_CTRL,  goto_end_of_file);           // ctrl + end       : seek bottom of file
        bind(context, key_home,      MDFR_CTRL,  goto_beginning_of_file);     // ctrl + home      : seek top of file
        bind(context, key_page_down, MDFR_NONE,  ryanb_page_down);            // page down        : page down and center view
        bind(context, key_page_up,   MDFR_NONE,  ryanb_page_up);              // page up          : page up and center view
        bind(context, key_up,        MDFR_ALT,   ryanb_move_line_up);         // alt + up         : move line up and auto-tab
        bind(context, key_up,        MDFR_CTRL,  ryanb_seek_whitespace_up);   // ctrl + up        : seek whitespace up and center view
        bind(context, key_down,      MDFR_ALT,   ryanb_move_line_down);       // alt + down       : move line down and auto-tab
        bind(context, key_down,      MDFR_CTRL,  ryanb_seek_whitespace_down); // ctrl + down      : seek whitespace down and center view
        bind(context, '*',           MDFR_CTRL,  ryanb_kill_buffer);          // ctrl + *         : close file or close build panel
        bind(context, ' ',           MDFR_CTRL,  center_view);                // ctrl + space     : center view
        bind(context, 'a',           MDFR_CTRL,  select_all);                 // ctrl + a         : select all
        bind(context, 'd',           MDFR_CTRL,  ryanb_duplicate_line);       // ctrl + d         : duplicate line and move down
        bind(context, 'g',           MDFR_CTRL,  ryanb_goto_line);            // ctrl + g         : go to line dialog and center view
        bind(context, 'h',           MDFR_CTRL,  query_replace);              // ctrl + h         : find and replace prompt
        bind(context, 'H',           MDFR_CTRL,  query_replace_identifier);   // ctrl + shift + h : find and replace prompt, with current identifier autopopulated as find
        bind(context, 'k',           MDFR_CTRL,  ryanb_kill_to_end_of_line);  // ctrl + k         : delete characters from cursor to end of line
        bind(context, 'O',           MDFR_CTRL,  ryanb_interactive_open_all); // ctrl + shift+ o  : open file prompt and open all code
        bind(context, 'q',           MDFR_CTRL,  exit_4coder);                // ctrl + q         : try to quit
        bind(context, 'w',           MDFR_CTRL,  interactive_switch_buffer);  // ctrl + w         : switch buffer prompt
        bind(context, 'Z',           MDFR_CTRL,  redo);                       // ctrl + shift + z : redo
    }
    end_map(context);

    begin_map(context, default_code_map);
    {
        inherit_map(context, mapid_file);

        bind(context, '\t', MDFR_NONE, auto_tab_line_at_cursor); // tab          : auto-tab current line
        bind(context, '{',  MDFR_NONE, ryanb_close_brackets);    // left bracket : auto-close brackets with new line and seek to blank line
    }
    end_map(context);

    return end_bind_helper(context);
}

#endif // FCODER_RYANB_BINDINGS
