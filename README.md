# 4coder Customization
My 4coder customization layer for 4coder 4.1.2+ (Beta)

* 4coder_ryanb.cpp and 4coder_ryanb_build.bat go in: 4coder\custom
* theme-ryanb.4coder goes in: 4coder\themes

Features:
* Jump to function under cursor, with the ability to jump back afterwards
* Blinking cursor when in notepad-like mode
* More informative File Bar
* Preview hex color codes that lie under the cursor
* Draw guide-lines from scope start to scope end
* Annotate scope end with scope start
* Highlight braces at current scope
* Configurable margin for line number gutter
* Extend token colorization to structs and functions (second color on defcolor_keyword)
* Extend token colorization to C++ operators (third color on defcolor_keyword)
* Extend token colorization to scope annotations (second color on defcolor_comment)

Includes heavily customized versions of code from the following:
* scope lines, brace highlights, scope annotation and .bat file from https://github.com/ryanfleury/4coder_fleury
* struct and function painting from: https://github.com/Skytrias/4files
* hex color preview from: https://gist.github.com/thevaber/58bb6a1c03ebe56309545f413e898a95
