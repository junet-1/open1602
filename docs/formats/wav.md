# WAV – Geräusche, Sprache, Musik

RIFF-WAVE-Dateien in drei Ordnern. Was das Spiel damit macht, steht in [audio.md](../re/audio.md).
Open1602: `audio::decode_wav` (`engine/audio/src/wav.cpp`).

| Ordner | Inhalt | Format |
|---|---|---|
| `SAMPLES` (106) | Geräusche | PCM, mono, 8 Bit (ohne Vorzeichen, Stille 0x80), 22050 Hz (zwei ungenutzte 11025 Hz) |
| `Speech8` (309) | Sprachmeldungen, `<Nummer>.wav` | PCM, mono, 8 Bit, 22050 Hz |
| `MUSIC8` (21) | Musik, nach Titel benannt | eigenes 4-Bit-Format, stereo, 22050 Hz (unten) |

## 4-Bit-Musik

Der `fmt `-Block beschreibt den **entpackten** Strom (Formatkennung 1 = PCM, 2 Kanäle, 22050 Hz,
88200 Byte/s, Blockgröße 4) bis auf `wBitsPerSample = 4`: das ist das Zeichen für „gepackt“. Danach folgt
ein `fact`-Block mit 4 Byte („M.K.“, ohne Bedeutung) und der `data`-Block.

Die Daten sind **ein durchgehender Strom** ohne Blöcke; der Zustand beginnt am Dateianfang bei 0 und wird
nie zurückgesetzt. **Ein Byte ist ein Stereo-Paar**: das obere Halbbyte links, das untere rechts. Je Kanal:

```
schritt = Schritttabelle[index]                       (die 89 Werte von IMA-ADPCM, 7 … 32767)
index   = begrenzt(index + Indextabelle[n], 0, 88)    (−1, −1, −1, −1, 2, 4, 6, 8, zweimal)
diff    = ((n & 4 ? 4·schritt : 0) + (n & 2 ? 2·schritt : 0) + (n & 1 ? schritt : 0)) >> 2
falls n & 8: diff = −diff
wert    = begrenzt(wert + diff, −32768, 32767)        (16 Bit)
```

Anders als der Referenz-Code von IMA fehlt der Rundungsanteil `schritt >> 3`. Mit ihm übersteuert das
Thema tausendfach und driftet weg; ohne ihn (wie hier) bleibt es sauber (geprüft am ganzen „ANNO1602
Theme“, 110 s). Länge = Datenbytes / 22050 s.

`Maxsound.dll` entpackt beim Abspielen in einen Ringpuffer (`adpcm_decoder 0x28101220`, Tabellen bei
`0x28105020`/`0x28105060`).
