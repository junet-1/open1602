# engine/net

Mehrspieler im Lockstep über den Gastgeber ([ADR 0011](../../docs/architecture/0011-mehrspieler-lockstep.md)):
Nur Befehle reisen, jeder Rechner rechnet das ganze Spiel. Hängt nur von `sim` ab (und ENet), läuft also
auch ohne Fenster, etwa als fester Server oder in Tests.

| Datei | Inhalt |
|---|---|
| `protocol.hpp` | Nachrichten als Bytes und zurück (Little Endian, feste Breiten, Grenzen geprüft) |
| `transport.hpp` | Schnittstelle: zuverlässige, geordnete Nachrichten je Verbindung |
| `enet_transport.hpp` | Transport über UDP mit ENet (`third_party/enet`), Port 16020 |
| `loopback.hpp` | Netz im Speicher mit eigener Uhr, Latenz, Schwankung und Abbrüchen (Tests) |
| `session.hpp` | `HostSession` (sammelt Befehle, schließt Rahmen, vergleicht Fingerabdrücke, schickt Spielstände) und `ClientSession` (führt vollständige Rahmen aus) |

Im Spiel: `open1602 --host --scenario "Szenes/Am Ende allein.szm" --name Merlin` und auf den anderen
Rechnern `open1602 --join ADRESSE --name Anna`.
