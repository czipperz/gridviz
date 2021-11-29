# gridviz

gridviz is a Grid Visualizer.  It is specifically useful
for debugging applications that show a 2D matrix of text.

This repository is licensed under GPL3.  If you wish to
purchase a different license, email czipperz AT gmail DOT com.

## Integration

You can integrate gridviz into your application via the
`netgridviz` library.  This library will establish a connection
to the `gridviz` server and provides a simple C interface.

`netgridviz.h` is an all in one header for `netgridviz`.  Just include it in
your project and `#define NETGRIDVIZ_DEFINE` in one file.  Alternatively, use
the `netgridviz` library that is automatically built as part of `gridviz`.

## Overview

Features:
* Draw commands are batched into strokes.
* Each stroke has a user configured title.
* Clicking on a stroke in the timeline rewinds the visual state to when the
  stroke occurred, allowing you to easily undo actions and see what went wrong.

TODO add gifs

## Building

1. Clone the repository and the submodules.

```
git clone https://github.com/czipperz/gridviz
cd gridviz
git submodule init
git submodule update
```

2. Build gridviz by running (on all platforms):

```
./build-release
```

3. After building, gridviz can be ran via `./build/release/gridviz`.
