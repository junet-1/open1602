# Was Open1602 von dir braucht

Stand: 17.10.2026 (Schiffe, Handel, freier Händler und Werften eingebaut). Diese Liste sammelt alles, was ich (Claude) von dir an Spielständen, Bildern,
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
| Test im Original: `Open1602Test.gam` und `Open1602Test2.gam` (mit Zusatzabschnitt) geladen und weitergespielt, ohne Absturz; danach `Merlin04_Laufend` (67:20, mit Gewürzplantage) | Spielstände schreiben, eigener Chunk | ✅ (Gold nach dem Laden 3219 statt 3206: vermutlich der erste Stadttakt) |

## So bekommst du etwas von mir

Dateien, die ich für dich erzeuge (zum Beispiel Spielstände zum Ausprobieren im Original), lege ich in
den Ordner `ausgaben/` in deinem Projektordner. Git übernimmt ihn nicht.

## Jetzt sinnvoll

Nach Nutzen geordnet. Nichts davon ist dringend; alles läuft auch ohne weiter.

1. **Neues Endlosspiel aus Open1602 im Original laden.** In `ausgaben/` liegt `Open1602Endlos.gam`,
   ein Endlosspiel, das Open1602 selbst angelegt hat (Stufe „Durchschnittlich“, gelbe Flagge, Name
   Merlin). Bitte in einen freien Platz kopieren, im Original laden und kurz ansehen: Inseln, die vier
   Schiffe mit Ladung, Geld 15 000, Eingeborenendörfer. Ein paar Minuten weiterspielen (Kontor bauen)
   und als neuen Stand speichern („Open1602Endlos_Laufend“).

2. **Ein Endlosspiel des Originals direkt nach dem Start**, sofort gespeichert („EndlosStart“), mit der
   gewählten Stufe. Damit vergleiche ich den ersten Spielstand Byte für Byte mit dem, was Open1602
   schreibt. Wenn du magst, dieselbe Stufe zweimal starten: Sind die Inseln verschieden, würfelt das
   Original den Startpunkt der Inseltabelle bei jedem Start neu.

3. **Schiffe im Original** (prüft Fahren, Handel und Händler). Ein kurzes Video, in dem ein eigenes
   Schiff aus dem Stand losfährt, wendet und ein Stück übers offene Meer fährt (Tempo und Drehen), und
   wenn möglich zwei Stände mit eigener Handelsroute: einer direkt vor der Ankunft an einem Kontor
   („VorKontor“), einer kurz danach („NachKontor“). Schön wäre außerdem, wenn du dir merkst, wann ein
   freier Händler an deinem Kontor anlegt und was er kauft oder verkauft (im Kontor sichtbar).

4. **Rückmeldung zum Betrachter.** Startet die Kamera über Lemonhall, und siehst du die Vorschau beim
   Bauen (rote Fläche, wo nichts geht)? Siehst du in deiner eigenen Stadt keine Häuser, schick mir
   die Zeilen `renderer: …` und `GFX: …` aus dem Terminal. Neu: Linksklick wählt ein Schiff (Name,
   Tempo und Ladung stehen in der Titelzeile), Rechtsklick schickt es los (aufs Meer, an ein Kontor,
   zu einem anderen Schiff); ziehen mit der rechten Taste verschiebt wie bisher die Karte.

5. **Vulkanausbruch** (prüft die eingebauten Vulkane). In Merlin04 sind schon zwei Vulkane
   ausgebrochen (Inseln 5 und 6); das bestätigt den Ablauf. Es fehlt ein Stand direkt davor und danach.
   Der nächste Ausbruch ist in Merlin04 für **130:32 Spielzeit** angesetzt; auf welcher der drei
   Inseln mit Vulkan er kommt, ist Zufall, keine davon ist deine.
   - einen Stand kurz davor (etwa bei Minute 129), „VorVulkan“;
   - einen kurz nach der Meldung „Ein Vulkan ist ausgebrochen!“, „NachVulkan“ (der Stand zeigt den
     nächsten Termin und welcher Krater es war);
   - einen etwa eine Minute danach, „NachVulkan2“ (zerstörte Bäume und Gebäude);
   - wenn möglich ein Video des Ausbruchs: wie lange er dauert, wie weit die Brocken fliegen.

6. **Dürre** (prüft die eingebaute Dürre). In Merlin04 hat der Dürrewurf zwischen Minute 88 und 95
   schon einmal getroffen (auf welcher Insel, zeigt der Stand nicht mehr); der nächste ist ab
   **122:00 Spielzeit** möglich. Damit es etwas zu verdorren gibt, braucht Lemonhall Felder oder
   Plantagen; deine Gewürzplantage ist ein Anfang, mehr helfen.
   - ein Stand während der Dürre (nach „Die Trockenheit vernichtet Eure Ernte!“), „Duerre“;
   - einer etwa zwei Minuten nach ihrem Ende (es gibt keine Meldung dafür; die verdorrten Felder
     wachsen dann wieder), „NachDuerre“.

7. **Zwei Stände kurz hintereinander** (für den Test des Stadttakts mit Geld und Verbrauch). Von den
   neuen Spielständen liegen keine zwei weniger als 10 Sekunden Spielzeit auseinander. Lade einen
   Stand, speichere sofort als „TaktA“, lass das Spiel etwa 5 Sekunden laufen und speichere als
   „TaktB“. Je größer die Stadt, desto mehr prüft das.

8. **Eine größere Stadt.** Für Brände, Pest, Räuber und die höheren Bewohnerklassen:
   - Brand: ab etwa 80 Pionieren oder 250 Pionieren und Siedlern;
   - Pest: ab 200 Siedlern und höher; Bürger, Kaufleute, Aristokraten in halb vollen Häusern;
   - Räuber: ab 300 Bürgern und höher.

   Spielstände mit Bürgern und Kaufleuten prüfen Verbrauch, Zufriedenheit und Hausaufstieg der
   höheren Klassen. Brände und Seuchen selbst stehen nicht im Spielstand des Originals; hier hilft nur
   ein **Video**: ein Brand mit anrückender Feuerwehr, eine Pest mit dem Arzt, ganz selten ein Räuber
   (er überfällt Träger und Marktkarren auf dem Heimweg). Wie lange ein Brand dauert und wohin er
   springt, sieht man darauf gut.

9. **Schloss oder Kathedrale**, wenn du so weit kommst: ein Stand unmittelbar vor dem Bau und einer
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
