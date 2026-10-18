# 0011 – Mehrspieler: Lockstep über den Gastgeber

Status: angenommen · 18.10.2026

Baut auf [ADR 0004](0004-deterministische-simulation.md) auf: Die Simulation ist deterministisch und
befehlsbasiert, also müssen im Netz nur die Befehle reisen. Umgesetzt in `engine/net`.

## Entscheidung

1. **Stern statt Peer-to-Peer.** Jeder Rechner ist nur mit dem Gastgeber verbunden. Nur der Gastgeber
   braucht einen erreichbaren Port (UDP 16020); die anderen verbinden sich nach außen, was durch
   Heimrouter ohne Freigabe geht (M-001). Ein Gastgeber ohne eigenen Spieler (fester Server) ist damit
   ebenfalls möglich.
2. **Rahmen.** Ein Rahmen ist ein Aufruf von `Simulation::tick()`, gezählt ab Beginn der Sitzung. Der
   Gastgeber sammelt die Befehle aller Spieler, legt jeden in den nächsten offenen Rahmen und schickt
   die Rahmen **vollständig** an alle („bis Rahmen N ist alles da“). Ein Rahmen ohne Befehle kostet
   nichts außer dieser Grenze. Ein Rechner führt nur vollständige Rahmen aus; alle führen so dieselben
   Befehle im selben Rahmen aus. In einer Pause laufen die Rahmen weiter, die Spielzeit steht: Befehle
   in der Pause (Bauen) gelten so auf allen Rechnern gleich. Die Sitzung treibt die Simulation statt
   `Simulation::advance()`; die Spielgeschwindigkeit bestimmt, wie viele Rahmen je Sekunde laufen.
3. **Vorlauf.** Der Gastgeber schließt Rahmen einige Rahmen vor seinem eigenen Spiel ab, so viele, wie
   eine Nachricht zum langsamsten Rechner braucht (aus der gemessenen Umlaufzeit, mit der
   Spielgeschwindigkeit, 2 bis 50 Rahmen). Er wächst sofort und schrumpft langsam. Ein Rechner mit mehr
   Rahmen in der Hand als dem Vorlauf holt auf.
4. **Langsame Rechner.** Der Gastgeber läuft höchstens eine Sekunde Spielzeit (plus Vorlauf) vor dem
   langsamsten Rechner; dann wartet er, und das Spiel wird für alle langsamer statt ungerecht. Steht ein
   Rechner 10 s ganz still (Debugger, Fenster gezogen), geht es ohne ihn weiter; er holt auf oder
   bekommt das Spiel neu, wenn er mehr als 30 s zurückliegt.
5. **Fingerabdruck.** Alle 100 Rahmen (5 s bei normaler Geschwindigkeit) schickt jeder Rechner dem
   Gastgeber `Simulation::state_hash()`. Der Gastgeber vergleicht mit seinem eigenen; nicht jeden Tick,
   weil jeder Abdruck einen Spielstand im Speicher kostet.
6. **Ein Weg für Start, Einstieg, Wiederverbinden und Desync: der Spielstand.** Der Gastgeber schickt
   einem Rechner sein Spiel als `.o1602`-Spielstand ([ADR 0009](0009-eigenes-speicherformat.md)) samt
   Geschwindigkeit, Pause und den schon abgeschlossenen Rahmen dahinter. So kommt ein Rechner am Anfang
   ins Spiel, in ein laufendes Spiel, nach einer abgebrochenen Verbindung zurück und nach einem Desync
   wieder in Gleichschritt (M-003). Der Gastgeber hat immer recht. Weicht ein Rechner auch direkt nach
   dem Laden wieder ab (andere Version), wird er nach zwei Versuchen getrennt. Eine Epochennummer je
   gesendetem Spiel verhindert, dass alte Meldungen eines Rechners das neue Spiel betreffen.
7. **Absender.** Jeder Befehl nennt seinen Spieler; der Gastgeber nimmt von einem Rechner nur Befehle
   für dessen Platz an (Review-Punkt 6), von Zuschauern keine, und höchstens 256 je Rahmen und Spieler.
   Nachrichten, die keinen Sinn ergeben, trennen den Rechner.
8. **Plätze.** Ein Rechner bekommt einen freien menschlichen Platz der Welt (oder den gewünschten) und
   ein Merkzeichen, mit dem er nach einem Verbindungsabbruch denselben Platz wieder bekommt. Ohne freien
   Platz schaut er zu. Ein verlassener Platz bleibt im Spiel und gibt keine Befehle, bis jemand
   zurückkommt. Der Name des Spielers wird als `PlayerRename` dieses Spielers gesetzt.
9. **Gleiche Daten.** Beim Beitritt prüft der Gastgeber Protokollversion und einen Fingerabdruck der
   Spieldaten, von denen die Simulation abhängt (`haeuser.cod`, `figuren.cod`, Stadt- und Schiffsnamen
   aus `Text.cod`).
10. **Transport.** Nachrichten reisen zuverlässig und geordnet über ENet 1.3.18
    (`third_party/enet`, MIT, unverändert mitgeliefert, damit der Bau ohne Netzzugriff geht). Die
    Sitzungen kennen nur die Schnittstelle `net::Transport`; für Tests gibt es ein Netz im Speicher
    mit Latenz, Schwankung und Abbrüchen, später lässt sich z. B. ein Relay oder Steam einhängen.

## Begründung

- Ein Stern mit Gastgeber hält die Reihenfolge der Befehle an einer Stelle fest: keine Abstimmung
  zwischen allen Rechnern, keine Abhängigkeit von der Netzreihenfolge, ein Ort für die Prüfung der
  Absender. Die Verzögerung ist eine Umlaufzeit zum Gastgeber; für ein Aufbauspiel unkritisch.
- Den Spielstand für alles zu nehmen, was „ein Rechner braucht das Spiel“ heißt, spart drei eigene
  Mechanismen und ist durch den Test „durchgehend gegen speichern und laden“ abgesichert.
- Die Netzlast ist unabhängig von der Spielgeschwindigkeit: eine Nachricht je Bild und Rechner.

## Folgen

- Videos, Menüs und Fenster halten im Mehrspieler das Spiel nicht an (M-018); nur Pause-Befehle tun es,
  für alle.
- Spielstände laden und neue Spiele beginnen ist während einer Sitzung gesperrt.
- Der Fingerabdruck kostet alle 100 Rahmen einen ungepackten Spielstand auf jedem Rechner.

## Offen

- Lobby und Mehrspieler-Menüs (heute `--host` und `--join` auf der Kommandozeile), Flaggenwahl,
  Chat, Mehrspieler-Endlosspiel (`.szm` mit Plätzen vom Gastgeber).
- Übernahme verlassener Plätze durch den Computer, Warten auf einen getrennten Spieler.
- Wechsel des Gastgebers, wenn er geht (heute endet das Spiel für alle), Relay für Gastgeber hinter
  strengem NAT, IPv6 (ENet 1.3 kann nur IPv4).
- Wiederholungen: Spielstand plus Rahmenfolge der Sitzung sind schon alles, was eine braucht.
