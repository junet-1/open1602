# 0008 – Spieleinstellungen

Status: angenommen · 17.10.2026

## Kontext
Spätere Teile der Anno-Reihe lassen das Endlosspiel weit genauer einstellen als Anno 1602:
Startkapital, Gegner, Katastrophen, Rückerstattung beim Abriss und vieles mehr. Open1602 soll das
ebenfalls können. Bisher gab es mit `sim::Rules` nur Schalter für Abweichungen vom Original
(ADR 0006).

## Entscheidung
1. **Alle Einstellungen eines Spiels stehen in `sim::Rules`**, gleich ob sie eine Eigenheit des
   Originals abschalten oder eine neue Wahl bieten. Es gibt keine zweite Stelle dafür.
2. Jede Einstellung hat einen **Originalwert**, den `Rules::original()` liefert, und einen
   Standardwert für Open1602. Wo es um eine freie Wahl geht und nicht um einen Fehler des Originals,
   ist der Standard das Verhalten des Originals.
3. Einstellungen gehören zum **Spielstand** und zur **Mehrspieler-Sitzung**. Sie werden vor dem
   Spielstart gewählt; ändern sie sich während eines Spiels, dann nur über einen Befehl
   (ADR 0004), damit alle Rechner dasselbe rechnen.
4. Die Simulation liest Einstellungen nur aus `Rules`. Oberfläche, Spielstart-Dialog und
   Kommandozeile füllen `Rules` aus und kennen die Regeln selbst nicht.
5. Neue Einstellungen werden im Code an der Stelle dokumentiert, die sie auswerten, und in der
   Verhaltensbeschreibung (`docs/re/*.md`) im Abschnitt „Open1602 und das Original“.

## Erste Einstellungen
| Einstellung | Werte | Original | Standard |
|---|---|---|---|
| Rückerstattung beim Abriss (`demolish_refund`, `refund_percent`) | keine, teilweise (1–100 %, Vorgabe 50 %), voll | keine | keine |
| Schiffe je Spieler (`max_ships_per_player`, L-001) | Zahl, 0 = unbegrenzt | 33 | unbegrenzt |
| Rohstoffe garantieren (`guarantee_resources`, S-019) | ja/nein | nein | ja |
| höchste Spielgeschwindigkeit (`max_game_speed`, L-004) | Faktor | 4 | 8 |
| Dürren (`droughts`) | wie das Spiel (Schwierigkeit), aus, selten, normal, häufig | wie das Spiel | wie das Spiel |
| Vulkane (`volcanoes`) | wie das Spiel (Einstellung), aus, an | wie das Spiel | wie das Spiel |

## Folgen
Der Spielstand von Open1602 braucht einen eigenen Abschnitt für die Einstellungen; Spielstände des
Originals bekommen beim Laden `Rules::original()` für die Wahlpunkte. Der Spielstart-Dialog wird
eine Liste von Einstellungen mit Gruppen (Wirtschaft, Gegner, Katastrophen …), wie in späteren
Anno-Teilen.
