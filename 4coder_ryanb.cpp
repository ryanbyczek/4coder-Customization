#include "4coder_default_include.cpp"
#include "generated/managed_id_metadata.cpp"

#pragma warning(push)
#pragma warning(disable:4189) // local variable is initialized but not referenced

// scope lines/annotation code and bat from: https://github.com/ryanfleury/4coder_fleury
// struct/function highlight code from: https://github.com/Skytrias/4files
// hex color preview code from: https://gist.github.com/thevaber/58bb6a1c03ebe56309545f413e898a95

// TODO: old features left to migrate...
// open/close panel (need numpad key bindings)
// kill buffer (need numpad key bindings)

// TODO: features to add...
// code folding (ctrol+m+o)
// rename symbol (f2) --> match only on full word and case (using replace_in_all_buffers, query_replace_identifier)
// spawn multiple cursors (ctrl+shift+down/up)
// goto definiton (ctrl+click or ctrl+alt+shift+b))
// function prototype helper
// type helper
// cut current scope (ctrl+shift+x)

/////////////////////////////////////////////////////////////////////////////
// CONSTANTS                                                               //
/////////////////////////////////////////////////////////////////////////////

namespace {
    static f32 line_number_margin   = 10.0f; // right-margin width for line number column, default was 2.0f
    static f32 scope_line_thickness = 2.0f;  // thickness of line connecting scope-start to scope-end
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM FUNCTIONS                                                        //
/////////////////////////////////////////////////////////////////////////////

function u32
ryanb_calculate_color_brightness(u32 color) {
    u32 r = ((color >> 16) & 0xFF);
    u32 g = ((color >> 8 ) & 0xFF);
    u32 b = ((color >> 0 ) & 0xFF);

    f32 brightness = sqrtf(
        (r * r * 0.241f)
      + (g * g * 0.691f)
      + (b * b * 0.068f)
    );

    return (u32)(brightness);
}

function FColor
ryanb_get_token_color_cpp(Token token) {
    FColor result = fcolor_id(defcolor_text_default);

    switch (token.kind) {
        case TokenBaseKind_Preprocessor:
            result = fcolor_id(defcolor_preproc);
        break;

        case TokenBaseKind_Keyword:
            result = fcolor_id(defcolor_keyword);
        break;

        case TokenBaseKind_Comment:
            result = fcolor_id(defcolor_comment);
        break;

        case TokenBaseKind_LiteralString:
            result = fcolor_id(defcolor_str_constant);
        break;

        case TokenBaseKind_LiteralInteger:
            result = fcolor_id(defcolor_int_constant);
        break;

        case TokenBaseKind_LiteralFloat:
            result = fcolor_id(defcolor_float_constant);
        break;

        default:
            switch (token.sub_kind) {
                case TokenCppKind_BraceOp:
                case TokenCppKind_BraceCl:
                case TokenCppKind_ParenOp:
                case TokenCppKind_ParenCl:
                case TokenCppKind_BrackOp:
                case TokenCppKind_BrackCl:
                case TokenCppKind_And:
                case TokenCppKind_AndAnd:
                case TokenCppKind_Or:
                case TokenCppKind_OrOr:
                case TokenCppKind_Colon:
				case TokenCppKind_ColonColon:
				case TokenCppKind_Semicolon:
                case TokenCppKind_Comma:
                case TokenCppKind_Dot:
                case TokenCppKind_DotDotDot:
                case TokenCppKind_Arrow:
                case TokenCppKind_Plus:
                case TokenCppKind_PlusPlus:
                case TokenCppKind_Minus:
                case TokenCppKind_MinusMinus:
                case TokenCppKind_Star:
                case TokenCppKind_Div:
                case TokenCppKind_Mod:
                case TokenCppKind_Ternary:
                case TokenCppKind_Eq:
                case TokenCppKind_PlusEq:
                case TokenCppKind_MinusEq:
                case TokenCppKind_StarEq:
                case TokenCppKind_DivEq:
                case TokenCppKind_ModEq:
                case TokenCppKind_Less:
                case TokenCppKind_LessEq:
                case TokenCppKind_Grtr:
                case TokenCppKind_GrtrEq:
                case TokenCppKind_EqEq:
                case TokenCppKind_NotEq:
                    result = fcolor_id(defcolor_keyword, 2);
                break;

                case TokenCppKind_LiteralTrue:
                case TokenCppKind_LiteralFalse:
                    result = fcolor_id(defcolor_bool_constant);
                break;

                case TokenCppKind_LiteralCharacter:
                case TokenCppKind_LiteralCharacterWide:
                case TokenCppKind_LiteralCharacterUTF8:
                case TokenCppKind_LiteralCharacterUTF16:
                case TokenCppKind_LiteralCharacterUTF32:
                    result = fcolor_id(defcolor_char_constant);
                break;

                case TokenCppKind_PPIncludeFile:
                    result = fcolor_id(defcolor_include);
                break;
            }
    }

    return (result);
}

Rect_f32_Pair
ryanb_layout_line_number_margin(Application_Links* app, Buffer_ID buffer, Rect_f32 rect, f32 digit_advance) {
    i64 lineCount = buffer_get_line_count(app, buffer);
    i64 digitCount = digit_count_from_integer(lineCount, 10);
    f32 width = (((f32)(digitCount) * digit_advance) + line_number_margin);
    return (rect_split_left_right(rect, width));
}

function u64
ryanb_string_find_first_non_whitespace(String_Const_u8 str) {
    u64 i;
    for (i = 0; i < str.size && str.str[i] <= 32; ++i);
    return (i);
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM RENDER FUNCTIONS                                                 //
/////////////////////////////////////////////////////////////////////////////

function void
ryanb_paint_tokens(Application_Links* app, Buffer_ID buffer, Text_Layout_ID text_layout_id) {
    Scratch_Block scratch(app);

    Range_i64 functionNameRange = {};

	Token_Array array = get_token_array_from_buffer(app, buffer);
    if (array.tokens != 0) {
        Range_i64 visibleRange = text_layout_get_visible_range(app, text_layout_id);
        i64 first = token_index_from_pos(&array, visibleRange.first);
        Token_Iterator_Array it = token_iterator_index(0, &array, first);
        for (;;) {
            Token* token = token_it_read(&it);

            if (token->pos >= visibleRange.one_past_last) {
                break;
            }

            switch (token->kind) {
                case TokenBaseKind_ParentheticalOpen:
                    // end function token range
                    if (functionNameRange.max == 0) {
                        functionNameRange.max = token->pos;
                    }
                break;

                case TokenBaseKind_Identifier:
                    // start function token range
                    if (functionNameRange.min == 0 && functionNameRange.max == 0) {
                        functionNameRange.min = token->pos;
                    }

                    // detect and paint struct tokens
                    if (token->sub_kind == TokenCppKind_Identifier) {
                        String_Const_u8 tokenString = push_token_lexeme(app, scratch, buffer, token);
                        for (Buffer_ID nextBuffer = get_buffer_next(app, 0, Access_Always); nextBuffer != 0; nextBuffer = get_buffer_next(app, nextBuffer, Access_Always)) {
                            Code_Index_File* file = code_index_get_file(nextBuffer);
                            if (file != 0) {
                                for (i32 i = 0; i < file->note_array.count; ++i) {
                                    Code_Index_Note* note = file->note_array.ptrs[i];
                                    switch (note->note_kind) {
                                        case CodeIndexNote_Type:
                                            if (string_match(note->text, tokenString, StringMatch_Exact)) {
                                                Range_i64 tokenRange;
                                                tokenRange.start = token->pos;
                                                tokenRange.end = token->pos + token->size;
                                                paint_text_color(app, text_layout_id, tokenRange, finalize_color(defcolor_keyword, 0));
                                                break;
                                            }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                break;

                default:
                    functionNameRange = {};
            }

            // paint function tokens
            if (functionNameRange.min != 0 && functionNameRange.max != 0) {
                paint_text_color(app, text_layout_id, functionNameRange, finalize_color(defcolor_keyword, 1));
                functionNameRange = {};
            }

            if (!token_it_inc_all(&it)) {
                break;
            }
        }
    }
}
function void
ryanb_draw_cpp_token_colors(Application_Links *app, Text_Layout_ID text_layout_id, Token_Array *array) {
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    i64 first_index = token_index_from_pos(array, visible_range.first);
    Token_Iterator_Array it = token_iterator_index(0, array, first_index);
    for (;;) {
        Token* token = token_it_read(&it);
        if (token->pos >= visible_range.one_past_last) {
            break;
        }
        //FColor color = get_token_color_cpp(*token);
        FColor color = ryanb_get_token_color_cpp(*token);
        ARGB_Color argb = fcolor_resolve(color);
        paint_text_color(app, text_layout_id, Ii64_size(token->pos, token->size), argb);
        if (!token_it_inc_all(&it)) {
            break;
        }
    }
}
function void
ryanb_draw_file_bar(Application_Links *app, View_ID view_id, Buffer_ID buffer, Face_ID face_id, Rect_f32 bar) {
    Scratch_Block scratch(app);

    draw_rectangle_fcolor(app, bar, 0.0f, fcolor_id(defcolor_bar));

    FColor color = fcolor_id(defcolor_base);

    Fancy_Line list = {};
    String_Const_u8 unique_name = push_buffer_unique_name(app, scratch, buffer);

    // show file name
    push_fancy_string(scratch, &list, color, unique_name);

    // show dirty flags
    Dirty_State dirty = buffer_get_dirty_state(app, buffer);
    u8 space[3];
    String_u8 str = Su8(space, 0, 3);
    if (dirty != 0) {
        string_append(&str, string_u8_litexpr(" "));
    }
    if (HasFlag(dirty, DirtyState_UnsavedChanges)) {
        string_append(&str, string_u8_litexpr("*"));
    }
    if (HasFlag(dirty, DirtyState_UnloadedChanges)) {
        string_append(&str, string_u8_litexpr("!"));
    }
    push_fancy_string(scratch, &list, color, str.string);

    // show cursor position
    i64 cursor_position = view_get_cursor_pos(app, view_id);
    Buffer_Cursor cursor = view_compute_cursor(app, view_id, seek_pos(cursor_position));
    push_fancy_stringf(scratch, &list, color, " | Line %3.lld, Col %3.lld", cursor.line, cursor.col);

    // show spaces/tabs
    if (global_config.indent_with_tabs) {
        push_fancy_string(scratch, &list, color, string_u8_litexpr(" | Spaces: Tab"));
    }
    else {
        push_fancy_stringf(scratch, &list, color, " | Spaces: %1.lld", global_config.indent_width);
    }

    // show encoding
    // TODO: get the actual encoding, looks like it always saves as UTF-8 for now...
    push_fancy_string(scratch, &list, color, string_u8_litexpr(" | UTF-8"));

    // show line endings
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Line_Ending_Kind* eol_setting = scope_attachment(app, scope, buffer_eol_setting, Line_Ending_Kind);
    switch (*eol_setting) {
        case LineEndingKind_Binary:
            push_fancy_string(scratch, &list, color, string_u8_litexpr(" | BIN"));
        break;

        case LineEndingKind_LF:
            push_fancy_string(scratch, &list, color, string_u8_litexpr(" | LF"));
        break;

        case LineEndingKind_CRLF:
            push_fancy_string(scratch, &list, color, string_u8_litexpr(" | CRLF"));
        break;
    }

    // show language type
    String_Const_u8 ext = string_file_extension(unique_name);
    if (string_match(ext, string_u8_litexpr("c"))   ||
        string_match(ext, string_u8_litexpr("cpp")) ||
        string_match(ext, string_u8_litexpr("h"))   ||
        string_match(ext, string_u8_litexpr("hpp")) ||
        string_match(ext, string_u8_litexpr("inl"))) {
        push_fancy_string(scratch, &list, color, string_u8_litexpr(" | C++"));
    }
    else if (string_match(ext, string_u8_litexpr("jai"))) {
        push_fancy_string(scratch, &list, color, string_u8_litexpr(" | JAI"));
    }
    else {
        push_fancy_string(scratch, &list, color, string_u8_litexpr(" | Plain Text"));
    }

    Vec2_f32 p = bar.p0 + V2f32(2.0f, 2.0f);
    draw_fancy_line(app, face_id, fcolor_zero(), &list, p);
}
function void
ryanb_draw_hex_color_preview(Application_Links* app, Buffer_ID buffer, Text_Layout_ID text_layout_id, i64 pos) {
    Scratch_Block scratch(app);

    Range_i64 range = enclose_pos_alpha_numeric(app, buffer, pos);
    String_Const_u8 identifier = push_buffer_range(app, scratch, buffer, range);
    if (identifier.size == 10) {
        if (identifier.str[0] == '0' && (identifier.str[1] == 'x' || identifier.str[1] == 'X')) {
            b32 isHex = true;
            for (u32 i = 0; (i < 8) && isHex; ++i) {
                char c = identifier.str[i + 2];
                isHex = ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9'));
            }

            if (isHex) {
                String_Const_u8 hex = string_substring(identifier, Ii64_size(2, 8));

                ARGB_Color hexColor = (u32)string_to_integer(hex, 16);
                draw_character_block(app, text_layout_id, Ii64_size(range.min, 10), 2.0f, hexColor);

                ARGB_Color textColor = ryanb_calculate_color_brightness(hexColor) < 128 ? 0xFFFFFFFF : 0xFF000000;
                paint_text_color(app, text_layout_id, range, textColor);
            }
        }
    }
}
function void
ryanb_draw_scope_annotations(Application_Links *app, Buffer_ID buffer, Text_Layout_ID text_layout_id, i64 pos, Rect_f32 rect) {
    Scratch_Block scratch(app);

    Face_ID faceID = get_face_id(app, buffer);
    Face_Metrics metrics = get_face_metrics(app, faceID);

    Token_Array tokenArray = get_token_array_from_buffer(app, buffer);
    if (tokenArray.tokens != 0) {
        Token_Iterator_Array it = token_iterator_pos(0, &tokenArray, pos);
        Token* token = token_it_read(&it);

        if (token != 0 && token->kind == TokenBaseKind_ScopeOpen) {
            pos = token->pos + token->size;
        }
        else if (token_it_dec_all(&it))
        {
            token = token_it_read(&it);
            if (token->kind == TokenBaseKind_ScopeClose && pos == (token->pos + token->size)) {
                pos = token->pos;
            }
        }
    }

    f32 xScopeOffset = rect.x0;

    Range_i64_Array ranges = get_enclosure_ranges(app, scratch, buffer, pos, RangeHighlightKind_CharacterHighlight);
    for (i32 i = ranges.count - 1; i >= 0; --i)
    {
        Range_i64 range = ranges.ranges[i];

        int nest = 0;
        Token* startToken = 0;
        Token_Iterator_Array it = token_iterator_pos(0, &tokenArray, range.start - 1);
        for (Token* token = token_it_read(&it); token && token_it_dec_non_whitespace(&it); token = token_it_read(&it)) {
            if (token->kind == TokenBaseKind_ParentheticalClose) {
                ++nest;
            }
            else if (token->kind == TokenBaseKind_ParentheticalOpen) {
                --nest;
            }
            else if (nest == 0 && (token->kind == TokenBaseKind_ScopeClose || token->kind == TokenBaseKind_StatementClose)) {
                break;
            }
            else if (nest == 0 && (token->kind == TokenBaseKind_Identifier || token->kind == TokenBaseKind_Keyword || token->kind == TokenBaseKind_Comment)) {
                startToken = token;
                break;
            }
        }

        if (startToken) {
            ARGB_Color color = finalize_color(defcolor_comment, 1);

            Rect_f32 rectStart = text_layout_character_on_screen(app, text_layout_id, range.start);
            Rect_f32 rectEnd   = text_layout_character_on_screen(app, text_layout_id, range.end);

            // draw annotation
            String_Const_u8 annotation = push_buffer_line(app, scratch, buffer, get_line_number_from_pos(app, buffer, startToken->pos));

            u64 annotationStart = ryanb_string_find_first_non_whitespace(annotation);
            annotation.str  += annotationStart;
            annotation.size -= annotationStart;

            if (annotation.str[annotation.size - 1] == 13) {
                --annotation.size;
            }
            if (annotation.str[annotation.size - 1] == 123) {
                --annotation.size;
            }

            Vec2_f32 annotationPosition = { rectEnd.x0, rectEnd.y0 };

            annotationPosition.x += metrics.space_advance;
            draw_string(app, faceID, string_u8_litexpr("<<"), annotationPosition, color);
            annotationPosition.x += (metrics.space_advance * 3);
            draw_string(app, faceID, annotation, annotationPosition, color);

            // draw scope lines
            Range_i64 visibleRange = text_layout_get_visible_range(app, text_layout_id);

            Rect_f32 scopeLine;
            scopeLine.x0 = xScopeOffset + (scope_line_thickness * 0.5f) + 1.0f;
            scopeLine.x1 = xScopeOffset + (scope_line_thickness * 1.5f);
            scopeLine.y0 = (range.start < visibleRange.start)
                ? 0.0f
                : rectStart.y0 + metrics.line_height;
            scopeLine.y1 = (range.end > visibleRange.end)
                ? 10000.0f
                : rectEnd.y0;

            draw_rectangle(app, scopeLine, 0.5f, color);

            xScopeOffset += (metrics.space_advance * global_config.indent_width);
        }
    }
}

function void
ryanb_render_buffer(Application_Links *app, View_ID view_id, Face_ID face_id, Buffer_ID buffer, Text_Layout_ID text_layout_id, Rect_f32 rect) {
    ProfileScope(app, "ryanb render buffer");

    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    Rect_f32 prev_clip = draw_set_clip(app, rect);

    // NOTE(allen): Token colorizing
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    if (token_array.tokens != 0) {
        //draw_cpp_token_colors(app, text_layout_id, &token_array);
        ryanb_draw_cpp_token_colors(app, text_layout_id, &token_array);

        // NOTE(allen): Scan for TODOs and NOTEs
        if (global_config.use_comment_keyword) {
            Comment_Highlight_Pair pairs[] = {
                {string_u8_litexpr("NOTE"), finalize_color(defcolor_comment_pop, 0)},
                {string_u8_litexpr("TODO"), finalize_color(defcolor_comment_pop, 1)},
            };
            draw_comment_highlights(app, buffer, text_layout_id,
                                    &token_array, pairs, ArrayCount(pairs));
        }
    }
    else{
        Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
        paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
    }

    i64 cursor_pos = view_correct_cursor(app, view_id);
    view_correct_mark(app, view_id);

    // NOTE(allen): Scope highlight
    if (global_config.use_scope_highlight) {
        Color_Array colors = finalize_color_array(defcolor_back_cycle);
        draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }

    if (global_config.use_error_highlight || global_config.use_jump_highlight) {
        // NOTE(allen): Error highlight
        String_Const_u8 name = string_u8_litexpr("*compilation*");
        Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);
        if (global_config.use_error_highlight) {
            draw_jump_highlights(app, buffer, text_layout_id, compilation_buffer,
                                 fcolor_id(defcolor_highlight_junk));
        }

        // NOTE(allen): Search highlight
        if (global_config.use_jump_highlight) {
            Buffer_ID jump_buffer = get_locked_jump_buffer(app);
            if (jump_buffer != compilation_buffer) {
                draw_jump_highlights(app, buffer, text_layout_id, jump_buffer,
                                     fcolor_id(defcolor_highlight_white));
            }
        }
    }

    // NOTE(allen): Color parens
    if (global_config.use_paren_helper) {
        Color_Array colors = finalize_color_array(defcolor_text_cycle);
        draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }

    // NOTE(allen): Line highlight
    if (global_config.highlight_line_at_cursor && is_active_view) {
        i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
        draw_line_highlight(app, text_layout_id, line_number, fcolor_id(defcolor_highlight_cursor_line));
    }

    ryanb_paint_tokens(app, buffer, text_layout_id);
    ryanb_draw_hex_color_preview(app, buffer, text_layout_id, cursor_pos);

    // NOTE(allen): Cursor shape
    Face_Metrics metrics = get_face_metrics(app, face_id);
    f32 cursor_roundness = (metrics.normal_advance*0.5f)*0.9f;
    f32 mark_thickness = 2.f;

    // NOTE(allen): Cursor
    switch (fcoder_mode) {
        case FCoderMode_Original:
        {
            draw_original_4coder_style_cursor_mark_highlight(app, view_id, is_active_view, buffer, text_layout_id, cursor_roundness, mark_thickness);
        }break;
        case FCoderMode_NotepadLike:
        {
            draw_notepad_style_cursor_highlight(app, view_id, buffer, text_layout_id, cursor_roundness);
        }break;
    }

    // NOTE(allen): put the actual text on the actual screen
    draw_text_layout_default(app, text_layout_id);

    ryanb_draw_scope_annotations(app, buffer, text_layout_id, cursor_pos, rect);

    draw_set_clip(app, prev_clip);
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM COMMANDS                                                         //
/////////////////////////////////////////////////////////////////////////////

// events
CUSTOM_COMMAND_SIG(ryanb_write_text) {
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    // skip when in comments
    i64 start = get_start_of_line_at_cursor(app, view, buffer);
    if (c_line_comment_starts_at_position(app, buffer, start)) {
        write_text_input(app);
        return;
    }

    i64 pos = view_get_cursor_pos(app, view);

    User_Input in = get_current_input(app);
    String_Const_u8 insert = to_writable(&in);

    b32 isQuote = false;
    b32 isClosingBrace = false;
    if (insert.str != 0 && insert.size > 0) {
        switch (insert.str[0]) {
            case '{':
                write_string(app, string_u8_litexpr("{}"));
            break;

            case '(':
                write_string(app, string_u8_litexpr("()"));
            break;

            case '[':
                write_string(app, string_u8_litexpr("[]"));
            break;

            case '}':
            case ')':
            case ']':
                isClosingBrace = true;
            break;

            case '\'':
            case '\"':
                isQuote = true;
            break;

            default:
                write_text_input(app);
                return;
            break;
        }
    }

    if (isClosingBrace || isQuote) {
        u8 nextCharacter = 0;
        buffer_read_range(app, buffer, Ii64(pos, pos + 1), &nextCharacter);

        if (insert.str[0] != nextCharacter) {
            write_string(app, SCu8(insert.str, 1));
            if (isQuote) {
                write_string(app, SCu8(insert.str, 1));
            }
        }
    }

    view_set_cursor_and_preferred_x(app, view, seek_pos(pos + 1));
}

// hotkeys
CUSTOM_COMMAND_SIG(ryanb_duplicate_line) {
    duplicate_line(app);
    move_down(app);
}
CUSTOM_COMMAND_SIG(ryanb_goto_line) {
    goto_line(app);
    center_view(app);
}
CUSTOM_COMMAND_SIG(ryanb_interactive_open_all_code) {
    close_all_code(app);
    interactive_open(app);
    open_all_code(app);
}
CUSTOM_COMMAND_SIG(ryanb_kill_buffer) {
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    if (view == build_footer_panel_view_id) {
        close_build_footer_panel(app);
    } else {
        kill_buffer(app);
    }
}
CUSTOM_COMMAND_SIG(ryanb_kill_to_end_of_line) {
    Scratch_Block scratch(app);
    current_view_boundary_delete(app, Scan_Forward, push_boundary_list(scratch, boundary_line));
}
CUSTOM_COMMAND_SIG(ryanb_move_down_to_blank_line) {
    move_down_to_blank_line(app);
    center_view(app);
}
CUSTOM_COMMAND_SIG(ryanb_move_left_token_boundary) {
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Backward, push_boundary_list(scratch, boundary_token, boundary_non_whitespace));
}
CUSTOM_COMMAND_SIG(ryanb_move_right_token_boundary) {
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Forward, push_boundary_list(scratch, boundary_token, boundary_non_whitespace));
}
CUSTOM_COMMAND_SIG(ryanb_move_up_to_blank_line) {
    move_up_to_blank_line(app);
    center_view(app);
}
CUSTOM_COMMAND_SIG(ryanb_page_down) {
    page_down(app);
    center_view(app);
}
CUSTOM_COMMAND_SIG(ryanb_page_up) {
    page_up(app);
    center_view(app);
}
CUSTOM_COMMAND_SIG(ryanb_rename_identifier) {
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    if (buffer != 0) {
        Scratch_Block scratch(app);
        i64 pos = view_get_cursor_pos(app, view);
        Range_i64 range = enclose_pos_alpha_numeric_underscore(app, buffer, pos);
        String_Const_u8 identifier = push_buffer_range(app, scratch, buffer, range);
        if (identifier.size != 0) {
            Query_Bar_Group group(app);
            Query_Bar replace = {};
            replace.prompt = string_u8_litexpr("Give a new name to ");
            replace.string = identifier;

            start_query_bar(app, &replace, 0);

            Query_Bar with = {};
            u8 with_space[1024];
            with.prompt = string_u8_litexpr("Name: ");
            with.string = SCu8(with_space, (u64)0);
            with.string_capacity = sizeof(with_space);

            if (query_user_string(app, &with)) {
                global_history_edit_group_begin(app);

                for (Buffer_ID next = get_buffer_next(app, 0, Access_ReadWriteVisible); next != 0; next = get_buffer_next(app, next, Access_ReadWriteVisible)) {
                    replace_in_range(app, next, buffer_range(app, buffer), replace.string, with.string);
                }

                global_history_edit_group_end(app);
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM HOOKS                                                            //
/////////////////////////////////////////////////////////////////////////////

void ryanb_render_caller(Application_Links* app, Frame_Info frame_info, View_ID view_id) {
    ProfileScope(app, "ryanb render caller");
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);

    Rect_f32 region = draw_background_and_margin(app, view_id, is_active_view);
    Rect_f32 prev_clip = draw_set_clip(app, region);

    Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
    Face_ID face_id = get_face_id(app, buffer);
    Face_Metrics face_metrics = get_face_metrics(app, face_id);
    f32 line_height = face_metrics.line_height;
    f32 digit_advance = face_metrics.decimal_digit_advance;

    // NOTE(allen): file bar
    b64 showing_file_bar = false;
    if (view_get_setting(app, view_id, ViewSetting_ShowFileBar, &showing_file_bar) && showing_file_bar) {
        Rect_f32_Pair pair = layout_file_bar_on_top(region, line_height);
        //draw_file_bar(app, view_id, buffer, face_id, pair.min);
        ryanb_draw_file_bar(app, view_id, buffer, face_id, pair.min);
        region = pair.max;
    }

    Buffer_Scroll scroll = view_get_buffer_scroll(app, view_id);

    Buffer_Point_Delta_Result delta = delta_apply(app, view_id, frame_info.animation_dt, scroll);
    if (!block_match_struct(&scroll.position, &delta.point)) {
        block_copy_struct(&scroll.position, &delta.point);
        view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
    }
    if (delta.still_animating) {
        animate_in_n_milliseconds(app, 0);
    }

    // NOTE(allen): query bars
    {
        Query_Bar *space[32];
        Query_Bar_Ptr_Array query_bars = {};
        query_bars.ptrs = space;
        if (get_active_query_bars(app, view_id, ArrayCount(space), &query_bars)) {
            for (i32 i = 0; i < query_bars.count; i += 1) {
                Rect_f32_Pair pair = layout_query_bar_on_top(region, line_height, 1);
                draw_query_bar(app, query_bars.ptrs[i], face_id, pair.min);
                region = pair.max;
            }
        }
    }

    // NOTE(allen): FPS hud
    if (show_fps_hud) {
        Rect_f32_Pair pair = layout_fps_hud_on_bottom(region, line_height);
        draw_fps_hud(app, frame_info, face_id, pair.max);
        region = pair.min;
        animate_in_n_milliseconds(app, 1000);
    }

    // NOTE(allen): layout line numbers
    Rect_f32 line_number_rect = {};
    if (global_config.show_line_number_margins) {
        //Rect_f32_Pair pair = layout_line_number_margin(app, buffer, region, digit_advance);
        Rect_f32_Pair pair = ryanb_layout_line_number_margin(app, buffer, region, digit_advance);
        line_number_rect = pair.min;
        region = pair.max;
    }

    // NOTE(allen): begin buffer render
    Buffer_Point buffer_point = scroll.position;
    Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);

    // NOTE(allen): draw line numbers
    if (global_config.show_line_number_margins) {
        draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
    }

    // NOTE(allen): draw the buffer
    //default_render_buffer(app, view_id, face_id, buffer, text_layout_id, region);
    ryanb_render_buffer(app, view_id, face_id, buffer, text_layout_id, region);

    text_layout_free(app, text_layout_id);
    draw_set_clip(app, prev_clip);
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM MAPPINGS                                                         //
/////////////////////////////////////////////////////////////////////////////

void ryanb_setup_default_mapping(Mapping* mapping, i64 global_id, i64 file_id, i64 code_id) {
    MappingScope();
    SelectMapping(mapping);

    // global bindings
    SelectMap(global_id);

    BindCore(default_startup,  CoreCode_Startup);
    BindCore(default_try_exit, CoreCode_TryExit);

    BindMouseWheel(mouse_wheel_scroll);

    Bind(exit_4coder,                     KeyCode_F4,     KeyCode_Alt);                     // alt + f4         : close 4coder
    Bind(toggle_fullscreen,               KeyCode_F11);                                     // f11              : toggle full screen
    Bind(close_build_footer_panel,        KeyCode_Escape);                                  // esc              : close open build panel
    Bind(change_active_panel,             KeyCode_Comma, KeyCode_Control);                  // ctrl + ,         : switch active panel
    //Bind(open_panel_vsplit,               KeyCode_Plus,  KeyCode_Control);                  // ctrl + +         : open split panel
    //Bind(close_panel,                     KeyCode_Minus,  KeyCode_Control);                 // ctrl + -         : close split panel
    Bind(build_in_build_panel,            KeyCode_B,     KeyCode_Control);                  // ctrl + b         : execute build in build panel
    Bind(interactive_new,                 KeyCode_N,     KeyCode_Control);                  // ctrl + n         : open new file prompt
    Bind(interactive_open,                KeyCode_O,     KeyCode_Control);                  // ctrl + o         : open existing file prompt
    Bind(ryanb_interactive_open_all_code, KeyCode_O,     KeyCode_Control, KeyCode_Shift);   // ctrl + shift + o : open existing file prompt and open all code near that file
    Bind(interactive_switch_buffer,       KeyCode_W,     KeyCode_Control);                  // ctrl + w         : switch buffer prompt
    Bind(exit_4coder,                     KeyCode_Q,     KeyCode_Control);                  // ctrl + q         : try to quit

    // plain text file bindings
    SelectMap(file_id);
    ParentMap(global_id);

    BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);

    BindMouse(click_set_cursor_and_mark, MouseCode_Left);
    BindMouseMove(click_set_cursor_if_lbutton);
    BindMouseRelease(click_set_cursor, MouseCode_Left);

    BindTextInput(write_text_input);

    Bind(rename_file_query,               KeyCode_F2,     KeyCode_Control);                // ctrl + f2        : rename file prompt
    Bind(backspace_char,                  KeyCode_Backspace);                              // backspace        : delete previous character
    Bind(delete_char,                     KeyCode_Delete);                                 // del              : delete next character
    Bind(delete_line,                     KeyCode_Delete, KeyCode_Shift);                  // shift + del      : delete line
    Bind(seek_end_of_line,                KeyCode_End);                                    // end              : seek line end
    Bind(goto_end_of_file,                KeyCode_End,    KeyCode_Control);                // ctrl + end       : seek bottom of file
    Bind(seek_beginning_of_line,          KeyCode_Home);                                   // home             : seek line start
    Bind(goto_beginning_of_file,          KeyCode_Home,   KeyCode_Control);                // ctrl + home      : seek top of file and
    Bind(ryanb_page_down,                 KeyCode_PageDown);                               // page down        : page down and center view
    Bind(ryanb_page_up,                   KeyCode_PageUp);                                 // page up          : page up and center view
    Bind(move_up,                         KeyCode_Up);                                     // up               : seek line up
    Bind(move_line_up,                    KeyCode_Up,     KeyCode_Alt);                    // alt + up         : move line up
    Bind(ryanb_move_up_to_blank_line,     KeyCode_Up,     KeyCode_Control);                // ctrl + up        : seek whitespace up and center view
    Bind(move_down,                       KeyCode_Down);                                   // down             : seek line down
    Bind(move_line_down,                  KeyCode_Down,   KeyCode_Alt);                    // alt + down       : move line down
    Bind(ryanb_move_down_to_blank_line,   KeyCode_Down,   KeyCode_Control);                // ctrl + down      : seek whitespace down and center view
    Bind(move_left,                       KeyCode_Left);                                   // left             : seek character left
    Bind(ryanb_move_left_token_boundary,  KeyCode_Left,   KeyCode_Control);                // ctrl + left      : seek token left
    Bind(move_right,                      KeyCode_Right);                                  // right            : seek character right
    Bind(ryanb_move_right_token_boundary, KeyCode_Right,  KeyCode_Control);                // ctrl + right     : seek token right
    //Bind(ryanb_kill_buffer,               KeyCode_Star,   KeyCode_Control);                // ctrl + *         : close file or close build panel
    Bind(center_view,                     KeyCode_Space,  KeyCode_Control);                // ctrl + space     : center view
    Bind(select_all,                      KeyCode_A,      KeyCode_Control);                // ctrl + a         : select all
    Bind(copy,                            KeyCode_C,      KeyCode_Control);                // ctrl + c         : copy selection
    Bind(ryanb_duplicate_line,            KeyCode_D,      KeyCode_Control);                // ctrl + d         : duplicate line and move down
    Bind(search,                          KeyCode_F,      KeyCode_Control);                // ctrl + f         : find in current buffer
    Bind(list_all_locations,              KeyCode_F,      KeyCode_Control, KeyCode_Shift); // ctrl + shift + f : find in every buffer
    Bind(ryanb_goto_line,                 KeyCode_G,      KeyCode_Control);                // ctrl + g         : go to line dialog and center view
    Bind(query_replace,                   KeyCode_H,      KeyCode_Control);                // ctrl + h         : find and replace prompt
    Bind(query_replace_identifier,        KeyCode_H,      KeyCode_Control, KeyCode_Shift); // ctrl + shift + h : find and replace prompt, with current identifier autopopulated as find
    Bind(ryanb_kill_to_end_of_line,       KeyCode_K,      KeyCode_Control);                // ctrl + k         : delete characters from cursor to end of line
    Bind(exit_4coder,                     KeyCode_Q,      KeyCode_Control);                // ctrl + q         : try to quit
    Bind(save,                            KeyCode_S,      KeyCode_Control);                // ctrl + s         : save current buffer
    Bind(save_all_dirty_buffers,          KeyCode_S,      KeyCode_Control, KeyCode_Shift); // ctrl + shift + s : save all buffers
    Bind(reopen,                          KeyCode_T,      KeyCode_Control, KeyCode_Shift); // ctrl + shift + t : reopen closed file
    Bind(paste,                           KeyCode_V,      KeyCode_Control);                // ctrl + v         : paste
    Bind(interactive_switch_buffer,       KeyCode_W,      KeyCode_Control);                // ctrl + w         : switch buffer prompt
    Bind(cut,                             KeyCode_X,      KeyCode_Control);                // ctrl + x         : cut current selection
    Bind(undo,                            KeyCode_Z,      KeyCode_Control);                // ctrl + z         : undo
    Bind(redo,                            KeyCode_Z,      KeyCode_Control, KeyCode_Shift); // ctrl + shift + z : redo

    // code file bindings
    SelectMap(code_id);
    ParentMap(file_id);

    BindTextInput(ryanb_write_text);

    Bind(ryanb_rename_identifier,         KeyCode_F2);                            // F2           : rename identifier in all open buffers
    Bind(ryanb_move_left_token_boundary,  KeyCode_Left,         KeyCode_Control); // ctrl + left  : seek token left
    Bind(ryanb_move_right_token_boundary, KeyCode_Right,        KeyCode_Control); // ctrl + right : seek token right
    Bind(word_complete,                   KeyCode_Tab);                           // tab          : auto-complete current word
    Bind(comment_line_toggle,             KeyCode_ForwardSlash, KeyCode_Control); // ctrl + /     : toggle line comment
    Bind(paste_and_indent,                KeyCode_V,            KeyCode_Control); // ctrl + v     : paste and indent
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM LAYER INIT                                                       //
/////////////////////////////////////////////////////////////////////////////

void custom_layer_init(Application_Links* app) {
    Thread_Context* tctx = get_thread_context(app);

    async_task_handler_init(app, &global_async_system);
    code_index_init();
    buffer_modified_set_init();
    Profile_Global_List* list = get_core_profile_list(app);
    ProfileThreadName(tctx, list, string_u8_litexpr("main"));
    initialize_managed_id_metadata(app);
    set_default_color_scheme(app);

    set_all_default_hooks(app);
    set_custom_hook(app, HookID_RenderCaller, ryanb_render_caller);
    mapping_init(tctx, &framework_mapping);
    //setup_default_mapping(&framework_mapping, mapid_global, mapid_file, mapid_code);
    ryanb_setup_default_mapping(&framework_mapping, mapid_global, mapid_file, mapid_code);
}

#pragma warning(pop)
