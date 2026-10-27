# engine/script

Scripts of content packages (ADR 0007 section 6, [docs/modding/scripts.md](../../docs/modding/scripts.md)).

- `sandbox.hpp`: `script::Sandbox`, a Lua 5.4 state (`third_party/lua`) that keeps scripts
  deterministic and closed off: no files, no clock, no addresses, random numbers only from the
  game, limits on memory and instructions.

Still open (milestone M8): loading the scripts of a package, events, commands, the script state in
the savegame and messages posted by scripts.
