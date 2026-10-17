# Was Open1602 von dir braucht

Stand: 17.10.2026 (Vulkane und Speichern eingebaut). Diese Liste sammelt alles, was ich (Claude) von dir an Spielständen, Bildern,
Videos und Beobachtungen im Original brauche oder schon bekommen habe. Sie wird fortgeschrieben,
sobald etwas dazukommt oder erledigt ist.

## So gibst du mir etwas

- **Spielstände:** Bitte in **neuen Plätzen** speichern (ab „Leer03“), nie über Merlin01, Merlin02
  oder Merlin03: Die Tests vergleichen genau diese Stände mit Open1602. Gib dem Stand einen
  sprechenden Namen (z. B. „VorVulkan“) und schreib mir kurz, was du davor getan hast.
- **Übergabe:** Nichts zu kopieren. Der Installationsordner unter Heroic ist für mich freigegeben;
  ich lese `SAVEGAME/` und `Game.dat` dort direkt (nur lesend, die Installation bleibt unverändert).
  Sag mir einfach den Namen des Stands.
- **Videos und Bildschirmfotos:** in den Ordner `eingaben/` in deinem Projektordner „Projekt Anno“.
  Den Ordner lese ich direkt; Git übernimmt ihn nicht (Originaldaten kommen nie ins Repository).
  Bildschirmfotos am besten in der kleinsten Grafikstufe (SGFX) und 1024 × 768, wie beim ersten.
- **Spielzeit:** Das Original zählt Spielzeit, nicht Echtzeit. Bei normaler Geschwindigkeit ist sie
  gleich; bei schnellerer Geschwindigkeit vergeht sie entsprechend schneller.

## Schon geliefert

| Was | Wofür | Stand |
|---|---|---|
| Freigabe der GOG-Installation unter Heroic (Grafik, `.cod`-Dateien, Szenarien, Spielstände, Musik `MUSIC8`, Geräusche `SAMPLES`, Sprache `Speech8`, Videos `VideoSmk`) | Grundlage für alles | ✅ vollständig lesbar |
| Merlin01 (`game00.gam`) und `lastgame.gam`, auch in der ersten, später überspeicherten Fassung | Spielstand-Format, Städte, Lager, Produktion, Häuser, Verbrauch, Geld | ✅ beide Fassungen werden getestet |
| Bildschirmfoto des Originals (Merlin01, SGFX, 1024 × 768) und eines mit stillgelegten Betrieben | Abgleich der Grafik, Flaggen, Statussymbole | ✅ Korrelation 0,967 |
| Bestätigungen im laufenden Spiel: Kontorbestände von Lemonhall, Antilopen und Hirsche, Flaggenwahl | Einheiten der Lagerbestände, Tiere | ✅ |
| Merlin02 und Merlin03 mit Jagdhütte, dazu das Video `Anno1602_Jagd.mp4` | Jagd und Hirsche; Merlin02 → Merlin03 läuft als Feld-für-Feld-Test | ✅ |
| Rückmeldung zum Bauen im Betrachter (unsichtbare Vorschau) | Bauwerkzeug | ✅ behoben; deine Bestätigung steht aus (unten) |

## So bekommst du etwas von mir

Dateien, die ich für dich erzeuge (zum Beispiel Spielstände zum Ausprobieren im Original), lege ich in
den Ordner `ausgaben/` in deinem Projektordner. Git übernimmt ihn nicht.

## Jetzt sinnvoll

Nach Nutzen geordnet. Nichts davon ist dringend; alles läuft auch ohne weiter.

1. **Ein Spielstand von Open1602 im Original** (prüft das Speichern). In `ausgaben/` liegt
   `Open1602Test.gam`: Merlin03, in Open1602 zehn Minuten weitergespielt (Spielzeit 48:14, Gold
   3206, Lemonhall mit 50 t Holz und 28 t Werkzeug). Seit dem 17.10. mittags enthält er auch Open1602s
   eigenen Zusatzabschnitt am Ende; der Test prüft damit zugleich, dass das Original ihn überspringt.
   Ich schreibe nie selbst in deine Installation; so kannst du ihn laden:
   - im Original Merlin03 laden und auf einen leeren Platz speichern, Name „Open1602Test“; beenden;
   - die Datei dieses Platzes (`SAVEGAME/game03.gam` beim Platz „Leer03“) durch `Open1602Test.gam`
     ersetzen (vorher sichern, wenn du magst);
   - das Original starten und „Open1602Test“ laden.

   Lädt er? Stimmen Spielzeit, Gold und Lager? Läuft das Spiel ein paar Minuten ohne Absturz? Ein
   Bildschirmfoto oder ein Stand, den du danach im Original speicherst („NachTest“), hilft mir.

2. **Rückmeldung zum Betrachter.** Startet die Kamera über Lemonhall, und siehst du die Vorschau beim
   Bauen (rote Fläche, wo nichts geht)? Siehst du in deiner eigenen Stadt keine Häuser, schick mir
   die Zeilen `renderer: …` und `GFX: …` aus dem Terminal.

3. **Vulkanausbruch** (prüft die eingebauten Vulkane). In deinem Spiel ist der erste Ausbruch für
   **84,5 Minuten Spielzeit** angesetzt, also etwa 46 Minuten nach Merlin03. Auf welcher der drei
   Inseln mit Vulkan er kommt, ist Zufall; keine davon ist deine.
   - einen Stand kurz davor (etwa bei Minute 83), „VorVulkan“;
   - einen kurz nach der Meldung „Ein Vulkan ist ausgebrochen!“, „NachVulkan“ (der Stand zeigt den
     nächsten Termin und welcher Krater es war);
   - einen etwa eine Minute danach, „NachVulkan2“ (zerstörte Bäume und Gebäude);
   - wenn möglich ein Video des Ausbruchs: wie lange er dauert, wie weit die Brocken fliegen.

4. **Dürre** (prüft die eingebaute Dürre). Ab **90 Minuten Spielzeit** sind Dürren möglich; bei
   deiner Schwierigkeit kommt die erste meist bald danach. Damit es etwas zu verdorren gibt, bau
   vorher in Lemonhall ein paar **Felder oder Plantagen** (Getreide, Tabak oder was die Insel trägt).
   - ein Stand während der Dürre (nach „Die Trockenheit vernichtet Eure Ernte!“), „Duerre“;
   - einer etwa zwei Minuten nach ihrem Ende (es gibt keine Meldung dafür; die verdorrten Felder
     wachsen dann wieder), „NachDuerre“.

5. **Zwei Stände kurz hintereinander** (für den Test des Stadttakts mit Geld und Verbrauch). Von den
   neuen Spielständen liegen keine zwei weniger als 10 Sekunden Spielzeit auseinander. Lade einen
   Stand, speichere sofort als „TaktA“, lass das Spiel etwa 5 Sekunden laufen und speichere als
   „TaktB“. Je größer die Stadt, desto mehr prüft das.

6. **Eine größere Stadt.** Für Brände, Pest, Räuber und die höheren Bewohnerklassen:
   - Brand: ab etwa 80 Pionieren oder 250 Pionieren und Siedlern;
   - Pest: ab 200 Siedlern und höher; Bürger, Kaufleute, Aristokraten in halb vollen Häusern;
   - Räuber: ab 300 Bürgern und höher.

   Spielstände mit Bürgern und Kaufleuten prüfen Verbrauch, Zufriedenheit und Hausaufstieg der
   höheren Klassen. Brände und Seuchen selbst stehen nicht im Spielstand des Originals; hier hilft nur
   ein **Video**: ein Brand mit anrückender Feuerwehr, eine Pest mit dem Arzt, ganz selten ein Räuber
   (er überfällt Träger und Marktkarren auf dem Heimweg). Wie lange ein Brand dauert und wohin er
   springt, sieht man darauf gut.

7. **Schloss oder Kathedrale**, wenn du so weit kommst: ein Stand unmittelbar vor dem Bau und einer
   etwa eine Minute danach. Der Unterschied in der Zufriedenheit prüft den „Stolz“ der Stadt.

## Später, wenn die Systeme kommen

| Wann | Was | Wofür |
|---|---|---|
| Militär | Stände mit Soldaten im Kampf, mit einem zerstörten Kontor und einem Wachturm daneben (S-018), mit einem zerstörten Eingeborenendorf | Kampf, Gebietsverlust, Fluch der Eingeborenen |
| Schiffe und Handel | Stände mit Handelsrouten, beladenen Schiffen, dem freien Händler | Schiffsbewegung, Laden, Handel |
| Computergegner | Stände eines länger laufenden Spiels mit wachsenden Gegnerstädten | KI |
| Mehrspieler | erst wenn Open1602 Netzwerk kann | – |

## Nur bei Gelegenheit

Punkte, die aus dem Code gefolgert und im Spiel nie gesehen wurden. Eine Beobachtung im Original
würde sie bestätigen oder widerlegen; Open1602 vermeidet sie ohnehin.

- **U-007 (alter Weg beim Ziehen):** Auf einer Insel mit zwei eigenen Städten: in Stadt A eine Straße
  ziehen, dann in Stadt B mit dem Ziehen beginnen und die Maus nach Stadt A bewegen. Beginnt der
  angezeigte Weg am alten Startpunkt in A?
- **S-018 (Wachturm hält Gebiet):** gehört zum Militär, siehe oben.
