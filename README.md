# 4coder Customization
My 4coder customization layer for 4coder 4.1.4+ (Beta)

* 4coder_ryanb.cpp and 4coder_ryanb_build.bat go in: 4coder\custom
* theme-ryanb.4coder (optional) goes in: 4coder\themes
* config.4coder (optional) goes in: 4coder\

Many settings can be adjusted under the CONSTANTS section near the top of 4coder_ryanb.cpp

Features:
* Jump to function/struct definition with ctrl+enter or ctrl+click when cursor is on token
* Double-click to select token (double-click slowly, or triple-click normally)
* Bookmark cursor position when idle every 3 seconds, or manually with ctrl+shift+alt+b
* Advance through bookmarks with ctrl++ or ctrl+-
* Search initializes with token under cursor, supports paste from clipboard
* Search advances with enter or tab, reverse with shift+enter or shift+tab
* Search terminates on mouse-click
* List All Locations initializes with token under cursor, supports paste from clipboard
* Blinking cursor when in notepad-like mode
* Apply open theme through F12 menu
* More informative File Bar with language, line-ending, spaces/tabs (eventually encoding type)
* Change line-ending mode of file through F12 menu
* Preview hex colors when cursor is on any hex color code value
* Draw guide-lines from scope start to scope end when within scope (supports nested scopes)
* Annotate scope end with scope start when within scope (supports nested scopes)
* Highlight braces at current scope when within scope (only current scope)
* Configurable margin for line number gutter (line_number_margin value in 4coder_ryanb.cpp)
* Generate .bat file through F12 menu
* Extend token colorization to structs and functions (second color on defcolor_keyword in theme)
* Extend token colorization to C++ operators (third color on defcolor_keyword in theme)
* Extend token colorization to scope annotations (second color on defcolor_comment in theme)

Includes heavily customized versions of code from the following:
* scope lines, brace highlights, scope annotation and .bat file from https://github.com/ryanfleury/4coder_fleury
* struct and function painting from: https://github.com/Skytrias/4files
* hex color preview from: https://gist.github.com/thevaber/58bb6a1c03ebe56309545f413e898a95
