# Stand der Umsetzung

Stand: 18.10.2026 (Steuerung nach ADR 0010 mit Angriffsbewegung, bildpunktgenauer Auswahl, Tastendatei, Schnellladen, Schiffs- und Städteliste, Diplomatiefenster). Diese Seite ist die Übersicht. Die
Einzelheiten stehen in den verlinkten Dokumenten, jeweils im Abschnitt „Offen“. Sie wird bei jedem
abgeschlossenen Schritt fortgeschrieben.

Zeichen: ✅ umgesetzt · 🟡 teilweise · ⬜ offen · ❓ Verhalten des Originals nicht belegt (Klärung nötig).

## Kurzfassung

- **Die Simulation des Originals ist weitgehend vollständig**: Bauen, Wirtschaft, Häuser und Verbrauch,
  Katastrophen, Vulkane, Schiffe und Handel, Militär, Diplomatie, Piraten, Eingeborene, Computergegner,
  neues Endlosspiel, Aufträge der Szenarien mit Sieg und Niederlage, Spielstände des Originals lesen
  und exportieren, eigenes Speicherformat. Sie läuft in festen Ticks, deterministisch und befehlsbasiert,
  mit 320 Tests (davon viele gegen die Originaldaten
  und echte Spielstände).
- **Spielbar** über das Hauptmenü des Originals (Endlosspiel, Szenarien, Spielstände) oder die
  Kommandozeile, mit der Seitenleiste des Originals (Bauen, Kampf, Status, Einstellungen), Steuerung nach
  ADR 0010; Rückmeldungen auch im Terminal.
- **Die großen offenen Blöcke** sind die Oberfläche (Menüs, die Fenster der Auswahl, Optionen),
  Kampagnenfortschritt und Bestenliste, der Mehrspieler und die
  Modding-Grundlagen (Inhaltspakete, Fraktionen als Daten, Lua).
- **Spielstände** speichert Open1602 seit dem 18.10.2026 im eigenen Format `.o1602`
  ([o1602-save.md](formats/o1602-save.md), [ADR 0009](architecture/0009-own-save-format.md)): ohne
  die Grenzen des Originals (7 Spieler, 150 Schiffe, 400 Soldaten, 350 Türme), mit Prüfsumme, gepackt,
  etwa ein Viertel so groß. Als `.GAM` für das Original exportieren geht weiter, solange das Spiel passt.

## 1. Originalspiel

### 1.1 Simulation

| Bereich | Stand | Offen | Dokument |
|---|---|---|---|
| Bauen und Abreißen | ✅ Prüfungen, Kosten, Ziehen von Straßen und Mauern, Strand, Brücken, Stege, Freischaltung, Gebiet, Stadtgründung vom Schiff, Übernahme wehrloser Städte; Baumenü mit Ausrichtung zur Straße | – | [construction.md](re/construction.md) |
| Produktion | ✅ Zyklus, Rohstoffe, Lager, Leerlauf, Statistik, Ein/Aus | – | [production.md](re/production.md) |
| Logistik | ✅ Träger, Marktkarren, Bauern, Herden, Fischerboote, Minen und Lagerstätten, Packesel, Steinmetz am Steinbruch, `Grundflg`-Gebäude in der Wegsuche, Handelseinstellungen nach `Maxware` des Kontors (`0x00481EE0`) | – (dringende Waren bevorzugt auch das Original nicht, S-104) | [logistics.md](re/logistics.md) |
| Wachstum, Dürre, Jagd | ✅ Klassenuhren, Ernte, Dürre, Hirsche und Jäger (Feld für Feld gegen Merlin02 → Merlin03) | – | [growth.md](re/growth.md), [deer.md](re/deer.md) |
| Häuser, Verbrauch, Steuern | ✅ Hausliste, Versorgung, Auf- und Abstieg, Ruinen, Verbrauch, Zufriedenheit, Stolz, Steuern, Betriebskosten, Sold; Stadtfenster als Befehl (Steuer je Klasse, Name, Baumaterial zurückhalten) | – | [houses.md](re/houses.md), [consumption.md](re/consumption.md) |
| Brand, Pest, Räuber | ✅ mit Arzt, Feuerwehr, Galgen | – | [disasters.md](re/disasters.md) |
| Vulkane | ✅ Krater, Wurf, Lava, Brand, Zerstörung, Meldung nur für Entdecker | Vulkanereignisse der Szenarien | [volcanoes.md](re/volcanoes.md) |
| Schiffe und Handel | ✅ Fahren und Wegsuche (Wasser bei feindlichen Türmen teurer), Laden, Handel am Kontor, Routen, freier Händler, Werften mit Bau und Reparatur, Erkunden mit Erz- und Goldsymbolen (gegen Video geprüft); Schiffsfenster als Befehl (Kanonen, über Bord, Name, Verkauf und Kauf zwischen Spielern) | – | [ships.md](re/ships.md) |
| Militär | ✅ Einheiten, Kanonentürme, Burgen und Ausbildung, Bewegung, Land- und Seekampf, Einschiffen, Eroberung, Fluch der Eingeborenen | – | [military.md](re/military.md) |
| Diplomatie | ✅ Verträge, Ereignisse, Stimmung, Tribut, Gesten, Entscheidungen der Computer | Szenario-Haltung (`PLAYER4` +0x1C/+0x20) | [diplomacy.md](re/diplomacy.md) |
| Piraten, Eingeborene | ✅ Piratenschiffe, Beute, Plünderung, Nest mit Gründung und Ausbau, Frieden kaufen, Bestechung; Eingeborene mit Wohlwollen, Verbrauch, Kriegern; Niederlage der Piraten, wenn ein Auftrag sie verlangt | – | [pirates.md](re/pirates.md) |
| Computergegner | ✅ Aufgabenstapel, Stufen und Bedarfe, Bauplatzsuche, Wartung, Siedeln, Kolonien, Versorgung, Flotte, Handel, Militär, Diplomatie, Aufträge der Schiffe aus Originalständen, Reaktion auf eine Gründung (Ereignis 0x0E) | KI-Schalter der Szenarien (Spieler +0x70) | [ai.md](re/ai.md) |
| Neues Endlosspiel | ✅ wie das Original, Byte für Byte gegen zwei Endlosstarts geprüft | Mehrspieler-Endlosspiel (`.szm`) | [newgame.md](re/newgame.md), [worldgen.md](re/worldgen.md) |
| Szenarien und Aufträge | ✅ Szenarien laden und laufen; Aufträge (`AUFTRAG4`, alle 18 Ziele, Verlustbedingungen), Sieg, Niederlage (ohne Stadt und Schiff, Bankrott, Computer, Piraten), Triumphbogen für den Schuldigen, Handelsstatistik, abwesende Plätze; Szenarien haben keine Ereignisse (belegt) | Kampagnenfortschritt (Original: `Game.dat`), Bestenliste und Punkte, Endbildschirm; KI-Schalter der Szenarien | [missions.md](re/missions.md) |
| Spielstände | ✅ Originalstände lesen; eigenes Format `.o1602` ohne Grenzen, kanonisch, mit Prüfsumme; Export als `.GAM` (unverändert Byte für Byte zurück, vom Original geladen und weitergespielt) | Export eines aus `.o1602` geladenen Spiels im Original ungeprüft | [o1602-save.md](formats/o1602-save.md), [savegame-writer.md](re/savegame-writer.md) |

### 1.2 Darstellung, Oberfläche, Ton

| Bereich | Stand | Offen | Dokument |
|---|---|---|---|
| Inseln, Gebäude, Weltkarte | ✅ drei Zoomstufen, vier Drehungen, Flaggen, Rauch, Statussymbole, Besitzansicht (Bildvergleich 0,967) | Helligkeits- und Farbstich-Tabellen; Werftsymbole „Material fehlt“ und „Brand“ | [rendering.md](re/rendering.md) |
| Figuren | ✅ alle Figuren der Wirtschaft, Soldaten in jeder Bannerfarbe, Schiffe mit Schaden, Bugwelle, weißer Flagge, Sinken, Zierfiguren (Spaziergänger, Gaukler, Wale, Delfine, Kraken) | Mündungs- und Schussrauch, Treffer, Wrack (SHIPSUNK), Lebensbalken; Zierfiguren auch auf unentdeckten Inseln | [figures.md](re/figures.md), [flavor.md](re/flavor.md) |
| Oberfläche | 🟡 Grundlagen: alle 45 Layouts (`.GAD`) gelesen, Schriften (`.ZEI`) mit Farben und Umbruch, Bereiche des Bildschirms, Gadgets im Betrieb (Zustände, Klick, Umschalter, Gruppen, Schieber, Zeichenliste); im Betrachter der Rahmen des Originals: Kopf der Seitenleiste mit Weltkarte, Kompass, Drehen und Zoom, Statusleiste mit Hilfetexten und Meldungssymbolen; Modi Bauen und Info: **Baumenü** (Gruppen als Daten, Aufklappfeld, Verfügbarkeit, Kosten, Vorschau, Produktionskette, Abreißen, Drehen) und Spielerstatus; Geld und Waren oben auf der Karte; **Fenster der Auswahl**: Betriebe, öffentliche Gebäude, Wohnhaus, Kontor/Markt mit Listen und Lager, fremder Kontor, Werft mit Typauswahl, Burg, Schiff mit Umladen, Handeln, Kontorgründung, Handelsroute und Piraten; **Steuerung nach ADR 0010**: frei belegbare Tasten (`keys.cfg`, Übersicht mit F1), bildpunktgenaue Auswahl mit Rahmen, Gruppen mit Ziffer, Formationen, Filtern und Tab, Rechtsklick- und Minikartenbefehle, Befehlsketten, Angriffsbewegung, Stellung halten, Rückfragen, Schnellspeichern/-laden; **Kampfmodus** mit den vier Fenstern; **Schiffs- und Städteliste**, **Diplomatie** mit Tribut; **Einstellungen** mit Spielständen, Musik & Effekten, Sprachmeldungen & Filmen, Auftragsfenster; **Hauptmenü** (Einzelspieler: Szenarienliste, Auftrag mit Stufenwahl und Bestenliste, Name und Flagge, Spielstände, Fortsetzen, Credits, Intro) | Mehrspieler-Menüs, eigenes Spielmenü, Endbildschirm mit Bewertung, eigene Bestenlisten, Freischaltung der Missionen, Mauszeiger | [ui.md](re/ui.md) |
| Ton, Musik, Sprache | ✅ Geräusche (Bauen, Schüsse, Treffer, Befehle, Vögel, Brandung, Arbeit und Tiere, Klicks) räumlich wie im Original, Sprachmeldungen mit Warteschlange, Musik mit Zufall, Kampfmusik und Ausblenden, 4-Bit-Musik entpackt; Lautstärken aus `Game.dat` (nur gelesen) | Tutorial-Sprache, Einwohner- und Warenmeldungen, Optionsfenster | [audio.md](re/audio.md) |
| Videos | ✅ eigener Smacker-Decoder, Meldungs-, Sieg-, Niederlage- und Pausenvideos, Startfolge (`--intro`), Ton über den Mischer | Hauptmenü (Intro-Knopf, Abspann im Bilderrahmen), Video 2990 (Kampagnenfortschritt), Optionsfenster; im Mehrspieler nicht anhalten (M-018) | [videos.md](re/videos.md), [smk.md](formats/smk.md) |
| Mehrspieler | 🟡 Grundlage: Befehle, feste Ticks, Zufallsströme, KI auf jedem Rechner, Einstellungen im Zustand | Netz (Lockstep), Zustands-Hash je Tick, Lobby, Spielerzuordnung, Wiederverbinden (M-003) | [ADR 0004](architecture/0004-deterministic-simulation.md) |
| Editor | ⬜ | `1602Edit.exe` nachbauen (später) | – |

### 1.3 Klärung nötig (Verhalten des Originals nicht belegt)

Eine Beobachtung im Original würde helfen, wo „Beobachtung“ steht; was ich dafür brauche, steht in
[requests.md](requests.md).

| Punkt | Wo | Weg zur Klärung |
|---|---|---|
| Tempo und Stärke der Computergegner im Vergleich | [ai.md](re/ai.md) | Beobachtung: Endlos-Stände nach 1 und 3 Stunden |
| Fahrverhalten der Computerschiffe | [ships.md](re/ships.md) | Beobachtung (Video) |
| Ansichtsdrehung 1–3 gegen das laufende Original | [rendering.md](re/rendering.md) | Beobachtung: Bildschirmfoto in gedrehter Ansicht |
| Dürre: Stärke, Ende, verdorrte Felder | [growth.md](re/growth.md) | Beobachtung: Stände „Duerre“, „NachDuerre“ |
| Vulkanausbruch im laufenden Spiel | [volcanoes.md](re/volcanoes.md) | Beobachtung: Stände vor und nach dem Ausbruch |
| Brand, Pest, Räuber in großen Städten | [disasters.md](re/disasters.md) | Beobachtung: größere Stadt, Video |
| Wirkung eines Brands auf den Schiffbau einer Werft | [disasters.md](re/disasters.md) | Beobachtung |
| Ob Einheiten in Ausbildung ausgesandt werden können | [military.md](re/military.md) | Beobachtung |
| Wer die Meldung 0x48 („protestiert“) schickt | [diplomacy.md](re/diplomacy.md) | Analyse |
| TIMERS +0xA5 („Computer untätig“): wer es setzt | [ai.md](re/ai.md) | Analyse |
| Stadt-Felder +2/+4/+8, HAUS +0x6A Bit 7 (vermutlich `Destroyflg`) | [logistics.md](re/logistics.md) | Analyse |
| Produktions-Bits +0x0F/4 und /6 | [production.md](re/production.md) | Analyse |
| Zweite Liste bei Insel +0x30, Zufallsklasse geladener Felder | [growth.md](re/growth.md) | Analyse |
| Leitschalter des Laders (`0x0060912C`, `0x005E6AA8`) | [deer.md](re/deer.md) | Analyse |
| Sonderfiguren der Zweige 9/10/11/13 | [rendering.md](re/rendering.md) | Analyse |
| Inselkopf (Rest), Attributbits 26–31, BSH-Feld `type`, Palettenanimation | [scp.md](formats/scp.md), [bsh.md](formats/bsh.md), [col.md](formats/col.md) | Analyse |
| Teile von `TIMERS`, übrige `SZENE`-Chunks; `AUFTRAG4` +0x48/+0x58 (nie gelesen); TIMERS +0xA5 (wohl das Tutorial) | [gam.md](formats/gam.md), [missions.md](re/missions.md) | Analyse |
| U-007, S-088 (Teil), S-094 (Teil) | [bugs.md](re/bugs.md) | aus dem Code gefolgert; Open1602 vermeidet sie ohnehin |
| Wiedergabe der Meldungen, Zeichenzuordnung der Schriften | [cod.md](formats/cod.md) | mit der Oberfläche |

## 2. Open1602-Erweiterungen

### 2.1 Umgesetzt

| Was | Wo |
|---|---|
| **Fehler des Originals**: 173 Einträge im Katalog, davon 96 behoben, 43 vermieden, 6 als Regel umstellbar, 18 beibehalten | [bugs.md](re/bugs.md) |
| **Einstellungen** (`sim::Rules`, im Spielstand): Rückerstattung beim Abriss, Schiffe je Spieler (100, Original 33), Einheiten je Spieler (250, Original 100), Rohstoffgarantie, höchstes Spieltempo (8, Original 4), Dürren, Vulkane, Händlerpreise | [ADR 0008](architecture/0008-game-settings.md) |
| **Eigene Welten** (Weltengenerator Stufe 1): 200–2000 Felder je Seite, Inselzahl, Größenmischung, Dörfer, Vulkaninseln, Startgebiete; Chunk `O1602MAP` | [worldgen.md](re/worldgen.md) |
| **Eigenes Speicherformat `.o1602`**: voller Zustand (Karte, Uhren, Zufall, Figuren, Brände, Lava, Diplomatie, Erkunden, Schiffe, Soldaten, KI) ohne Grenzen, kanonisch, CRC-32, zlib; ein gespeichertes Spiel läuft genau weiter. Exportierte `.GAM` tragen denselben Zustand im Chunk `O1602` | [o1602-save.md](formats/o1602-save.md), [o1602-chunk.md](formats/o1602-chunk.md) |
| **Aufträge auf jedem Rechner gleich geprüft** (S-105), nur mit dem eigenen Auftrag (S-106), Bankrott nur nach 200 s am Stück (S-107), beste Zuordnung der Stadtziele (S-108), Inselziele mit beim Start festgehaltenen Inseln (S-110), geprüfte Auftragsdaten (S-111); nach dem Sieg kann weitergespielt werden | [missions.md](re/missions.md) |
| **Determinismus**: feste Ticks, Zufallsströme je Teilsystem, KI und Zierfiguren auf jedem Rechner gleich | [ADR 0004](architecture/0004-deterministic-simulation.md) |
| **Banner**: beliebig viele Spieler im Modell, eingefärbte Einheitsflagge, Einheiten in jeder Farbe | [ADR 0007](architecture/0007-extensibility-and-modding.md), [flags.md](modding/flags.md) |
| **Keine technischen Grenzen** der Originaltabellen in der Simulation (Pools mit stabilen IDs), keine Grenze von 250 Anzeigefiguren | [bugs.md](re/bugs.md) (L-002, L-007) |
| **Computergegner**: Tempo unabhängig von der Bildrate, breitere Steinbruchsuche, Felder nachpflanzen statt abreißen, Mitschrift mit `O1602_AI_LOG` | [ai.md](re/ai.md) |
| **Korrekturen nach Rückmeldung (18.10.2026)**: Namenseingabe im Menü (Texteingabe lief nur im Spiel), Name vorbelegt mit „Namenlos“ oder dem letzten, erste Taste ersetzt ihn, Enter spricht „Wählt Euer Banner“ (Speech8 107–115), Escape zurück zur Auftragswahl; Rückfragen und Tastenübersicht antworten nur auf Klicks (Drücken und Loslassen), nicht auf Überfahren; Meerestiere seltener (`sea_creatures_every`, Standard 60 s); Sichtrahmen der Minikarte am Kartenrand behält seine Form; Schatten der Bauvorschau zeigt das gewählte Gebäude (vorher die Holzmauer); Fließtext bricht in Schriftpixeln um (Auftragsfenster, Briefing im Menü); Statusleiste und Auftragsfenster in voller Breite ohne Nähte; Ereignissymbol in der Farbe des Konkurrenten (neue Stadt, neues Schiff); Rechtsklick schließt erst das Aufklappfeld des Baumenüs, dann das Gebäude in der Hand | [flavor.md](re/flavor.md), [bugs.md](re/bugs.md#fehler-von-open1602-selbst) |
| **Korrekturen nach Rückmeldung, zweite Liste (18.10.2026)**: Fruchtbarkeit einer erkundeten Insel in der Statusleiste über Gras und Bäumen ([GAME] 22); Baumodus dunkelt ab, was nicht bebaubar ist, beim Setzen und Anklicken eines Gebäudes alles außerhalb seines Einzugskreises; Wald in zufälligen Varianten; leere Warenfelder ohne gelben Rahmen; Kamera bleibt über der Karte; weichere Schatten im Baumenü; Rechtsklick zurück zum Spielerstatus (ein Klick aus jedem Menü, Baumenü samt Gebäude in der Hand); eine angeschaute Farm wählt ihr Feld im Baumenü; beim Ziehen von Feldern und Wald ist die Fläche gelb schraffiert wie in der History Edition; Spielstände unbegrenzt mit Pfeilen, Enter speichert einen neuen sofort, Stimme bestätigt das Speichern | [ui.md](re/ui.md), [bugs.md](re/bugs.md#fehler-von-open1602-selbst) |
| **Darstellung und Bedienung**: Figuren auch in der kleinsten Zoomstufe, Zoom mit dem Mausrad, Pixelskalierung, Bildschirmfoto als PNG, eigener Speicherordner (nie in die Installation) | [setup-arch.md](setup-arch.md) |

### 2.2 Offen oder geplant

| Was | Abhängigkeit | Quelle |
|---|---|---|
| Startdialog mit allen Einstellungen in Gruppen | Oberfläche | [ADR 0008](architecture/0008-game-settings.md) |
| Flaggenmenü mit freier RGB-Farbe wie in Anno 1503 | Oberfläche | [ADR 0007](architecture/0007-extensibility-and-modding.md) |
| Mehr als vier Spieler beim Spielstart (Plan kann 16), Piraten und Händler skalieren mit | Mehrspieler, Fraktionen als Daten | [worldgen.md](re/worldgen.md) |
| Fraktionen als Daten (heute festes `PlayerRole`) | – | ADR 0007 |
| Inhaltspakete mit Manifest und Kennungen, freie Warenliste, PNG- und OGG-Import | M8 | ADR 0007 |
| Lua-Skripte, eigene Szenarien und Kampagnen | M8, Szenarien | ADR 0007 |
| Weltengenerator Stufe 2 (eigene Inselformen, auch größer als die Vorlagen) | zurückgestellt bis alles läuft | [00-plan.md](00-plan.md) |
| Daten-Patches für `.cod`-Fehler (D-001, D-002 als Kandidaten), abschaltbar | Mechanismus fehlt | [bugs.md](re/bugs.md), ADR 0006 |
| Beliebige Auflösung, freier Zoom, UI-Skalierung, frei belegbare Tasten | M7, Oberfläche | [00-plan.md](00-plan.md) |
| Mehrspieler mit Lobby, Desync-Erkennung, Wiederverbinden, Wiederholungen | M6 | ADR 0004 |
| Ideen: volle Rückerstattung kurz nach dem Bau; eigene Straßen-Wegsuche neben der des Originals; Piratenabstand des Schwierigkeitsgrads als Einstellung (S-068) | offen, Entscheidung nötig | [00-plan.md](00-plan.md) |

### 2.3 Entscheidungen, die noch anstehen

- ~~Speichern über die Originalgrenzen~~: entschieden (18.10.2026), eigenes Format `.o1602` (ADR 0009).
- ~~U-013~~ (Erkunden an Werft und Piratenkontor): entschieden (18.10.2026), die Oberfläche bietet es an (ADR 0010).
- ~~Tastenbelegung~~ (ADR 0010): bestätigt (18.10.2026).
- **S-035** (Markt-Abriss zerstört Häuser): beibehalten, Warnung mit der Oberfläche.
- ~~Dringende Waren und Turm-Kostenklasse~~: erledigt (18.10.2026).

## 3. Technik und Qualität

| Punkt | Stand |
|---|---|
| Tests | 313 Fälle (30 Unit-Dateien, 8 mit Originaldaten). Ohne eigene Unit-Tests: KI (nur zwei Datentests), See- und Landwege (nur indirekt), Weltengenerator (über das neue Spiel), eigener Chunk (über die Datentests), Betrachter |
| Builds | lokal unter Linux: debug, game, game-rel, asan, game-asan ohne Warnungen; CI für Linux (GCC, Clang, ASan), Windows (MSVC 2022), macOS 14 und das Spiel mit SDL3 auf allen drei. Windows und macOS sind lokal nie gelaufen |
| Spielerbefehle | vollständig für die Simulation (seit 18.10.2026 auch Stadtfenster und Schiffsfenster); die Fenster der Auswahl lösen sie aus; weiße Flagge, Versenken, Patrouille, Befehlsketten und Gruppenbefehle mit Versatz sind dazugekommen |
| Symboltabelle `re/symbols/` | 532 Funktionen (18.10.2026 um die Adressen aus Militär, Piraten, Diplomatie, KI, Aufträgen, Katastrophen und Neuem Spiel ergänzt; Beleg verweist auf das Dokument) |
| Lokaler Spieler | der erste Mensch, bis es eine Lobby gibt |
