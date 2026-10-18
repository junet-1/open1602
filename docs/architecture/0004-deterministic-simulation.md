# 0004 – Deterministische, befehlsbasierte Simulation

Status: angenommen · 16.10.2026

## Entscheidung
1. Die Simulation (`engine/sim`) läuft in **festen Ticks** und ist **bitgenau deterministisch**:
   - nur Ganzzahl-/Festkomma-Arithmetik (kein `float`/`double` im Simulationszustand),
   - eigener, seedbarer Zufallsgenerator im Spielzustand,
   - keine Iteration über ungeordnete Container, keine Zeiger als Sortierschlüssel,
   - keine Abhängigkeit von Systemzeit, Framerate oder Plattform.
2. Jede Spieleraktion ist ein serialisierbarer **Befehl** (`Command`), der zu einem definierten Tick
   angewendet wird. Darstellung und UI lesen den Zustand nur.
3. Über den Zustand wird pro Tick ein **Hash** gebildet.

## Begründung
- **Multiplayer** per Lockstep: Es werden nur Befehle übertragen; der Hash erkennt Desyncs sofort.
- **Replays, Tests, Headless-Server** fallen damit fast kostenlos ab.
- **Stabilität über lange Sitzungen**: Entitäten in Pools mit stabilen IDs statt verstreuter Allokationen;
  keine Allokation im Tick-Hotpath.

## Folgen
Werte aus `.cod`-Dateien mit Dezimalstellen (`Nahrung: 1.3`) werden beim Laden in Festkomma umgerechnet.
Rendering interpoliert zwischen Ticks und darf eine beliebige Framerate haben.
