#include "4coder_default_include.cpp"
#include "generated/managed_id_metadata.cpp"

// scope lines/brace highlight/scope annotation code and bat from: https://github.com/ryanfleury/4coder_fleury
// struct/function highlight code from: https://github.com/Skytrias/4files
// hex color preview code from: https://gist.github.com/thevaber/58bb6a1c03ebe56309545f413e898a95

// NOTE(ryanb): changes in other files...
// 4coder_draw.cpp -> layout_line_number_margin changed to make fixed-width gutter

// TODO(ryanb): features to add...
// update build script to match my new one
// when cursor rests on a token, highlight all matching tokens within scope
// when cursor highlights a token, highlight all visible matching tokens
// replace menu should support paste
// better virtual whitespace for ternary operator
// code folding (ctrol+m+o)
// rename symbol (F2) --> match only on full word and case (using replace_in_all_buffers, query_replace_identifier)
//   ---> fleury_smart_replace_identifier
// allow cursor to exist outside of view
// spawn multiple cursors (ctrl+alt+down/up)
// type helper
// double-click should detect faster (4coder limitation?)


void set_mark_to_cursor(Application_Links* app){
    View_ID view = get_active_view(app, Access_ReadVisible);
    i64 cursor_pos = view_get_cursor_pos(app, view);
    view_set_mark(app, view, seek_pos(cursor_pos));
}

function void
ryanb_draw_tooltip(Application_Links *app, Rect_f32 rect) {
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    Face_ID face = get_face_id(app, buffer);
    Face_Metrics metrics = get_face_metrics(app, face);
    
    ARGB_Color background_color = fcolor_resolve(fcolor_id(defcolor_back));
    background_color &= 0x00FFFFFF;
    background_color |= 0xD0000000;
    
    ARGB_Color border_color = fcolor_resolve(fcolor_id(defcolor_margin_active));
    border_color &= 0x00FFFFFF;
    border_color |= 0xD0000000;
    
    f32 x_padding = metrics.space_advance;
    f32 y_padding = metrics.line_height * 0.5f;
    
    f32 border_thickness = 3.0f;
    rect.x0 -= ((x_padding * 0.5f) + border_thickness);
    rect.y0 -= ((y_padding * 0.5f) + border_thickness);
    rect.x1 += ((x_padding * 0.5f) + border_thickness);
    rect.y1 += ((y_padding * 0.5f));
    
    f32 tooltip_roundness = 4.0f;
    draw_rectangle(app, rect, tooltip_roundness, background_color);
    draw_rectangle_outline(app, rect, tooltip_roundness, border_thickness, border_color);
}

static void
ryanb_function_helper(Application_Links* app, View_ID view, Buffer_ID buffer, Text_Layout_ID text_layout_id, i64 pos) {
    ProfileScope(app, "ryanb function helper");
    
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    Token_Iterator_Array it;
    Token* token = 0;
    
    Rect_f32 view_rect = view_get_screen_rect(app, view);
    
    Face_ID face = get_face_id(app, buffer);
    Face_Metrics metrics = get_face_metrics(app, face);
    
    if(token_array.tokens != 0) {
        it = token_iterator_pos(0, &token_array, pos);
        token = token_it_read(&it);
        
        if(token != 0 && token->kind == TokenBaseKind_ParentheticalOpen) {
            pos = token->pos + token->size;
        }
        else {
            if (token_it_dec_all(&it)) {
                token = token_it_read(&it);
                if (token->kind == TokenBaseKind_ParentheticalClose && pos == token->pos + token->size) {
                    pos = token->pos;
                }
            }
        }
    }
    
    Scratch_Block scratch(app);
    Range_i64_Array ranges = { };
    {
        i64 temp_pos = pos;
        i32 max = 100;
        ranges.ranges = push_array(scratch, Range_i64, max);
        
        for (;;) {
            Range_i64 range = { };
            range.max = temp_pos;
            if (find_nest_side(app, buffer, temp_pos - 1, FindNest_Paren | FindNest_Balanced, Scan_Backward, NestDelim_Open, &range.start)) {
                ranges.ranges[ranges.count] = range;
                ranges.count += 1;
                temp_pos = range.first;
                if (ranges.count >= max) {
                    break;
                }
            }
            else {
                break;
            }
        }
    }
    
    for (int range_index = 0; range_index < ranges.count; ++range_index) {
        it = token_iterator_pos(0, &token_array, ranges.ranges[range_index].min);
        token_it_dec_non_whitespace(&it);
        token = token_it_read(&it);
        if(token->kind == TokenBaseKind_Identifier) {
            // highlight function name
            Range_i64 function_name_range = Ii64(token->pos, token->pos+token->size);
            String_Const_u8 function_name = push_buffer_range(app, scratch, buffer, function_name_range);
            ARGB_Color background_color = fcolor_resolve(fcolor_id(defcolor_highlight));
            background_color &= 0x00FFFFFF;
            background_color |= 0x60000000;
            draw_character_block(app, text_layout_id, function_name_range, 4.0f, background_color);
            
            // find active parameter
            int active_parameter_index = 0;
            static int last_active_parameter = -1;
            {
                it = token_iterator_pos(0, &token_array, function_name_range.min);
                int paren_nest = 0;
                for(;token_it_inc_non_whitespace(&it);)
                {
                    token = token_it_read(&it);
                    if(token->pos + token->size > pos) {
                        break;
                    }
                    
                    if(token->kind == TokenBaseKind_ParentheticalOpen) {
                        ++paren_nest;
                    }
                    else if(token->kind == TokenBaseKind_StatementClose) {
                        if(paren_nest == 1) {
                            ++active_parameter_index;
                        }
                    }
                    else if(token->kind == TokenBaseKind_ParentheticalClose) {
                        if(!--paren_nest) {
                            break;
                        }
                    }
                }
            }
            last_active_parameter = active_parameter_index;
            
            for(Buffer_ID buffer_it = get_buffer_next(app, 0, Access_Always); buffer_it != 0; buffer_it = get_buffer_next(app, buffer_it, Access_Always)) {
                Code_Index_File* file = code_index_get_file(buffer_it);
                if(file == 0) continue;
                for(i32 i = 0; i < file->note_array.count; i += 1) {
                    Code_Index_Note *note = file->note_array.ptrs[i];
                    
                    if((note->note_kind == CodeIndexNote_Function || note->note_kind == CodeIndexNote_Macro) && string_match(note->text, function_name)) {
                        Range_i64 function_def_range;
                        function_def_range.min = note->pos.min;
                        function_def_range.max = note->pos.max;
                        
                        Range_i64 highlight_parameter_range = {0};
                        
                        Token_Array find_token_array = get_token_array_from_buffer(app, buffer_it);
                        it = token_iterator_pos(0, &find_token_array, function_def_range.min);
                        
                        int paren_nest = 0;
                        int param_index = 0;
                        for(;token_it_inc_non_whitespace(&it);) {
                            token = token_it_read(&it);
                            if(token->kind == TokenBaseKind_ParentheticalOpen) {
                                if(++paren_nest == 1) {
                                    if(active_parameter_index == param_index) {
                                        highlight_parameter_range.min = token->pos+1;
                                    }
                                }
                            }
                            else if(token->kind == TokenBaseKind_ParentheticalClose) {
                                if(!--paren_nest) {
                                    function_def_range.max = token->pos + token->size;
                                    if(param_index == active_parameter_index) {
                                        highlight_parameter_range.max = token->pos;
                                    }
                                    break;
                                }
                            }
                            else if(token->kind == TokenBaseKind_StatementClose) {
                                if(param_index == active_parameter_index) {
                                    highlight_parameter_range.max = token->pos;
                                }
                                
                                if(paren_nest == 1) {
                                    ++param_index;
                                }
                                
                                if(param_index == active_parameter_index) {
                                    highlight_parameter_range.min = token->pos+1;
                                }
                            }
                        }
                        
                        if(highlight_parameter_range.min > function_def_range.min && function_def_range.max > highlight_parameter_range.max) {
                            String_Const_u8 function_def = push_buffer_range(app, scratch, buffer_it, function_def_range);
                            String_Const_u8 highlight_param = push_buffer_range(app, scratch, buffer_it, highlight_parameter_range);
                            
                            String_Const_u8 pre_highlight_def =
                            {
                                function_def.str,
                                (u64)(highlight_parameter_range.min - function_def_range.min),
                            };
                            
                            String_Const_u8 post_highlight_def =
                            {
                                function_def.str + highlight_parameter_range.max - function_def_range.min,
                                (u64)(function_def_range.max - highlight_parameter_range.max),
                            };
                            
                            Rect_f32 token_rect = text_layout_character_on_screen(app, text_layout_id, function_name_range.min);
                            i64 cursor_pos = view_get_cursor_pos(app, view);
                            Rect_f32 cursor_rect = text_layout_character_on_screen(app, text_layout_id, cursor_pos);
                            
                            Rect_f32 tooltip_rect =
                            {
                                token_rect.x0,
                                cursor_rect.y0 - (metrics.line_height * 1.5f),
                                token_rect.x0,
                                cursor_rect.y0 - (metrics.line_height * 0.5f) ,
                            };
                            
                            tooltip_rect.x1 += get_string_advance(app, face, highlight_param);
                            tooltip_rect.x1 += get_string_advance(app, face, pre_highlight_def);
                            tooltip_rect.x1 += get_string_advance(app, face, post_highlight_def);
                            
                            ryanb_draw_tooltip(app, tooltip_rect);
                            
                            Vec2_f32 text_position = { tooltip_rect.x0, tooltip_rect.y0 };
                            text_position = draw_string(app, face, pre_highlight_def, text_position, finalize_color(defcolor_comment, 0));
                            text_position = draw_string(app, face, highlight_param, text_position, finalize_color(defcolor_comment_pop, 1));
                            text_position = draw_string(app, face, post_highlight_def, text_position, finalize_color(defcolor_comment, 0));
                            
                            goto end_lookup;
                        }
                    }
                }
            }
            
            end_lookup:;
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// TYPES                                                                   //
/////////////////////////////////////////////////////////////////////////////

struct Bookmark {
    Buffer_ID buffer;
    View_ID   view;
    i64       pos;
    b32       is_valid;
};

/////////////////////////////////////////////////////////////////////////////
// CONSTANTS                                                               //
/////////////////////////////////////////////////////////////////////////////

namespace {
    static const u32 bookmark_max_count      = 256;
    static const f32 bookmark_auto_seconds   = 3.0f;  // number of seconds to wait until bookmarking the cursor position when the cursor isn't moving, default was 3.0f
    static const f32 build_buffer_height     = 0.33f; // percentage of the sceen height to use when opening the compilation buffer, default was 14 text-lines tall
    static const f32 cursor_fade_time        = 0.7f;  // time in seconds it takes to fade the cursor out
    static const f32 cursor_roundness        = 0.45f; // roundness of cursor highlight, default was 0.45f
    static const f32 cursor_thickness        = 3.0f;  // thickness of cursor when in notepad style mode, default was 1.0f
    static const f32 mark_thickness          = 2.0f;  // thickness of the mark in 4coder style mode, default was 2.0f
    static const f32 scope_line_thickness    = 1.0f;  // thickness of line connecting scope-start to scope-end
}

/////////////////////////////////////////////////////////////////////////////
// STATE                                                                   //
/////////////////////////////////////////////////////////////////////////////

namespace {
    Bookmark bookmarks[bookmark_max_count] = { };
    f32 auto_bookmark_time_remaining = bookmark_auto_seconds;
    i64 bookmark_cursor = 0;
    
    b32 show_function_helper = false;
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM FUNCTIONS                                                        //
/////////////////////////////////////////////////////////////////////////////

function void
ryanb_bookmark_cursor_inc(void) {
    bookmark_cursor++;
    if (bookmark_cursor >= bookmark_max_count) {
        bookmark_cursor = 0;
    }
}
function void
ryanb_bookmark_cursor_dec(void) {
    bookmark_cursor--;
    if (bookmark_cursor < 0) {
        bookmark_cursor = bookmark_max_count - 1;
    }
}

function b32
ryanb_is_cursor_at_bookmark(Application_Links* app) {
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 pos = view_get_cursor_pos(app, view);
    
    Bookmark* bookmark = &bookmarks[bookmark_cursor];
    
    return (bookmark->is_valid && bookmark->view == view && bookmark->buffer == buffer && bookmark->pos == pos);
}

function u32
ryanb_calculate_color_brightness(u32 color) {
    u32 r = ((color >> 16) & 0xFF);
    u32 g = ((color >> 8 ) & 0xFF);
    u32 b = ((color >> 0 ) & 0xFF);
    
    f32 brightness = sqrtf((r * r * 0.241f) + (g * g * 0.691f) + (b * b * 0.068f));
    
    return (u32)(brightness);
}

function FColor
ryanb_get_token_color_cpp(Token token) {
    FColor result = fcolor_id(defcolor_text_default);
    
    switch (token.kind) {
        case TokenBaseKind_Preprocessor: {
            result = fcolor_id(defcolor_preproc);
        }
        break;
        
        case TokenBaseKind_Keyword: {
            result = fcolor_id(defcolor_keyword);
        }
        break;
        
        case TokenBaseKind_Comment: {
            result = fcolor_id(defcolor_comment);
        }
        break;
        
        case TokenBaseKind_LiteralString: {
            result = fcolor_id(defcolor_str_constant);
        }
        break;
        
        case TokenBaseKind_LiteralInteger: {
            result = fcolor_id(defcolor_int_constant);
        }
        break;
        
        case TokenBaseKind_LiteralFloat: {
            result = fcolor_id(defcolor_float_constant);
        }
        break;
        
        default: {
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
                case TokenCppKind_NotEq: {
                    result = fcolor_id(defcolor_keyword, 2);
                }
                break;
                
                case TokenCppKind_LiteralTrue:
                case TokenCppKind_LiteralFalse: {
                    result = fcolor_id(defcolor_bool_constant);
                }
                break;
                
                case TokenCppKind_LiteralCharacter:
                case TokenCppKind_LiteralCharacterWide:
                case TokenCppKind_LiteralCharacterUTF8:
                case TokenCppKind_LiteralCharacterUTF16:
                case TokenCppKind_LiteralCharacterUTF32: {
                    result = fcolor_id(defcolor_char_constant);
                }
                break;
                
                case TokenCppKind_PPIncludeFile: {
                    result = fcolor_id(defcolor_include);
                }
                break;
            }
        }
    }
    
    return (result);
}

function u64
ryanb_string_find_first_non_whitespace(String_Const_u8 str) {
    u64 i = 0;
    for (i; (i < str.size) && (str.str[i] <= 32); ++i);
    return (i);
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM COMMANDS                                                         //
/////////////////////////////////////////////////////////////////////////////

// events
CUSTOM_COMMAND_SIG(ryanb_startup) {
    ProfileScope(app, "ryanb startup");
    User_Input input = get_current_input(app);
    if (match_core_code(&input, CoreCode_Startup)){
        String_Const_u8_Array file_names = input.event.core.file_names;
        load_themes_default_folder(app);
        default_4coder_initialize(app, file_names);
        default_4coder_side_by_side_panels(app, file_names);
        if (global_config.automatically_load_project) {
            load_project(app);
        }
    }
    
    system_set_fullscreen(true);
}

CUSTOM_COMMAND_SIG(ryanb_create_build_script) {
    Scratch_Block scratch(app);
    
    String_Const_u8 script_path = push_hot_directory(app, scratch);
    String_Const_u8 bat_file_name = push_u8_stringf(scratch, "%.*s/build.bat", string_expand(script_path));
    
    FILE* bat_script = fopen((char*)bat_file_name.str, "wb");
    if (bat_script != 0) {
        fprintf(bat_script, "@echo off\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, ":options\n");
        fprintf(bat_script, "set RELEASEBUILD=0\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, ":win32\n");
        fprintf(bat_script, "echo [WINDOWS]\n");
        fprintf(bat_script, "if %%RELEASEBUILD%% equ 1 echo Release Build\n");
        fprintf(bat_script, "if %%RELEASEBUILD%% neq 1 echo Internal Build\n");
        fprintf(bat_script, "echo.\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, "where /q cl\n");
        fprintf(bat_script, "if %%ERRORLEVEL%% equ 0 goto compiler_setup\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, ":msvc\n");
        fprintf(bat_script, "echo [MSVC]\n");
        fprintf(bat_script, "echo finding vcvarsall.bat...\n");
        fprintf(bat_script, "set VCVARS=C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\VC\\Auxiliary\\Build\\vcvarsall.bat\n");
        fprintf(bat_script, "if exist \"%%VCVARS%%\" goto vc_vars_found\n");
        fprintf(bat_script, "set VCVARS=C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvarsall.bat\n");
        fprintf(bat_script, "if exist \"%%VCVARS%%\" goto vc_vars_found\n");
        fprintf(bat_script, "set VCVARS=C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat\n");
        fprintf(bat_script, "if exist \"%%VCVARS%%\" goto vc_vars_found\n");
        fprintf(bat_script, "set VCVARS=C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Enterprise\\VC\\Auxiliary\\Build\\vcvarsall.bat\n");
        fprintf(bat_script, "if exist \"%%VCVARS%%\" goto vc_vars_found\n");
        fprintf(bat_script, "set VCVARS=C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Professional\\VC\\Auxiliary\\Build\\vcvarsall.bat\n");
        fprintf(bat_script, "if exist \"%%VCVARS%%\" goto vc_vars_found\n");
        fprintf(bat_script, "set VCVARS=C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat\n");
        fprintf(bat_script, "if exist \"%%VCVARS%%\" goto vc_vars_found\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, "echo unable to find vcvarsall.bat\n");
        fprintf(bat_script, "goto error\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, ":vc_vars_found\n");
        fprintf(bat_script, "echo found vcvarsall.bat: \"%%VCVARS%%\"\n");
        fprintf(bat_script, "call \"%%VCVARS%%\" x64 > NUL\n");
        fprintf(bat_script, "echo.\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, ":compiler_setup\n");
        fprintf(bat_script, "set COMPILERFLAGS=/diagnostics:column /EHa- /FC /fp:except- /fp:fast /Gm- /GR- /GS- /Gs9999999 /nologo /W4 /WX /Z7 /Zo\n");
        fprintf(bat_script, "if %%RELEASEBUILD%% equ 0 (\n");
        fprintf(bat_script, "    set COMPILERFLAGSEXE=%%COMPILERFLAGS%% /MTd /Od /DINTERNAL_BUILD\n");
        fprintf(bat_script, "    set COMPILERFLAGSDLL=%%COMPILERFLAGS%% /LDd /Od /DINTERNAL_BUILD\n");
        fprintf(bat_script, ")\n");
        fprintf(bat_script, "if %%RELEASEBUILD%% equ 1 (\n");
        fprintf(bat_script, "    set COMPILERFLAGSEXE=%%COMPILERFLAGS%% /MT /Oi /O2 /DRELEASE_BUILD\n");
        fprintf(bat_script, "    set COMPILERFLAGSDLL=%%COMPILERFLAGS%% /LD /Oi /O2 /DRELEASE_BUILD\n");
        fprintf(bat_script, ")\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, ":linker_setup\n");
        fprintf(bat_script, "set LINKERFLAGS=/INCREMENTAL:NO /MAP /NODEFAULTLIB /OPT:REF /STACK:0x100000,0x100000\n");
        fprintf(bat_script, "set LINKERFLAGSEXE=%%LINKERFLAGS%% /ENTRY:Main /SUBSYSTEM:WINDOWS\n");
        fprintf(bat_script, "set LINKERFLAGSDLL=%%LINKERFLAGS%% /NOENTRY /EXPORT:Update\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, "if not exist \"..\\bin\\win32\" mkdir \"..\\bin\\win32\"\n");
        fprintf(bat_script, "pushd \"..\\bin\\win32\"\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, "del *.exp 2> NUL\n");
        fprintf(bat_script, "del *.lib 2> NUL\n");
        fprintf(bat_script, "del *.map 2> NUL\n");
        fprintf(bat_script, "del *.o   2> NUL\n");
        fprintf(bat_script, "del *.obj 2> NUL\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, "echo.\n");
        fprintf(bat_script, "echo building game dll...\n");
        fprintf(bat_script, "cl %%COMPILERFLAGSDLL%% /EP /C \"..\\..\\src\\game.cpp\" > expanded_game.cpp\n");
        fprintf(bat_script, "cl %%COMPILERFLAGSDLL%% \"..\\..\\src\\game.cpp\" /link /PDB:game.pdb %%LINKERFLAGSDLL%%\n");
        fprintf(bat_script, "if %%ERRORLEVEL%% neq 0 goto error\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, "tasklist /FI \"IMAGENAME eq platform_win32.exe\" 2>NUL | find /I /N \"platform_win32.exe\">NUL");
        fprintf(bat_script, "if %%ERRORLEVEL%% equ 0 (");
        fprintf(bat_script, "echo.");
        fprintf(bat_script, "echo platform_win32.exe is running, skipping build...");
        fprintf(bat_script, "goto cleanup");
        fprintf(bat_script, ")");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, "echo.\n");
        fprintf(bat_script, "echo building game exe...\n");
        fprintf(bat_script, "cl %%COMPILERFLAGSEXE%% /EP /C \"..\\..\\src\\platform_win32.cpp\" > expanded_platform_win32.cpp\n");
        fprintf(bat_script, "cl %%COMPILERFLAGSEXE%% \"..\\..\\src\\platform_win32.cpp\" /link /PDB:platform_win32.pdb %%LINKERFLAGSEXE%% kernel32.lib\n");
        fprintf(bat_script, "if %%ERRORLEVEL%% neq 0 goto error\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, ":cleanup");
        fprintf(bat_script, "del *.exp 2> NUL\n");
        fprintf(bat_script, "del *.lib 2> NUL\n");
        fprintf(bat_script, "del *.o   2> NUL\n");
        fprintf(bat_script, "del *.obj 2> NUL\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, ":end\n");
        fprintf(bat_script, "echo.\n");
        fprintf(bat_script, "echo build complete!\n");
        fprintf(bat_script, "popd\n");
        fprintf(bat_script, "exit /b 0\n");
        fprintf(bat_script, "\n");
        
        fprintf(bat_script, ":error\n");
        fprintf(bat_script, "echo.\n");
        fprintf(bat_script, "echo build error, quitting...\n");
        fprintf(bat_script, "popd\n");
        fprintf(bat_script, "exit /b 1\n");
        fprintf(bat_script, "\n");
        
        fclose(bat_script);
    }
}

CUSTOM_COMMAND_SIG(ryanb_set_line_endings_crlf) {
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    rewrite_lines_to_crlf(app, buffer);
    set_eol_mode_to_crlf(app);
}

CUSTOM_COMMAND_SIG(ryanb_set_line_endings_lf) {
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    rewrite_lines_to_lf(app, buffer);
    set_eol_mode_to_lf(app);
}

// hotkeys
CUSTOM_COMMAND_SIG(ryanb_build_in_build_panel) {
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    
    View_ID build_view = 0;
    Buffer_ID build_buffer = get_comp_buffer(app);
    if (build_buffer != 0) {
        build_view = get_first_view_with_buffer(app, build_buffer);
    }
    if (build_view == 0) {
        if (build_footer_panel_view_id == 0) {
            build_footer_panel_view_id = open_view(app, view, ViewSplit_Bottom);
            new_view_settings(app, build_footer_panel_view_id);
            view_set_split(app, build_footer_panel_view_id, ViewSplitKind_Ratio, build_buffer_height);
            view_set_passive(app, build_footer_panel_view_id, true);
        }
        build_view = build_footer_panel_view_id;
    }
    
    standard_search_and_build(app, build_view, buffer);
    set_fancy_compilation_buffer_font(app);
    
    block_zero_struct(&prev_location);
    lock_jump_buffer(app, string_u8_litexpr("*compilation*"));
    
    view_set_active(app, view);
}

CUSTOM_COMMAND_SIG(ryanb_click_set_cursor_and_mark) {
    View_ID view = get_active_view(app, Access_ReadVisible);
    i64 cursor_pos = view_get_cursor_pos(app, view);
    
    Mouse_State mouse = get_mouse_state(app);
    i64 mouse_pos = view_pos_from_xy(app, view, V2f32(mouse.p));
    
    if (cursor_pos != mouse_pos) {
        view_set_cursor_and_preferred_x(app, view, seek_pos(mouse_pos));
        view_set_mark(app, view, seek_pos(mouse_pos));
    }
    else {
        Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
        Range_i64 range = enclose_pos_alpha_numeric_underscore(app, buffer, mouse_pos);
        view_set_cursor_and_preferred_x(app, view, seek_pos(range.min));
        no_mark_snap_to_cursor(app, view);
        view_set_mark(app, view, seek_pos(range.max));
    }
}

CUSTOM_COMMAND_SIG(ryanb_close_extra_panels) {
    close_build_footer_panel(app);
    show_function_helper = false;
}

CUSTOM_COMMAND_SIG(ryanb_command_lister) {
    View_ID view = get_this_ctx_view(app, Access_Always);
    if (view == 0) return;
    Buffer_ID buffer = buffer = view_get_buffer(app, view, Access_ReadVisible);
    
    Scratch_Block scratch(app);
    
    Lister_Block lister(app, scratch);
    lister_set_query(lister, string_u8_litexpr("Select a command..."));
    lister_set_default_handlers(lister);
    
    // apply theme
    {
        String_Const_u8 fcoder_extension = string_u8_litexpr(".4coder");
        String_Const_u8 file_name = push_buffer_unique_name(app, scratch, buffer);
        if (string_match(string_postfix(file_name, fcoder_extension.size), fcoder_extension)) {
            lister_add_item(lister, string_u8_litexpr("apply theme"), file_name, (void*)load_theme_current_buffer, 0);
        }
    }
    
    // create build script
    {
        String_Const_u8 hot_directory = push_hot_directory(app, scratch);
        lister_add_item(lister, string_u8_litexpr("create build script"), hot_directory, (void*)ryanb_create_build_script, 0);
    }
    
    // change line endings
    {
        Managed_Scope scope = buffer_get_managed_scope(app, buffer);
        Line_Ending_Kind* eol_setting = scope_attachment(app, scope, buffer_eol_setting, Line_Ending_Kind);
        switch (*eol_setting) {
            case LineEndingKind_Binary: {
                lister_add_item(lister, string_u8_litexpr("change line endings to CRLF"), string_u8_litexpr(""), (void*)ryanb_set_line_endings_crlf, 0);
                lister_add_item(lister, string_u8_litexpr("change line endings to LF"),   string_u8_litexpr(""), (void*)ryanb_set_line_endings_lf, 0);
            }
            break;
            
            case LineEndingKind_LF: {
                lister_add_item(lister, string_u8_litexpr("change line endings to CRLF"), string_u8_litexpr(""), (void*)ryanb_set_line_endings_crlf, 0);
            }
            break;
            
            case LineEndingKind_CRLF: {
                lister_add_item(lister, string_u8_litexpr("change line endings to LF"), string_u8_litexpr(""), (void*)ryanb_set_line_endings_lf, 0);
            }
            break;
        }
    }
    
    Lister_Result result = run_lister(app, lister);
    
    if (!result.canceled) {
        Custom_Command_Function* command = (Custom_Command_Function*)(result.user_data);
        if (command != 0) {
            view_enqueue_command_function(app, view, command);
        }
    }
}

CUSTOM_COMMAND_SIG(ryanb_set_bookmark) {
    if (ryanb_is_cursor_at_bookmark(app)) {
        return;
    }
    
    Bookmark* bookmark = &bookmarks[bookmark_cursor];
    if (bookmark->is_valid) {
        
        ryanb_bookmark_cursor_inc();
        bookmark = &bookmarks[bookmark_cursor];
    }
    
    View_ID view = get_active_view(app, Access_ReadVisible);
    
    bookmark->view = view;
    bookmark->buffer = view_get_buffer(app, view, Access_ReadVisible);
    bookmark->pos = view_get_cursor_pos(app, view);
    bookmark->is_valid = true;
    
    ryanb_bookmark_cursor_inc();
    bookmarks[bookmark_cursor].is_valid = false;
    ryanb_bookmark_cursor_dec();
}

CUSTOM_COMMAND_SIG(ryanb_goto_bookmark_next) {
    ryanb_bookmark_cursor_inc();
    Bookmark* bookmark = &bookmarks[bookmark_cursor];
    
    if (bookmark->is_valid) {
        View_ID view = bookmark->view;
        Buffer_ID buffer = bookmark->buffer;
        i64 pos = bookmark->pos;
        
        switch_to_existing_view(app, view, buffer);
        set_view_to_location(app, view, buffer, seek_pos(pos));
        view_set_active(app, view);
        center_view(app);
    }
    else {
        ryanb_bookmark_cursor_dec();
    }
}

CUSTOM_COMMAND_SIG(ryanb_goto_bookmark_prev) {
    if (ryanb_is_cursor_at_bookmark(app)) {
        ryanb_bookmark_cursor_dec();
    }
    Bookmark* bookmark = &bookmarks[bookmark_cursor];
    
    if (bookmark->is_valid) {
        View_ID view = bookmark->view;
        Buffer_ID buffer = bookmark->buffer;
        i64 pos = bookmark->pos;
        
        switch_to_existing_view(app, view, buffer);
        set_view_to_location(app, view, buffer, seek_pos(pos));
        view_set_active(app, view);
        center_view(app);
    }
    else {
        ryanb_bookmark_cursor_inc();
    }
}

CUSTOM_COMMAND_SIG(ryanb_goto_definition) {
    Scratch_Block scratch(app);
    
    View_ID view = get_active_view(app, Access_ReadVisible);
    
    String_Const_u8 query = push_token_or_word_under_active_cursor(app, scratch);
    
    code_index_lock();
    {
        for (Buffer_ID buffer = get_buffer_next(app, 0, Access_Always); buffer != 0; buffer = get_buffer_next(app, buffer, Access_Always)) {
            Code_Index_File* file = code_index_get_file(buffer);
            if (file == 0) continue;
            
            for (i32 i = 0; i < file->note_array.count; ++i) {
                Code_Index_Note* note = file->note_array.ptrs[i];
                if (string_match(note->text, query)) {
                    ryanb_set_bookmark(app);
                    switch_to_existing_view(app, view, buffer);
                    set_view_to_location(app, view, buffer, seek_pos(note->pos.first));
                    center_view(app);
                    ryanb_set_bookmark(app);
                    break;
                }
            }
        }
    }
    code_index_unlock();
}

CUSTOM_COMMAND_SIG(ryanb_click_goto_definition) {
    click_set_cursor_and_mark(app);
    ryanb_goto_definition(app);
}

CUSTOM_COMMAND_SIG(ryanb_goto_line) {
    goto_line(app);
    center_view(app);
    set_mark_to_cursor(app);
}

CUSTOM_COMMAND_SIG(ryanb_interactive_open_all_code) {
    close_all_code(app);
    interactive_open(app);
    open_all_code(app);
}

CUSTOM_COMMAND_SIG(ryanb_kill_to_end_of_line) {
    Scratch_Block scratch(app);
    current_view_boundary_delete(app, Scan_Forward, push_boundary_list(scratch, boundary_line));
}

CUSTOM_COMMAND_SIG(ryanb_list_all_locations) {
    Scratch_Block scratch(app);
    
    String_Const_u8 query_init = push_token_or_word_under_active_cursor(app, scratch);
    
    Query_Bar_Group group(app);
    Query_Bar bar = {};
    if (start_query_bar(app, &bar, 0) == 0) {
        return;
    }
    
    u8 bar_string_space[256];
    bar.string = SCu8(bar_string_space, query_init.size);
    block_copy(bar.string.str, query_init.str, query_init.size);
    bar.prompt = string_u8_litexpr("Search All: ");
    
    b32 do_clear_query = true;
    b32 do_list = false;
    
    User_Input in = { };
    for (;;) {
        in = get_next_input(app, EventPropertyGroup_AnyKeyboardEvent, EventProperty_Escape|EventProperty_ViewActivation);
        if (in.abort) {
            break;
        }
        if (match_key_code(&in, KeyCode_Return)) {
            do_list = true;
            break;
        }
        
        // allow paste
        b32 did_paste = false;
        if (match_key_code(&in, KeyCode_V)) {
            Input_Modifier_Set* mods = &in.event.key.modifiers;
            if (has_modifier(mods, KeyCode_Control)) {
                did_paste = true;
                String_Const_u8 paste = push_clipboard_index(app, scratch, 0, 0);
                if (paste.size > 0) {
                    bar.string.size = 0;
                    String_u8 bar_string = Su8(bar.string, sizeof(bar_string_space));
                    string_append(&bar_string, paste);
                    bar.string = bar_string.string;
                }
            }
        }
        
        if (!did_paste) {
            String_Const_u8 string = to_writable(&in);
            if (string.str != 0 && string.size > 0) {
                if (do_clear_query) {
                    do_clear_query = false;
                    if (bar.string.size > 0){
                        bar.string.size = 0;
                    }
                }
                String_u8 bar_string = Su8(bar.string, sizeof(bar_string_space));
                string_append(&bar_string, string);
                bar.string = bar_string.string;
            }
            else if (match_key_code(&in, KeyCode_Backspace)) {
                if (do_clear_query) {
                    do_clear_query = false;
                    if (bar.string.size > 0){
                        bar.string.size = 0;
                    }
                }
                else if (is_unmodified_key(&in.event)) {
                    bar.string = backspace_utf8(bar.string);
                }
                else if (has_modifier(&in.event.key.modifiers, KeyCode_Control)) {
                    if (bar.string.size > 0){
                        bar.string.size = 0;
                    }
                }
            }
            else {
                leave_current_input_unhandled(app);
            }
        }
    }
    
    if (do_list) {
        list_all_locations__generic(app, bar.string, ListAllLocationsFlag_CaseSensitive);
    }
}

CUSTOM_COMMAND_SIG(ryanb_move_left_token_boundary) {
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Backward, push_boundary_list(scratch, boundary_token, boundary_non_whitespace));
}

CUSTOM_COMMAND_SIG(ryanb_move_right_token_boundary) {
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Forward, push_boundary_list(scratch, boundary_token, boundary_non_whitespace));
}

CUSTOM_COMMAND_SIG(ryanb_open_panel_vsplit) {
    View_ID view = get_active_view(app, Access_Always);
    View_ID next_view = get_next_view_looped_primary_panels(app, view, Access_Always);
    
    if (view == next_view) {
        open_panel_vsplit(app);
    }
    else {
        Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
        view_set_active(app, next_view);
        view_set_buffer(app, next_view, buffer, 0);
    }
}

CUSTOM_COMMAND_SIG(ryanb_page_down) {
    page_down(app);
    center_view(app);
    set_mark_to_cursor(app);
}

CUSTOM_COMMAND_SIG(ryanb_page_up) {
    page_up(app);
    center_view(app);
    set_mark_to_cursor(app);
}

CUSTOM_COMMAND_SIG(ryanb_rename_identifier) {
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    if (buffer == 0) return;
    
    Scratch_Block scratch(app);
    
    String_Const_u8 identifier = push_token_or_word_under_active_cursor(app, scratch);
    if (identifier.size != 0) {
        Query_Bar_Group group(app);
        
        Query_Bar replace = { };
        replace.prompt = string_u8_litexpr("Give a new name to ");
        replace.string = identifier;
        
        start_query_bar(app, &replace, 0);
        
        u8 with_space[1024];
        
        Query_Bar with = { };
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

CUSTOM_COMMAND_SIG(ryanb_search) {
    Scratch_Block scratch(app);
    
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 buffer_size = buffer_get_size(app, buffer);
    
    i64 pos = view_get_cursor_pos(app, view);
    String_Const_u8 query_init = push_token_or_word_under_active_cursor(app, scratch);
    
    Scan_Direction scan = Scan_Forward;
    
    Query_Bar_Group group(app);
    Query_Bar bar = {};
    if (start_query_bar(app, &bar, 0) == 0) {
        return;
    }
    
    u8 bar_string_space[256];
    bar.string = SCu8(bar_string_space, query_init.size);
    block_copy(bar.string.str, query_init.str, query_init.size);
    bar.prompt = string_u8_litexpr("Search: ");
    
    b32 do_clear_query = true;
    u64 match_size = bar.string.size;
    
    User_Input in = { };
    for (;;) {
        isearch__update_highlight(app, view, Ii64_size(pos, match_size));
        center_view(app);
        
        in = get_next_input(app, EventPropertyGroup_AnyKeyboardEvent, EventProperty_Escape|EventProperty_ViewActivation|EventProperty_MouseButton);
        if (in.abort) {
            break;
        }
        
        String_Const_u8 string = to_writable(&in);
        
        b32 string_change = false;
        
        // allow paste
        if (match_key_code(&in, KeyCode_V)) {
            Input_Modifier_Set* mods = &in.event.key.modifiers;
            if (has_modifier(mods, KeyCode_Control)) {
                String_Const_u8 paste = push_clipboard_index(app, scratch, 0, 0);
                if (paste.size > 0) {
                    bar.string.size = 0;
                    String_u8 bar_string = Su8(bar.string, sizeof(bar_string_space));
                    string_append(&bar_string, paste);
                    bar.string = bar_string.string;
                    string_change = true;
                }
            }
        }
        
        if (string.str != 0 && string.size > 0) {
            if (do_clear_query) {
                do_clear_query = false;
                if (bar.string.size > 0) {
                    bar.string.size = 0;
                    string_change = true;
                }
            }
            String_u8 bar_string = Su8(bar.string, sizeof(bar_string_space));
            string_append(&bar_string, string);
            bar.string = bar_string.string;
            string_change = true;
        }
        else if (match_key_code(&in, KeyCode_Backspace)) {
            if (do_clear_query) {
                do_clear_query = false;
                if (bar.string.size > 0) {
                    bar.string.size = 0;
                    string_change = true;
                }
            }
            else if (is_unmodified_key(&in.event)) {
                u64 old_bar_string_size = bar.string.size;
                bar.string = backspace_utf8(bar.string);
                string_change = (bar.string.size < old_bar_string_size);
            }
            else if (has_modifier(&in.event.key.modifiers, KeyCode_Control)) {
                if (bar.string.size > 0){
                    bar.string.size = 0;
                    string_change = true;
                }
            }
        }
        
        b32 do_scan_action = false;
        Scan_Direction change_scan = scan;
        if (!string_change) {
            if (match_key_code(&in, KeyCode_Return) || match_key_code(&in, KeyCode_Tab)) {
                Input_Modifier_Set* mods = &in.event.key.modifiers;
                do_scan_action = true;
                
                if (has_modifier(mods, KeyCode_Shift)) {
                    change_scan = Scan_Backward;
                }
                else {
                    change_scan = Scan_Forward;
                }
            }
        }
        
        if (string_change){
            switch (scan){
                case Scan_Forward: {
                    i64 new_pos = 0;
                    seek_string_insensitive_forward(app, buffer, pos - 1, 0, bar.string, &new_pos);
                    if (new_pos < buffer_size){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }
                break;
                
                case Scan_Backward: {
                    i64 new_pos = 0;
                    seek_string_insensitive_backward(app, buffer, pos + 1, 0, bar.string, &new_pos);
                    if (new_pos >= 0){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }
                break;
            }
        }
        else if (do_scan_action) {
            scan = change_scan;
            switch (scan){
                case Scan_Forward: {
                    i64 new_pos = 0;
                    seek_string_insensitive_forward(app, buffer, pos, 0, bar.string, &new_pos);
                    if (new_pos < buffer_size){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }
                break;
                
                case Scan_Backward: {
                    i64 new_pos = 0;
                    seek_string_insensitive_backward(app, buffer, pos, 0, bar.string, &new_pos);
                    if (new_pos >= 0){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }
                break;
            }
        }
        else{
            leave_current_input_unhandled(app);
        }
    }
    
    view_disable_highlight_range(app, view);
    view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
}

CUSTOM_COMMAND_SIG(ryanb_toggle_function_helper) {
    show_function_helper = !show_function_helper;
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM RENDER FUNCTIONS                                                 //
/////////////////////////////////////////////////////////////////////////////

function void
ryanb_draw_line_number_margin(Application_Links *app, View_ID view_id, Buffer_ID buffer, Face_ID face_id, Text_Layout_ID text_layout_id, Rect_f32 margin){
    Scratch_Block scratch(app);
    
    Rect_f32 prev_clip = draw_set_clip(app, margin);
    draw_rectangle_fcolor(app, margin, 0.f, fcolor_id(defcolor_line_numbers_back));
    
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    
    i64 line_count = buffer_get_line_count(app, buffer);
    FColor line_color = fcolor_id(defcolor_line_numbers_text);
    
    Buffer_Cursor cursor = view_compute_cursor(app, view_id, seek_pos(visible_range.first));
    i64 line_number = cursor.line;
    for (;cursor.pos <= visible_range.one_past_last;){
        if (line_number > line_count){
            break;
        }
        Range_f32 line_y = text_layout_line_on_screen(app, text_layout_id, line_number);
        Vec2_f32 p = V2f32(margin.x0, line_y.min);
        Temp_Memory_Block temp(scratch);
        Fancy_String *string = push_fancy_stringf(scratch, 0, line_color, "  %4.lld", line_number);
        draw_fancy_string(app, face_id, fcolor_zero(), string, p);
        line_number += 1;
    }
    
    draw_set_clip(app, prev_clip);
}

function void
ryanb_draw_hex_color_preview(Application_Links* app, Buffer_ID buffer, Text_Layout_ID text_layout_id, i64 pos) {
    ProfileScope(app, "ryanb draw hex color preview");
    
    Scratch_Block scratch(app);
    
    Range_i64 range = enclose_pos_alpha_numeric(app, buffer, pos);
    String_Const_u8 token = push_buffer_range(app, scratch, buffer, range);
    if (token.size == 10) {
        if (token.str[0] == '0' && (token.str[1] == 'x' || token.str[1] == 'X')) {
            b32 is_hex = true;
            for (u32 i = 0; (i < 8) && is_hex; ++i) {
                char c = token.str[i + 2];
                is_hex = ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9'));
            }
            
            if (is_hex) {
                String_Const_u8 hex = string_substring(token, Ii64_size(2, 8));
                
                ARGB_Color hex_color = (u32)string_to_integer(hex, 16);
                draw_character_block(app, text_layout_id, Ii64_size(range.min, 10), 2.0f, hex_color);
                
                ARGB_Color text_color = ryanb_calculate_color_brightness(hex_color) < 128 ? 0xFFFFFFFF : 0xFF000000;
                paint_text_color(app, text_layout_id, range, text_color);
            }
        }
    }
}

function void
ryanb_draw_brace_highlight(Application_Links* app, Buffer_ID buffer, Text_Layout_ID text_layout_id, Token_Array token_array, i64 pos){
    ProfileScope(app, "ryanb draw scope highlight");
    
    Token_Iterator_Array it = token_iterator_pos(0, &token_array, pos);
    Token* token = token_it_read(&it);
    if(token != 0 && token->kind == TokenBaseKind_ScopeOpen) {
        pos = token->pos + token->size;
    }
    else {
        if (token_it_dec_all(&it)) {
            token = token_it_read(&it);
            if (token->kind == TokenBaseKind_ScopeClose && pos == token->pos + token->size) {
                pos = token->pos;
            }
        }
    }
    
    Range_i64 range;
    if (find_surrounding_nest(app, buffer, pos, FindNest_Scope, &range)) {
        ARGB_Color color;
        
        color = finalize_color(defcolor_text_cycle, 0);
        draw_character_block(app, text_layout_id, range.min, 0.0f, color);
        draw_character_block(app, text_layout_id, (range.max - 1), 0.0f, color);
        
        color = fcolor_resolve(fcolor_id(defcolor_back));
        paint_text_color_pos(app, text_layout_id, range.min, color);
        paint_text_color_pos(app, text_layout_id, (range.max - 1), color);
    }
}

function void
ryanb_draw_scope_highlight(Application_Links* app, Buffer_ID buffer, Text_Layout_ID text_layout_id, Token_Array token_array, i64 pos){
    ProfileScope(app, "ryanb draw scope highlight");
    
    Token_Iterator_Array it = token_iterator_pos(0, &token_array, pos);
    Token* token = token_it_read(&it);
    if(token != 0 && token->kind == TokenBaseKind_ScopeOpen) {
        pos = token->pos + token->size;
    }
    else {
        if (token_it_dec_all(&it)) {
            token = token_it_read(&it);
            if (token->kind == TokenBaseKind_ScopeClose && pos == token->pos + token->size) {
                pos = token->pos;
            }
        }
    }
    
    Range_i64 range;
    if (find_surrounding_nest(app, buffer, pos, FindNest_Scope, &range)) {
        ARGB_Color color = finalize_color(defcolor_back_cycle, 0);
        Range_i64 lines = get_line_range_from_pos_range(app, buffer, range);
        draw_line_highlight(app, text_layout_id, lines, color);
    }
}

function void
ryanb_draw_paren_highlight(Application_Links* app, Buffer_ID buffer, Text_Layout_ID text_layout_id, Token_Array token_array, i64 pos){
    ProfileScope(app, "ryanb draw paren highlight");
    
    Token_Iterator_Array it = token_iterator_pos(0, &token_array, pos);
    Token* token = token_it_read(&it);
    if(token != 0 && token->kind == TokenBaseKind_ScopeOpen) {
        pos = token->pos + token->size;
    }
    else {
        if (token_it_dec_all(&it)) {
            token = token_it_read(&it);
            if (token->kind == TokenBaseKind_ParentheticalClose && pos == token->pos + token->size) {
                pos = token->pos;
            }
        }
    }
    
    Range_i64 range;
    if (find_surrounding_nest(app, buffer, pos, FindNest_Paren, &range)) {
        ARGB_Color color = finalize_color(defcolor_text_cycle, 1);
        draw_character_block(app, text_layout_id, range.min, 0.0f, color);
        draw_character_block(app, text_layout_id, (range.max - 1), 0.0f, color);
        
        color = fcolor_resolve(fcolor_id(defcolor_back));
        paint_text_color_pos(app, text_layout_id, range.min, color);
        paint_text_color_pos(app, text_layout_id, (range.max - 1), color);
    }
}

function void
ryanb_draw_tokens(Application_Links* app, Buffer_ID buffer, Text_Layout_ID text_layout_id, Token_Array token_array) {
    ProfileScope(app, "ryanb paint tokens");
    
    Scratch_Block scratch(app);
    
    Range_i64 function_name_range = { };
    if (token_array.tokens != 0) {
        Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
        i64 first = token_index_from_pos(&token_array, visible_range.first);
        Token_Iterator_Array it = token_iterator_index(0, &token_array, first);
        for (;;) {
            Token* token = token_it_read(&it);
            
            if (token->pos >= visible_range.one_past_last) {
                break;
            }
            
            // paint keyword tokens
            FColor color = ryanb_get_token_color_cpp(*token);
            ARGB_Color argb = fcolor_resolve(color);
            paint_text_color(app, text_layout_id, Ii64_size(token->pos, token->size), argb);
            
            // search function tokens
            switch (token->kind) {
                case TokenBaseKind_ParentheticalOpen: {
                    // end function token range
                    if (function_name_range.max == 0) {
                        function_name_range.max = token->pos;
                    }
                }
                break;
                
                case TokenBaseKind_Identifier: {
                    // start function token range
                    if (function_name_range.min == 0 && function_name_range.max == 0) {
                        function_name_range.min = token->pos;
                    }
                    
                    // detect and paint struct tokens
                    if (token->sub_kind == TokenCppKind_Identifier) {
                        String_Const_u8 tokenString = push_token_lexeme(app, scratch, buffer, token);
                        for (Buffer_ID nextBuffer = get_buffer_next(app, 0, Access_Always); nextBuffer != 0; nextBuffer = get_buffer_next(app, nextBuffer, Access_Always)) {
                            Code_Index_File* file = code_index_get_file(nextBuffer);
                            if (file == 0) continue;
                            
                            for (i32 i = 0; i < file->note_array.count; ++i) {
                                Code_Index_Note* note = file->note_array.ptrs[i];
                                switch (note->note_kind) {
                                    case CodeIndexNote_Type: {
                                        if (string_match(note->text, tokenString, StringMatch_Exact)) {
                                            Range_i64 tokenRange = Ii64_size(token->pos, token->size);
                                            paint_text_color(app, text_layout_id, tokenRange, finalize_color(defcolor_keyword, 1));
                                            break;
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
                break;
                
                default: {
                    function_name_range = { };
                }
            }
            
            // paint function tokens
            if (function_name_range.min != 0 && function_name_range.max != 0) {
                paint_text_color(app, text_layout_id, function_name_range, finalize_color(defcolor_keyword, 1));
                function_name_range = { };
            }
            
            if (!token_it_inc_all(&it)) {
                break;
            }
        }
    }
}

function void
ryanb_draw_cpp_token_colors(Application_Links *app, Text_Layout_ID text_layout_id, Token_Array array) {
    ProfileScope(app, "ryanb draw cpp token colors");
    
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    i64 first_index = token_index_from_pos(&array, visible_range.first);
    Token_Iterator_Array it = token_iterator_index(0, &array, first_index);
    for (;;) {
        Token* token = token_it_read(&it);
        if (token->pos >= visible_range.one_past_last) {
            break;
        }
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
    ProfileScope(app, "ryanb draw file bar");
    
    Scratch_Block scratch(app);
    
    draw_rectangle_fcolor(app, bar, 0.0f, fcolor_id(defcolor_bar));
    
    FColor color = fcolor_id(defcolor_base);
    
    Fancy_Line list = { };
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
    push_fancy_string(scratch, &list, fcolor_id(defcolor_highlight_junk), str.string);
    
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
        case LineEndingKind_Binary: {
            push_fancy_string(scratch, &list, color, string_u8_litexpr(" | BIN"));
        }
        break;
        
        case LineEndingKind_LF: {
            push_fancy_string(scratch, &list, color, string_u8_litexpr(" | LF"));
        }
        break;
        
        case LineEndingKind_CRLF: {
            push_fancy_string(scratch, &list, color, string_u8_litexpr(" | CRLF"));
        }
        break;
    }
    
    // show language type
    String_Const_u8 ext = string_file_extension(unique_name);
    if (string_match(ext, string_u8_litexpr("c"))   ||
        string_match(ext, string_u8_litexpr("cc"))  ||
        string_match(ext, string_u8_litexpr("cpp")) ||
        string_match(ext, string_u8_litexpr("h"))   ||
        string_match(ext, string_u8_litexpr("hpp"))) {
        push_fancy_string(scratch, &list, color, string_u8_litexpr(" | C++"));
    }
    else if (string_match(ext, string_u8_litexpr("cs"))) {
        push_fancy_string(scratch, &list, color, string_u8_litexpr(" | C#"));
    }
    else {
        push_fancy_string(scratch, &list, color, string_u8_litexpr(" | Plain Text"));
    }
    
    Vec2_f32 p = bar.p0 + V2f32(2.0f, 2.0f);
    draw_fancy_line(app, face_id, fcolor_zero(), &list, p);
}

function void
ryanb_draw_notepad_style_cursor_highlight(Application_Links* app, Frame_Info frame_info, View_ID view_id, b32 is_active_view, Buffer_ID buffer, Text_Layout_ID text_layout_id, f32 roundness) {
    ProfileScope(app, "ryanb draw notepad style cursor highlight");
    
    static f32 accumulator = 0.0f;
    static i64 last_cursor_pos = view_get_cursor_pos(app, view_id);
    
    if (!draw_highlight_range(app, view_id, buffer, text_layout_id, roundness)) {
        i64 cursor_pos = view_get_cursor_pos(app, view_id);
        i64 mark_pos = view_get_mark_pos(app, view_id);
        
        if (view_id == get_active_view(app, Access_Always)) {
            if (cursor_pos != mark_pos) {
                Range_i64 range = Ii64(cursor_pos, mark_pos);
                draw_character_block(app, text_layout_id, range, roundness, fcolor_id(defcolor_highlight));
                paint_text_color_fcolor(app, text_layout_id, range, fcolor_id(defcolor_at_highlight));
            }
            
            if (accumulator <= 0.0f) {
                last_cursor_pos = cursor_pos;
            }
            
            animate_in_n_milliseconds(app, 0);
            
            accumulator += frame_info.literal_dt;
            if (accumulator >= cursor_fade_time || last_cursor_pos != cursor_pos) {
                accumulator = 0.0f;
            }
            
            auto_bookmark_time_remaining -= frame_info.literal_dt;
            if (last_cursor_pos != cursor_pos) {
                auto_bookmark_time_remaining = bookmark_auto_seconds;
            }
            if (auto_bookmark_time_remaining <= 0.0f) {
                auto_bookmark_time_remaining = bookmark_auto_seconds;
                ryanb_set_bookmark(app);
            }
            
            Rect_f32 rect = text_layout_character_on_screen(app, text_layout_id, cursor_pos);
            rect.x0 -= 1;
            rect.x1 = rect.x0 + cursor_thickness;
            
            u32 alpha = (u32)(((1.0f - (accumulator / cursor_fade_time)) * 255.0f) + 0.5f);
            u32 color = (fcolor_resolve(fcolor_id(defcolor_cursor)) & 0x00FFFFFF) | (alpha << 24);
            draw_rectangle(app, rect, 0.0f, color);
        }
    }
}

function void
ryanb_draw_scope_annotations(Application_Links *app, Buffer_ID buffer, Text_Layout_ID text_layout_id, Face_ID face_id, Face_Metrics metrics, i64 pos, Rect_f32 rect) {
    ProfileScope(app, "ryanb draw scope annotations");
    
    Scratch_Block scratch(app);
    
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    if (token_array.tokens != 0) {
        Token_Iterator_Array it = token_iterator_pos(0, &token_array, pos);
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
    
    f32 scope_offset_x = rect.x0;
    Range_i64_Array ranges = get_enclosure_ranges(app, scratch, buffer, pos, RangeHighlightKind_CharacterHighlight);
    for (i32 i = ranges.count - 1; i >= 0; --i) {
        Range_i64 range = ranges.ranges[i];
        
        int nest = 0;
        Token* start_token = 0;
        Token_Iterator_Array it = token_iterator_pos(0, &token_array, range.start - 1);
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
                start_token = token;
                break;
            }
        }
        
        if (start_token) {
            ARGB_Color color = finalize_color(defcolor_comment, 1);
            
            Rect_f32 rect_start = text_layout_character_on_screen(app, text_layout_id, range.start);
            Rect_f32 rect_end   = text_layout_character_on_screen(app, text_layout_id, range.end);
            
            Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
            
            // draw scope line
            Rect_f32 scope_line;
            scope_line.x0 = scope_offset_x;
            scope_line.x1 = scope_line.x0 + scope_line_thickness;
            scope_line.y0 = (range.start < visible_range.start) ? 0.0f : rect_start.y0 + metrics.line_height;
            scope_line.y1 = (range.end > visible_range.end) ? 10000.0f : rect_end.y0;
            draw_rectangle(app, scope_line, 0.0f, color);
            
            // draw scope annotation
            if (i == 0) {
                i64 start_line_number = get_line_number_from_pos(app, buffer, range.start);
                i64 end_line_number = get_line_number_from_pos(app, buffer, range.end);
                if (start_line_number != end_line_number) {
                    u8 c = buffer_get_char(app, buffer, range.end);
                    i64 line_count = buffer_get_line_count(app, buffer);
                    if (line_count == end_line_number || c == '\r' || c == '\n') {
                        String_Const_u8 annotation = push_buffer_line(app, scratch, buffer, get_line_number_from_pos(app, buffer, start_token->pos));
                        
                        u64 annotation_start = ryanb_string_find_first_non_whitespace(annotation);
                        annotation.str  += annotation_start;
                        annotation.size -= annotation_start;
                        
                        if (annotation.str[annotation.size - 1] == 13) {
                            --annotation.size;
                        }
                        if (annotation.str[annotation.size - 1] == 123) {
                            --annotation.size;
                        }
                        
                        Vec2_f32 annotation_position = { rect_end.x0, rect_end.y0 };
                        
                        annotation_position.x += metrics.space_advance;
                        draw_string(app, face_id, string_u8_litexpr("<<"), annotation_position, color);
                        annotation_position.x += (metrics.space_advance * 3);
                        draw_string(app, face_id, annotation, annotation_position, color);
                    }
                }
            }
            
            scope_offset_x += (metrics.space_advance * global_config.indent_width);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM RENDERER                                                         //
/////////////////////////////////////////////////////////////////////////////

void ryanb_render_buffer(Application_Links *app, Frame_Info frame_info, View_ID view_id, b32 is_active_view, Face_ID face_id, Buffer_ID buffer, Text_Layout_ID text_layout_id, Rect_f32 rect) {
    ProfileScope(app, "ryanb render buffer");
    
    Rect_f32 prev_clip = draw_set_clip(app, rect);
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    
    // NOTE(allen): Token colorizing
    if (token_array.tokens != 0) {
        ryanb_draw_tokens(app, buffer, text_layout_id, token_array);
        if (global_config.use_comment_keyword) {
            Comment_Highlight_Pair pairs[] = {
                { string_u8_litexpr("NOTE"), finalize_color(defcolor_comment_pop, 0) },
                { string_u8_litexpr("TODO"), finalize_color(defcolor_comment_pop, 1) },
            };
            draw_comment_highlights(app, buffer, text_layout_id, &token_array, pairs, ArrayCount(pairs));
        }
    }
    else {
        Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
        paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
    }
    
    i64 cursor_pos = view_correct_cursor(app, view_id);
    view_correct_mark(app, view_id);
    
    // NOTE(allen): Scope highlight
    if (global_config.use_scope_highlight) {
        //Color_Array colors = finalize_color_array(defcolor_back_cycle);
        //draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
        ryanb_draw_scope_highlight(app, buffer, text_layout_id, token_array, cursor_pos);
    }
    
    // NOTE(ryanb): Jump highlight
    if (global_config.use_error_highlight || global_config.use_jump_highlight) {
        // NOTE(allen): Error highlight
        String_Const_u8 name = string_u8_litexpr("*compilation*");
        Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);
        if (global_config.use_error_highlight) {
            draw_jump_highlights(app, buffer, text_layout_id, compilation_buffer, fcolor_id(defcolor_highlight_junk));
        }
        
        // NOTE(allen): Search highlight
        if (global_config.use_jump_highlight) {
            Buffer_ID jump_buffer = get_locked_jump_buffer(app);
            if (jump_buffer != compilation_buffer) {
                draw_jump_highlights(app, buffer, text_layout_id, jump_buffer, fcolor_id(defcolor_highlight_white));
            }
        }
    }
    
    // NOTE(allen): Line highlight
    if (global_config.highlight_line_at_cursor && is_active_view) {
        i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
        draw_line_highlight(app, text_layout_id, line_number, fcolor_id(defcolor_highlight_cursor_line));
    }
    
    // NOTE(allen): Color parens
    if (global_config.use_paren_helper) {
        //Color_Array colors = finalize_color_array(defcolor_text_cycle);
        //draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
        ryanb_draw_brace_highlight(app, buffer, text_layout_id, token_array, cursor_pos);
        ryanb_draw_paren_highlight(app, buffer, text_layout_id, token_array, cursor_pos);
    }
    
    // NOTE(ryanb): Hex color highlight
    ryanb_draw_hex_color_preview(app, buffer, text_layout_id, cursor_pos);
    
    // NOTE(allen): Cursor shape
    Face_Metrics metrics = get_face_metrics(app, face_id);
    f32 roundness = (metrics.normal_advance * cursor_roundness);
    
    // NOTE(allen): Cursor
    switch (fcoder_mode) {
        case FCoderMode_Original: {
            draw_original_4coder_style_cursor_mark_highlight(app, view_id, is_active_view, buffer, text_layout_id, roundness, mark_thickness);
        }
        break;
        
        case FCoderMode_NotepadLike: {
            ryanb_draw_notepad_style_cursor_highlight(app, frame_info, view_id, is_active_view, buffer, text_layout_id, roundness);
        }
        break;
    }
    
    // NOTE(allen): Put the actual text on the actual screen
    draw_text_layout_default(app, text_layout_id);
    ryanb_draw_scope_annotations(app, buffer, text_layout_id, face_id, metrics, cursor_pos, rect);
    
    draw_set_clip(app, prev_clip);
    
    // NOTE(ryanb): Function helper
    if (show_function_helper && view_id == get_active_view(app, Access_Always)) {
        ryanb_function_helper(app, view_id, buffer, text_layout_id, cursor_pos);
    }
}

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
        Query_Bar_Ptr_Array query_bars = { };
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
    Rect_f32 line_number_rect = { };
    if (global_config.show_line_number_margins) {
        Rect_f32_Pair pair = layout_line_number_margin(app, buffer, region, digit_advance);
        line_number_rect = pair.min;
        region = pair.max;
    }
    
    // NOTE(allen): begin buffer render
    Buffer_Point buffer_point = scroll.position;
    Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);
    
    // NOTE(allen): draw line numbers
    if (global_config.show_line_number_margins) {
        //draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
        ryanb_draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
    }
    
    // NOTE(allen): draw the buffer
    //default_render_buffer(app, view_id, face_id, buffer, text_layout_id, region);
    ryanb_render_buffer(app, frame_info, view_id, is_active_view, face_id, buffer, text_layout_id, region);
    
    text_layout_free(app, text_layout_id);
    draw_set_clip(app, prev_clip);
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM MAPPINGS                                                         //
/////////////////////////////////////////////////////////////////////////////

void setup_ryanb_mapping(Mapping* mapping, i64 global_id, i64 file_id, i64 code_id) {
    MappingScope();
    SelectMapping(mapping);
    
    // global bindings
    SelectMap(global_id);
    
    //BindCore(default_startup, CoreCode_Startup);
    BindCore(ryanb_startup, CoreCode_Startup);
    BindCore(default_try_exit, CoreCode_TryExit);
    
    BindMouseWheel(mouse_wheel_scroll);
    
    Bind(exit_4coder,                     KeyCode_F4,          KeyCode_Alt);                    // alt  + f4              : close 4coder
    Bind(toggle_fullscreen,               KeyCode_F11);                                         // f11                    : toggle full screen
    Bind(ryanb_command_lister,            KeyCode_F12);                                         // f12                    : open custom command lister
    Bind(ryanb_close_extra_panels,        KeyCode_Escape);                                      // esc                    : close open build panel and toggle off tooltips
    Bind(change_active_panel,             KeyCode_Comma,       KeyCode_Control);                // ctrl + ,               : switch active panel
    Bind(swap_panels,                     KeyCode_Comma,       KeyCode_Control, KeyCode_Shift); // ctrl + shift + ,       : swap panels
    Bind(ryanb_open_panel_vsplit,         KeyCode_NumPadPlus,  KeyCode_Control);                // ctrl + numpad +        : open split panel
    Bind(close_panel,                     KeyCode_NumPadMinus, KeyCode_Control);                // ctrl + numpad -        : close panel
    Bind(kill_buffer,                     KeyCode_NumPadStar,  KeyCode_Control);                // ctrl + numpad *        : close file
    Bind(ryanb_build_in_build_panel,      KeyCode_B,           KeyCode_Control);                // ctrl + b               : execute build in build panel
    Bind(interactive_new,                 KeyCode_N,           KeyCode_Control);                // ctrl + n               : open new file prompt
    Bind(interactive_open,                KeyCode_O,           KeyCode_Control);                // ctrl + o               : open existing file prompt
    Bind(ryanb_interactive_open_all_code, KeyCode_O,           KeyCode_Control, KeyCode_Shift); // ctrl + shift + o       : open existing file prompt and open all code near that file
    Bind(interactive_switch_buffer,       KeyCode_W,           KeyCode_Control);                // ctrl + w               : switch buffer prompt
    Bind(exit_4coder,                     KeyCode_Q,           KeyCode_Control);                // ctrl + q               : try to quit
    
    // plain text file bindings
    SelectMap(file_id);
    ParentMap(global_id);
    
    BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);
    
    //BindMouse(click_set_cursor_and_mark, MouseCode_Left);
    BindMouse(ryanb_click_set_cursor_and_mark, MouseCode_Left); // double-click: select token under cursor
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
    Bind(move_line_up,                    KeyCode_Up,     KeyCode_Alt);                    // alt  + up        : move line up
    Bind(move_up_to_blank_line,           KeyCode_Up,     KeyCode_Control);                // ctrl + up        : seek whitespace up and center view
    Bind(move_down,                       KeyCode_Down);                                   // down             : seek line down
    Bind(move_line_down,                  KeyCode_Down,   KeyCode_Alt);                    // alt  + down      : move line down
    Bind(move_down_to_blank_line,         KeyCode_Down,   KeyCode_Control);                // ctrl + down      : seek whitespace down and center view
    Bind(move_left,                       KeyCode_Left);                                   // left             : seek character left
    Bind(ryanb_move_left_token_boundary,  KeyCode_Left,   KeyCode_Control);                // ctrl + left      : seek token left
    Bind(move_right,                      KeyCode_Right);                                  // right            : seek character right
    Bind(ryanb_move_right_token_boundary, KeyCode_Right,  KeyCode_Control);                // ctrl + right     : seek token right
    Bind(center_view,                     KeyCode_Space,  KeyCode_Control);                // ctrl + space     : center view
    Bind(select_all,                      KeyCode_A,      KeyCode_Control);                // ctrl + a         : select all
    Bind(copy,                            KeyCode_C,      KeyCode_Control);                // ctrl + c         : copy selection
    Bind(duplicate_line,                  KeyCode_D,      KeyCode_Control);                // ctrl + d         : duplicate line
    Bind(ryanb_search,                    KeyCode_F,      KeyCode_Control);                // ctrl + f         : find in current buffer
    Bind(ryanb_list_all_locations,        KeyCode_F,      KeyCode_Control, KeyCode_Shift); // ctrl + shift + f : find in every buffer
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
    
    BindMouse(ryanb_click_goto_definition, MouseCode_Left,  KeyCode_Control); // ctrl+click  : move cursor and bookmark current position and go to function or type definition for token under cursor
    
    Bind(ryanb_toggle_function_helper,    KeyCode_F1);                                                        // F1                     : toggle the function helper tooltips on oroff
    Bind(ryanb_rename_identifier,         KeyCode_F2);                                                        // F2                     : rename identifier in all open buffers
    Bind(ryanb_move_left_token_boundary,  KeyCode_Left,         KeyCode_Control);                             // ctrl + left            : seek token left
    Bind(ryanb_move_right_token_boundary, KeyCode_Right,        KeyCode_Control);                             // ctrl + right           : seek token right
    Bind(word_complete,                   KeyCode_Tab);                                                       // tab                    : auto-complete current word
    Bind(comment_line_toggle,             KeyCode_ForwardSlash, KeyCode_Alt);                                 // alt  + /               : toggle line comment
    Bind(ryanb_goto_definition,           KeyCode_Return,       KeyCode_Control);                             // ctrl + enter           : bookmark current position and go to function or type definition for token under cursor
    Bind(ryanb_goto_bookmark_next,        KeyCode_Equal,        KeyCode_Control);                             // ctrl + =               : go to next bookmarked location
    Bind(ryanb_goto_bookmark_prev,        KeyCode_Minus,        KeyCode_Control);                             // ctrl + -               : go to last bookmarked location
    Bind(ryanb_set_bookmark,              KeyCode_B,            KeyCode_Control, KeyCode_Shift, KeyCode_Alt); // ctrl + shift + alt + b : bookmark current position
    Bind(write_note,                      KeyCode_N,            KeyCode_Alt);                                 // alt  + t               : write a NOTE comment
    Bind(write_todo,                      KeyCode_T,            KeyCode_Alt);                                 // alt  + t               : write a TODO comment
    Bind(paste_and_indent,                KeyCode_V,            KeyCode_Control);                             // ctrl + v               : paste and indent
}

/////////////////////////////////////////////////////////////////////////////
// CUSTOM LAYER INIT                                                       //
/////////////////////////////////////////////////////////////////////////////

void custom_layer_init(Application_Links* app) {
    Thread_Context* tctx = get_thread_context(app);
    
    default_framework_init(app);
    set_all_default_hooks(app);
    set_custom_hook(app, HookID_RenderCaller, ryanb_render_caller);
    mapping_init(tctx, &framework_mapping);
    setup_ryanb_mapping(&framework_mapping, mapid_global, mapid_file, mapid_code);
}