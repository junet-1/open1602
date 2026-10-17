# Was Open1602 von dir braucht

Stand: 17.10.2026 (Schiffe, Handel, Werften und der Neustart wie im Original; als Nächstes das Militär). Diese Liste sammelt alles, was ich (Claude) von dir an Spielständen, Bildern,
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
| Test im Original: `Open1602Endlos.gam` (ein von Open1602 angelegtes Endlosspiel) geladen und gespielt | Neues Spiel im Originalformat | ✅ |
| Endlos1 und Endlos2 (`game06.gam`, `game07.gam`): zwei Endlosspiele des Originals direkt nach dem Start („Durchschnitt“, Merlin, Gelb) | Abgleich des Neustarts Byte für Byte | ✅ Inseln, Spieler, Städte, Kontore und Märkte stimmen; das Original würfelt die Inseltabelle bei jedem Start neu |
| Video `Schiffe_Fahren.mp4` (92 s, 4K) | Tempo und Drehen der Schiffe | ✅ ausgewertet: Tempo aller Schiffstypen auf ±1 % wie berechnet, Diagonalen √2 länger, Drehen wie angenommen; dazu der Stadttakt zur vollen 10. Sekunde und ein zusätzlicher beim Laden ([ships.md](re/ships.md#abgleich-mit-dem-original)) |
| Routenschiff_Vor und Routenschiff_Nach (`game08.gam`, `game09.gam`): ein freier Händler kurz vor und direkt nach dem Anlegen am eigenen Kontor | Handel des freien Händlers | ✅ der Handel stimmt genau (7 t Werkzeug zu je 105 an Lemonhall); die Computer behalten im selben Takt dasselbe Geld; kleine Abweichungen nur im Kurs einzelner Schiffe |
| Endlos_Laufend (`game10.gam`): zwei Inseln, erste Kaufleute, Ausbildung in der Burg, ein Kanonier bereit | Militär im Spielstand | ✅ lädt und wird Byte für Byte zurückgeschrieben, auch die Einheiten in der Burg |

## So bekommst du etwas von mir

Dateien, die ich für dich erzeuge (zum Beispiel Spielstände zum Ausprobieren im Original), lege ich in
den Ordner `ausgaben/` in deinem Projektordner. Git übernimmt ihn nicht.

## Jetzt sinnvoll

Nach Nutzen geordnet. Nichts davon ist dringend; alles läuft auch ohne weiter.

1. **Eigene Handelsroute** (freiwillig; Video und Händler-Stände sind schon da). Zwei Stände mit einer
   eigenen Handelsroute: einer direkt vor der Ankunft an einem Kontor („VorKontor“), einer kurz danach
   („NachKontor“). Damit prüfe ich Laden und Entladen nach Route.

2. **Rückmeldung zum Betrachter.** Startet die Kamera über Lemonhall, und siehst du die Vorschau beim
   Bauen (rote Fläche, wo nichts geht)? Siehst du in deiner eigenen Stadt keine Häuser, schick mir
   die Zeilen `renderer: …` und `GFX: …` aus dem Terminal. Neu: Linksklick wählt ein Schiff (Name,
   Tempo und Ladung stehen in der Titelzeile), Rechtsklick schickt es los (aufs Meer, an ein Kontor,
   zu einem anderen Schiff); ziehen mit der rechten Taste verschiebt wie bisher die Karte.

3. **Vulkanausbruch** (prüft die eingebauten Vulkane). In Merlin04 sind schon zwei Vulkane
   ausgebrochen (Inseln 5 und 6); das bestätigt den Ablauf. Es fehlt ein Stand direkt davor und danach.
   Der nächste Ausbruch ist in Merlin04 für **130:32 Spielzeit** angesetzt; auf welcher der drei
   Inseln mit Vulkan er kommt, ist Zufall, keine davon ist deine.
   - einen Stand kurz davor (etwa bei Minute 129), „VorVulkan“;
   - einen kurz nach der Meldung „Ein Vulkan ist ausgebrochen!“, „NachVulkan“ (der Stand zeigt den
     nächsten Termin und welcher Krater es war);
   - einen etwa eine Minute danach, „NachVulkan2“ (zerstörte Bäume und Gebäude);
   - wenn möglich ein Video des Ausbruchs: wie lange er dauert, wie weit die Brocken fliegen.

4. **Dürre** (prüft die eingebaute Dürre). In Merlin04 hat der Dürrewurf zwischen Minute 88 und 95
   schon einmal getroffen (auf welcher Insel, zeigt der Stand nicht mehr); der nächste ist ab
   **122:00 Spielzeit** möglich. Damit es etwas zu verdorren gibt, braucht Lemonhall Felder oder
   Plantagen; deine Gewürzplantage ist ein Anfang, mehr helfen.
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

8. **Militär** (prüft Ausbildung, Kampf und Eroberung, die ich gerade einbaue). Eine Burg bauen,
   je zwei Schwertkämpfer und Musketiere ausbilden (Schwerter und Musketen müssen im Kontor liegen)
   und ein Eingeborenendorf angreifen (Angriffszeiger auf eine Hütte), wenn du so weit kommst. Dazu:
   - ein Stand mit den fertig ausgebildeten Einheiten noch in der Burg, „Burg“;
   - einer kurz nach dem Angriffsbefehl, wenn die Krieger des Dorfs herauskommen, „Angriff“;
   - einer nach dem Fall des Dorfs (Meldung „Die Eingeborenen verfluchen Euch!“, falls sie kommt),
     „DorfZerstoert“;
   - ein Video des Gefechts: wie schnell die Einheiten schlagen und schießen, wie lange eine Hütte
     hält.

## Später, wenn die Systeme kommen

| Wann | Was | Wofür |
|---|---|---|
| Militär (über Punkt 8 hinaus) | ein Stand mit einem zerstörten Kontor und einem Wachturm daneben (S-018), ein Video eines Seegefechts mit Kriegsschiffen | Gebietsverlust, Breitseite und Tempo des Seekampfs |
| Computergegner | Stände eines länger laufenden Spiels mit wachsenden Gegnerstädten | KI |
| Mehrspieler | erst wenn Open1602 Netzwerk kann | – |

## Nur bei Gelegenheit

Punkte, die aus dem Code gefolgert und im Spiel nie gesehen wurden. Eine Beobachtung im Original
würde sie bestätigen oder widerlegen; Open1602 vermeidet sie ohnehin.

- **U-007 (alter Weg beim Ziehen):** Auf einer Insel mit zwei eigenen Städten: in Stadt A eine Straße
  ziehen, dann in Stadt B mit dem Ziehen beginnen und die Maus nach Stadt A bewegen. Beginnt der
  angezeigte Weg am alten Startpunkt in A?
- **S-018 (Wachturm hält Gebiet):** aus dem Code geklärt und beibehalten: Eine Stadt ist erst
  eroberbar, wenn Kontor, Märkte und Türme gefallen sind. Ein Stand mit zerstörtem Kontor und
  stehendem Turm würde es im Spiel bestätigen.
