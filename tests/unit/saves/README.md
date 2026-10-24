# Stored savegames of the save tests

Small synthetic games, without any original data (a building table of three records written in
the test, two islands of grass, four players, a few ships and soldiers), saved by Open1602 as
it was on 2026-10-24 with the state of the running game (section SIMU) in each of its earlier
versions 1 to 6. `test_save_game.cpp` ("games of every earlier state version load") loads each
one, runs it 200 ticks and compares the world with the hash those versions gave.

They stand for files players keep: never regenerate them with the current code. When a format
changes, add the new version's file next to them instead.
