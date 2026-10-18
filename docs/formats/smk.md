# SMK – Smacker-Videos

Die Videos des Spiels sind Smacker-Dateien (RAD Game Tools); das Original spielt sie mit
`smackw32.dll`. Open1602 hat einen eigenen Decoder, geschrieben nach der öffentlichen
Formatbeschreibung ([MultimediaWiki: Smacker](https://wiki.multimedia.cx/index.php/Smacker)), ohne
fremden Code und ohne Bibliothek: `video::Smacker` (`engine/video/src/smacker.cpp`). Was das Spiel wann
zeigt, steht in [videos.md](../re/videos.md).

## Die Dateien des Spiels

| Datei | Bild | Bilder/s | Ton |
|---|---|---|---|
| `VideoSmk/*.smk` (51) | 352 × 240 (295: 352 × 216, 10: 432 × 352) | 15 (9: 25) | Spur 0 stereo, meist Spur 1 (mono oder stereo), bei 2500–2530 auch Spur 2; 8 Bit, 22050 Hz (9: 44100 Hz), alle gepackt |
| `TOOLGFX/6s.smk` | 320 × 220 | 15 | Spur 0 stereo |

Alle sind `SMK2` ohne Ringbild, ohne Zeilensprung und ohne Zeilenverdopplung; die Palette ändert sich
nur ein- bis viermal je Video (Überblendungen sind im Bild selbst gemalt). Der Decoder schafft auch im
Debug-Build rund ein Bild je Millisekunde.

## Aufbau

Alle Zahlen Little-Endian.

| Teil | Größe |
|---|---|
| Kopf | 0x68 Byte |
| Bildgrößen | 4 Byte je Bild (Bit 0: Schlüsselbild, Bit 1 unbekannt; beide vor dem Gebrauch löschen) |
| Bildarten | 1 Byte je Bild (Bit 0: Palette, Bit 1 + n: Tonspur n) |
| Huffman-Tabellen | `TreesSize` Byte, ein Bitstrom |
| Bilder | nacheinander, je Bild die Größe aus der Tabelle |

Gibt es ein Ringbild (Flag Bit 0), haben beide Tabellen ein Bild mehr als `Frames`.

### Kopf

| Offset | Feld |
|---|---|
| 0x00 | `SMK2` oder `SMK4` |
| 0x04 | Breite |
| 0x08 | Höhe |
| 0x0C | Bilder |
| 0x10 | Bildrate (vorzeichenbehaftet): > 0 Millisekunden je Bild, < 0 Hunderttausendstelsekunden je Bild, 0 = 10 Bilder/s. Das Spiel: −6666 (15 Bilder/s) |
| 0x14 | Flags: Bit 0 Ringbild, Bit 1 Zeilensprung, Bit 2 Zeilenverdopplung (beides: doppelte Höhe auf dem Schirm) |
| 0x18 | 7 × größter entpackter Tonblock je Spur |
| 0x34 | `TreesSize` |
| 0x38 | Tabellengrößen der vier 16-Bit-Bäume (Bildmasken, Zweifarbfarben, Vollblöcke, Blockarten), in Byte zu 4 je Eintrag |
| 0x48 | 7 × Tonspur: Bit 31 gepackt, 30 vorhanden, 29 16 Bit, 28 stereo, 27–26 andere Packart (unbekannt, abgelehnt), 0–23 Abtastrate |
| 0x64 | unbenutzt |

### Bitstrom

Bits werden vom niedrigsten Bit jedes Bytes an gelesen; mehrstellige Werte beginnen mit ihrem niedrigsten
Bit.

**Bytebaum** (8 Bit): ein Bit „vorhanden“; fehlt der Baum, ergibt er beim Lesen 0 und verbraucht keine
Bits. Sonst rekursiv: Bit 1 = Knoten (erst der 0-Zweig, dann der 1-Zweig), Bit 0 = Blatt mit 8 Bit
Wert. Danach ein abschließendes 0-Bit. Beim Dekodieren wählt jedes gelesene Bit den Zweig (0 links,
1 rechts) bis zu einem Blatt; ein Baum aus nur einem Blatt verbraucht keine Bits.

**16-Bit-Baum**: Bit „vorhanden“ (sonst: ein Blatt 0), Bytebaum der niedrigen, Bytebaum der hohen
Bytes, drei 16-Bit-**Marken**, dann der Baum wie oben, aber jedes Blatt ist „niedriges Byte (mit dem
ersten Bytebaum gelesen) | hohes Byte (mit dem zweiten) << 8“, dann ein abschließendes 0-Bit. Ein Blatt
mit dem Wert einer Marke ist ein **Merkblatt**: es enthält keinen festen Wert, sondern einen der drei
zuletzt gelesenen. Zu Beginn jedes Bildes sind alle drei 0. Liest der Decoder einen Wert, der sich vom
Inhalt des Merkblatts 1 unterscheidet, rückt Merkblatt 2 in 3, 1 in 2, und der neue Wert kommt in 1.
Marken ohne Blatt zählen nicht.

Die vier 16-Bit-Bäume liegen in dieser Reihenfolge hintereinander im Tabellenbereich.

### Ein Bild

In dieser Reihenfolge, je nach Bildart:

1. **Palette**: ein Byte Länge (Größe des ganzen Blocks samt diesem Byte, durch 4), dann Befehle,
   bis 256 Einträge erreicht sind oder der Block endet (ein angefangener Befehl am Ende ist Auffüllung):
   - `1ccccccc`: die nächsten c + 1 Einträge bleiben, wie sie waren;
   - `01cccccc ssssssss`: c + 1 Einträge aus der **vorigen** Palette ab Eintrag s kopieren;
   - `00rrrrrr 00gggggg 00bbbbbb`: ein neuer Eintrag aus 6-Bit-Anteilen, verbreitert mit 4·c + c/16
     (0 → 0, 16 → 65, 63 → 255).

   Die Reihenfolge Rot, Grün, Blau ist an den Videos des Spiels geprüft (die Quelle nennt Blau zuerst;
   damit wären Haut und Pergament blau).
2. **Tonblöcke** der Spuren 0 … 6 mit gesetztem Bit: 4 Byte Länge (samt diesem Feld), bei gepackten
   Spuren 4 Byte entpackte Länge, dann die Daten.
3. **Bild**: der Rest. Das Bild besteht aus 4 × 4-Blöcken, zeilenweise von links oben (Breite und Höhe
   auf ganze Blöcke aufgerundet). Es baut auf dem vorigen Bild auf.

### Blöcke

Ein Wert des Blockartbaums beschreibt eine Kette: Bits 0–1 Art, Bits 2–7 Index der Kettenlänge
(1 … 59, dann 128, 256, 512, 1024, 2048), Bits 8–15 Zusatz. Die Kette reicht höchstens bis zum Bildende.

| Art | Name | Inhalt je Block |
|---|---|---|
| 0 | zwei Farben | Wert des Farbbaums (hohes Byte: Farbe der 1-Bits, niedriges: der 0-Bits), Wert des Maskenbaums: 16 Bits, Bit i = Pixel i (Zeile für Zeile, links nach rechts) |
| 1 | voll | je Zeile zwei Werte des Vollbaums: der erste füllt die Pixel 2 und 3 (niedriges Byte links), der zweite die Pixel 0 und 1 |
| 2 | leer | bleibt wie im vorigen Bild |
| 3 | einfarbig | alle Pixel in der Farbe des Zusatzbytes |

Die Reihenfolge der beiden Werte bei vollen Blöcken ist an den Videos geprüft (die Quelle sagt nichts
dazu; umgekehrt wären alle Kanten gezackt). Bei `SMK4` beginnen Ketten voller Blöcke mit Bits für eine
Unterart: `0` wie oben, `10` doppelt (ein Wert füllt zwei 2 × 2-Felder nebeneinander), `11` halb (nur
jede zweite Zeile, die darunter wiederholt sie). Das Spiel hat keine `SMK4`-Dateien; diese Unterarten
sind nur nach der Beschreibung umgesetzt.

### Ton

Ungepackt: PCM (8 Bit ohne Vorzeichen oder 16 Bit). Gepackt (Huffman-DPCM), je Block ein Bitstrom:

1. Bit „Daten“ (0: der Block ist stumm), Bit stereo, Bit 16 Bit (beide müssen zum Kopf passen);
2. Bytebäume: einer je Kanal, bei 16 Bit zwei je Kanal (niedriges, hohes Byte; links vor rechts);
3. Startwerte, **rechter Kanal zuerst**, bei 16 Bit das hohe Byte zuerst; sie sind das erste Sample;
4. dann je Sample und Kanal (links, rechts) eine Differenz aus dem Baum des Kanals (bei 16 Bit niedriges
   und hohes Byte zu einer 16-Bit-Zahl), auf den vorigen Wert addiert (mit Überlauf), bis die entpackte
   Länge erreicht ist.

Die Spuren des Spiels decken ihre Videos auf die Zehntelsekunde ab (geprüft: 911, 282). Open1602 gibt
eine Spur als Ganzes an den Mischer ([audio.md](../re/audio.md)).

## Prüfung

- `o1602-tool smk <datei> [--png ORDNER] [--every N] [--wav SPUR AUS.wav]`: Kopf, alle Bilder dekodieren,
  Bilder als PNG, eine Spur als WAVE.
- `tests/unit/test_video.cpp` baut kleine Dateien Bit für Bit (alle Blockarten, Ketten, Merkblätter,
  Palettenbefehle, die drei Tonarten, beschädigte Dateien); `tests/data/test_video_data.cpp` dekodiert
  ein Video des Spiels ganz.
- Geprüft an 12 der 51 Videos und `6s.smk` (alle Bilder, Stichproben angesehen).
