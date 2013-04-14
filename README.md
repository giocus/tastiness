TAStiNESs HTML ui for NES TAS videos
=========

Some code taken from [tom7](http://www.cs.cmu.edu/~tom7/)'s cool project: [learnfun & playfun: A general technique for automating NES games](http://www.cs.cmu.edu/~tom7/mario/)

After learning about tom7's project, it prompted me to spend some time on a personal project that I've thought of off and on for some years, around automatically finding optimal speed runs for games.

I always thought the concept was more of an "I guess it's technically possible, but not feasible" idea...  Tom7's casual mention that the NES has only 2KB of ram changes the idea to "perhaps it's slightly feasible".

As phase 1 of the project, I made this web UI to fceux

Building
-----------
I use a Mac, and haven't tried to support other platforms.

1. `brew install scons sdl gtk+`
2. `scons`

Running
-----------
1. `bin/tastiness-main`
2. open [http://localhost:9999/](http://localhost:9999/)

Using
--------
1. Use the file selector in the upper left to load a NES ROM file
2. (Optional) Use the file selector in the upper right to load a FM2 movie file
3. Use the controls below the display to move back and forth in time.
4. Hold down RLDUTSBA, and optionally Shift, and mouse over the 'player piano' input stuff at the bottom.

Sorry, I couldn't get realtime playing working...  It worked fine on the native side, but I couldn't get the browser to keep up with 60FPS coming over the websocket link.
