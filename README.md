# pebble_reversi
An implementation of the classic game Reversi for the Pebble smart watch featuring a simple minimax AI, written in C with the Pebble SDK.

Updated for support for both the Pebble (Aplite) and Pebble Time (Basalt) platforms.

## Features:

* The game Reversi, implemented for the controls and display of a Pebble watch, including simple frame animations for flipping the pieces.
* A minimax AI with adjustable search depth (for difficulty)
* Options for zero, one, or two human players.
* Serialized game state for automatic saving and resuming on exit.

## Notes:

* Alpha-Beta pruning is implemented, but turned off, as it was causing odd behaviors and was likely buggy.  Feel free to try it out.
* Also deactivated: the out of memory protections for the AI.  In practice, processor performance was the actual limiting factor, not memory.

## Suggested usage:

It's a little messy.  But the contained assets, AI, and framework could be a suitable starting place for a number of simple two-player board games, such as Checkers, Attaxx, Go, or Connect Four. Have at it!  Let me know if you make anything.

## Assets:

Including the piece flipping animation frames and the app icon. (For the sake of completion: assets included are Creative Commons Attribution-ShareAlike 4.0 International)

## Compiling:

The appinfo.json has been gitignored, because it contains UUIDs that may make it possible for users to overwrite the Pebble Reversi available on the Pebble App Store.  In order to compile, create a new appinfo.json (for instance, by creating a new project using "pebble new-project new_project_name"), then copy the UUID into the appinfo.json.template of this project, rename appinfo.json.template to appinfo.json, and run "pebble build".  You may also want to swap out the names and company name.