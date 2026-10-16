# 0006 – Umgang mit Fehlern des Originals

Status: angenommen · 16.10.2026

## Kontext
Anno 1602 hat technische Probleme auf heutigen Systemen, spielmechanische Fehler und Exploits sowie
feste Grenzen, deren Überschreitung zu Fehlverhalten führt. Sie stehen gesammelt in
[docs/re/bugs.md](../re/bugs.md). Open1602 soll sie nicht übernehmen, aber Originalspielstände laden
können und das Spielgefühl erhalten.

## Entscheidung
1. **Technik (T, M):** Wird nicht nachgebildet. Die Plattformschicht (SDL3), eigene Lader und das
   eigene Netzwerk ersetzen die Originalkomponenten vollständig.
2. **Spielmechanik (S):** Fehler und Exploits werden **immer behoben**, ohne Schalter.
   Dafür gelten drei Regeln für den gesamten Simulationscode:
   - *Befehle prüfen sich selbst.* Jede Spieleraktion ist ein `Command` (ADR 0004). Die Simulation
     prüft beim Ausführen Besitzer, Reichweite, Kosten, Bau- und Stufenregeln. Die Oberfläche
     darf nichts erlauben, was der Befehl nicht prüft. (S-005, S-006, S-010 … S-015, S-017)
   - *Der gesamte Zustand gehört zum Spielstand.* Laden und Speichern ändern das Spiel nicht.
     Das betrifft auch Katastrophen, ausstehende Ereignisse und die Spielerzuordnung. Ein Test lädt
     einen gespeicherten Stand, simuliert N Ticks und vergleicht den Hash mit dem durchgelaufenen
     Original. (S-001, S-007, S-008, S-016)
   - *Verweise über stabile IDs, nie über Feldpositionen.* Träger, Tiere, Waren und Aufträge gehören
     zu einem Objekt. Verschwindet es, werden sie aufgelöst. (S-002, S-009)
3. **Grenzen (L, S-020, S-021):** Es gibt keine stillen technischen Grenzen. Container wachsen
   dynamisch, Pools vergeben stabile IDs. Wo eine Grenze nötig ist (Dateiformat, Balance), ist sie
   benannt, wird geprüft und erzeugt eine Fehlermeldung statt eines Fehlverhaltens.
4. **Abschaltbar ist nur, was Balance oder Spielgefühl betrifft.** Solche Punkte stehen in
   `sim::Rules`. Der Standard ist das korrigierte Verhalten, `Rules::original()` liefert das
   Originalverhalten. `Rules` ist Teil des Spielstands und der Mehrspieler-Sitzung, damit alle
   Teilnehmer dieselben Regeln haben.
5. **Datenfehler (D):** Der `.cod`-Interpreter bleibt originalgetreu. Korrekturen kommen als
   abschaltbare Daten-Patches, die nach dem Laden angewendet werden.
6. **Nachvollziehbarkeit:** Code, der einen Eintrag behebt, nennt seine Kennung (`// Fix S-002: …`).
   Jede Korrektur bekommt einen Test. `re/scripts/check_bug_refs.py` prüft in CI, dass jede im Code
   genannte Kennung im Katalog existiert.

## Originalspielstände
Originale `.GAM`-Dateien können Zustände enthalten, die nur durch einen Fehler entstanden sind,
etwa Gebäude auf früheren Flussfeldern (S-009) oder mehr als 33 Schiffe. Beim Import werden sie
geprüft. Unmögliche Zustände werden repariert oder gemeldet, nie stillschweigend übernommen.

## Folgen
- Einige Speedrun-Tricks und Exploits funktionieren in Open1602 nicht. Das ist gewollt.
- Paritätstests gegen das Original (Plan, Abschnitt 3.3) müssen Szenarien vermeiden, die einen
  behobenen Fehler auslösen, oder `Rules::original()` verwenden, soweit möglich.
