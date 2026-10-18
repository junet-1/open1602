# Spielerflaggen und Farben

Alle Spielerflaggen sind **eine** Flagge, die die Engine in der Farbe des Spielers einfärbt. Eine neue
Farbe braucht deshalb keine Grafik: Im Flaggenmenü wählt man eine der zwölf vorgegebenen Farben
(`render::BannerTable::kDefaultColors`) oder eine beliebige RGB-Farbe. So wächst die Zahl der möglichen
Spieler ohne neue Bilder ([ADR 0007](../architecture/0007-erweiterbarkeit-und-modding.md), Abschnitt 4).

**Stand:** Umgesetzt sind das Einfärben (`render/flag_tint.hpp`), die zwölf Vorgaben und die Vorlage aus
der weißen Originalflagge (`render::BannerTable`), eigene Bilder je Banner (`FlagStyle::Sprites`, so für
den Totenkopf) und Einheiten in Spielerfarbe (`render/unit_tint.hpp`). Noch offen: das Flaggenmenü und
die freie RGB-Farbe (Oberfläche; bis dahin `--colour 0–3` beim neuen Spiel im Betrachter) sowie eigene Flaggen als PNG
(Inhaltspakete, Meilenstein M8).

## Wie gefärbt wird

Jedes Pixel der Flaggenvorlage mit **R = G = B** (reines Grau) gehört zum Tuch und nimmt die
Spielerfarbe an. Sein Grauwert ist die Schattierung:

| Grauwert | Ergebnis |
|---|---|
| 128 | genau die Spielerfarbe |
| 0 … 127 | dunkler, linear bis Schwarz: `Farbe · Grau / 128` |
| 129 … 255 | heller, linear bis Weiß: `Farbe + (255 − Farbe) · (Grau − 128) / 127` |

Alle anderen Pixel – farbig oder durchsichtig – bleiben unverändert. Damit lassen sich Mast, Rand,
Wappen oder Schrift in festen Farben zeichnen.

## Eine eigene Flagge zeichnen

- **Bilder:** 8 Phasen des Wehens, je Zoomstufe ein Satz (volle Größe wie `gfx/ship.bsh` 216–223,
  89 × 30 Pixel; halbe und Viertelgröße für die anderen Stufen). Die Phasen laufen mit 90 ms je Bild.
- **Ankerpunkt:** Die Engine zentriert das Bild waagrecht über dem Mast. Auf Gebäuden liegt die
  Unterkante des Bildes in der Höhe `Fahnoffs[2]` der Flaggenfigur, auf Schiffen hängt die Oberkante
  von der Mastspitze herab ([figures.md](../re/figures.md), [rendering.md](../re/rendering.md) §6).
  Das Tuch beginnt in den Originalbildern knapp rechts der Mitte (Spalte 46 von 89, Zeile 6); eine
  eigene Flagge sollte denselben Ansatzpunkt haben, sonst schwebt sie neben dem Mast.
- **Tuch in Grautönen:** Grundton 128, Schatten darunter, Glanzlichter darüber. Mit einem
  Grauverlauf statt einer festen Farbe zeichnen, dann sieht die Flagge in jeder Spielerfarbe richtig aus.
- **Feste Teile in Farbe:** Ein Wappen, das nicht eingefärbt werden soll, darf kein reines Grau
  enthalten. Für graue feste Teile (z. B. einen silbernen Rand) genügt eine minimale Abweichung, etwa
  (120, 120, 121).
- **Format:** PNG mit Alphakanal; Pixel mit Alpha 0 sind durchsichtig. Halbtransparenz wird nicht
  unterstützt, damit die Flagge zum Pixelstil des Spiels passt.

Ein Mod kann außerdem für einzelne Banner eigene, fertig gefärbte Bilder liefern (`FlagStyle::Sprites`),
wenn ein Motiv nicht einfärbbar sein soll – so behalten die Piraten ihren Totenkopf.

Dieselben Regeln sollen für eigene Einheiten in Spielerfarbe gelten (Soldaten, Reiter, Kanoniere): Tuch
in reinem Grau, alles andere mit mindestens einem Wert Abweichung. Für die Originaleinheiten erzeugt
Open1602 die Vorlage schon jetzt selbst ([figures.md](../re/figures.md#soldaten-und-spielerfarben)).

## Die Originalflaggen

Das Original hat für Rot, Blau, Gelb und Weiß je eigene, von Hand gerasterte Bilder (`ship.bsh`
192–223), dazu den Totenkopf der Piraten (224–231). Open1602 macht aus der weißen Flagge die Vorlage:
Jedes Pixel bekommt den Grauwert 0,48 · Helligkeit + 5. Die Faktoren und die Grundfarben der vier
Banner (Rot 216/51/8, Blau 0/0/241, Gelb 255/219/57, Weiß 255/255/255) sind an die Originalbilder
angepasst; die mittlere Abweichung liegt bei etwa 15 von 255 je Farbkanal, weil die Originale mit
256 Farben gerastert sind. Wer die Originalbilder möchte, schaltet sie mit `--original-flags` ein
(`render::BannerOptions::original_player_flags`).

## Farben wählen

Die zwölf Vorgaben sind auf Land, Wasser und der Minikarte gut zu unterscheiden: Rot, Blau, Gelb,
Weiß (wie im Original), Grün, Orange, Lila, Türkis, Rosa, Hellblau, Braun, Hellgrün. Schwarz fehlt,
weil die Piraten schwarz fliegen und sehr dunkle Flaggen auf den Dächern kaum zu sehen sind. Das
Menü soll später bei freien RGB-Farben vor zu ähnlichen oder zu dunklen Farben warnen.
