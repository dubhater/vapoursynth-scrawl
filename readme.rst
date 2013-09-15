Description
===========
This is a simple text output plugin for VapourSynth. It doesn't use any external libraries for drawing the text. It uses a built-in bitmap font: the not-bold, 8×16 version of Terminus. The font was not modified, only converted from PCF to an array of bytes.

The font covers Windows-1252, which is a superset of ISO-8859-1 (aka latin1). Unprintable characters get turned into underscores. Long lines get wrapped in a dumb way. Lines that end up too low to fit in the frame are silently dropped.


Functions
=========
::

   scrawl.Text(clip clip, string text[, int alignment=7])

Draws arbitrary text.

::

   scrawl.ClipInfo(clip clip[, int alignment=7])

Draws information about the clip.

::

   scrawl.CoreInfo(clip clip[, int alignment=7])

Draws information about the VapourSynth core.

::

   scrawl.FrameNum(clip clip[, int alignment=7])

Draws the frame number on every frame.

::

   scrawl.FrameProps(clip clip[, int alignment=7])

Draws the properties attached to the frames.

The *alignment* parameter takes a number from 1 to 9, corresponding to the positions of the keys on a numpad.


Compilation
===========
This plugin requires some features of C++11: std::to_string() and std::next().

::

   g++ -O3 -std=c++11 -Wall -Wextra -Wno-unused-parameter -fPIC -shared -o libscrawl.so scrawl.cpp


Licenses
========
For the code, WTFPL.
The Terminus font uses the OFL.
