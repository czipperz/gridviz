# gridviz

gridviz is a Grid Visualizer.  It is specifically useful
for debugging applications that show a 2D matrix of text.

You can integrate gridviz into your application via:
* Linking the `netgridviz` library.  This library will establish a connection to
  the `gridviz` server and provides a simple C interface as well as C++ wrappers.

This repository is licensed under GPL3.  If you wish to
purchase a different license, email czipperz AT gmail DOT com.

Features:
* TODO writeme

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
