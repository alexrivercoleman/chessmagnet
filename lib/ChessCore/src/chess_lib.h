// The one place that includes Disservin's chess-library.
// CHESS_NO_EXCEPTIONS must be defined before the include (we build without
// relying on C++ exceptions, which are costly on a microcontroller).
#pragma once
#ifndef CHESS_NO_EXCEPTIONS
#define CHESS_NO_EXCEPTIONS
#endif
// Put the 840 KB of sliding-piece attack tables on the heap (= PSRAM on the
// ESP32) instead of in static RAM. See chess/VERSION.txt.
#ifndef CHESS_SLIDER_TABLES_ON_HEAP
#define CHESS_SLIDER_TABLES_ON_HEAP
#endif

// Arduino.h defines a function-like macro `sq(x)` (square a number) that
// collides with identifiers inside chess.hpp. Hide it during the include.
#pragma push_macro("sq")
#undef sq
// Silence one harmless warning inside the vendored library (only there).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include "chess/chess.hpp"
#pragma GCC diagnostic pop
#pragma pop_macro("sq")
