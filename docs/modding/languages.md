# Languages

Open1602 shows the game in the language of the installed *Anno 1602*: a German installation gives a
German game, an English one an English game. Texts that come from the original's data (Text.cod,
the scenarios, the speech and videos) are shown as they are. Open1602's **own** texts (menus,
questions, the key overview and other messages the original does not have) come from catalogues,
one per language.

## Which language is chosen

1. `--language CODE` on the command line, e.g. `--language en`.
2. `language = CODE` in `settings.cfg` in the user's data folder (`auto` or missing: next step).
3. The installation's language: the catalogue whose `language.detect` text equals a line of the
   section `[PLAYCOL]` in Text.cod ("Roter Spieler" for German).
4. GOG's installer file `goggame-*.info` ("languages": ["de-DE"] gives `de`).
5. The History Edition's language folder in use (`Loca/de-DE` gives `de`, see below).
6. English.

## The History Edition

Ubisoft's *Anno 1602 History Edition* keeps the files of each installed language in
`Loca/<language>/` (`Loca/de-DE/`: Text.cod, Editor.cod, the speech, the samples of the soldiers and
ships, the menu art TOOLS.BSH and START.BSH, the videos) and the English ones in the installation's
own folders. Open1602 reads a file from that language folder when it has it and from the
installation otherwise, just as the History Edition does. With several language folders it takes
the one of `--language` (or `language` in `settings.cfg`), else the one of the system's language,
else English.

A key the chosen language lacks is taken from English; a key no catalogue has is shown as the key
itself, so a missing line is easy to spot.

## Catalogues

The built-in catalogues are [data/locale/en.lang](../../data/locale/en.lang) and
[data/locale/de.lang](../../data/locale/de.lang). They are compiled into the program, so the game
needs no extra files. English is complete: every key has a line there, and a test checks that no
other catalogue has a key English lacks.

A catalogue is UTF-8 text with one `key = value` per line:

```ini
# Comments start with #.
language.name = Deutsch
language.detect = Roter Spieler
dialog.quit_anno = Anno 1602 beenden?
dialog.break_peace = Friedensvertrag mit {1} brechen?\nDas bedeutet Krieg.
```

- `\n` is a line break; questions show up to two lines.
- A no-break space (U+00A0) joins a number and its unit: a wrapped text does not break there
  („33 %“ stays on one line); it is drawn as a space.
- `{1}`, `{2}`, ... are filled in by the game (a name, a number); the translation decides where they
  stand. What each key gets is noted in `en.lang`.
- The game's fonts know the letters of Windows-1252 (Western European languages); other letters are
  left out.
- `language.detect` holds one or more lines of `[PLAYCOL]` from that language's Text.cod, separated
  by `|`.

## A new language or own texts

Put a file `<code>.lang` into the folder `locale` of the user's data folder
(Linux `~/.local/share/Open1602/locale/`, macOS `~/Library/Application Support/Open1602/locale/`,
Windows `%APPDATA%\Open1602\locale\`). Its lines extend or replace the built-in catalogue of the
same code, so a new language works without rebuilding (choose it with `--language` or
`settings.cfg` until it is built in). To build it in, add it to `data/locale/` and name a
`language.detect` line.

## In the code

`ui::tr("key")` gives the text of the current language in Windows-1252, ready for the game's fonts;
`ui::tr("key", {name, std::to_string(n)})` fills `{1}`, `{2}`. New texts get a key in `en.lang`,
`de.lang` and every other built-in catalogue (see [o1602/ui/localization.hpp](../../engine/ui/include/o1602/ui/localization.hpp)).
