// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where the program's messages go. Linux and macOS: stderr, as always. Windows starts open1602.exe
// as a GUI program without a console, so stderr leads nowhere: a console it was started from gets
// the messages, else the file open1602.log in the user's data folder; fatal errors also show in a
// message box, as a double click otherwise ends without a word.
#pragma once

#include <string_view>

namespace o1602::app {

/// Call first in main(), before anything is written to stderr.
void setup_diagnostics();

/// An error that ends the program: on stderr, and on Windows in a message box (UTF-8 text).
void report_fatal(std::string_view message);

}  // namespace o1602::app
