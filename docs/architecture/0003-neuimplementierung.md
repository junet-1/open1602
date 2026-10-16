# 0003 – Neuimplementierung statt Dekompilat

Status: angenommen · 16.10.2026

## Kontext
Möglich wären (a) eine Rekompilierung des dekompilierten Codes, (b) eine schrittweise Ersetzung
von Funktionen per Hook-DLL (frühes OpenRCT2) oder (c) eine Neuimplementierung nach Spezifikation.

## Entscheidung
**(c) Neuimplementierung.** Reverse Engineering liefert Spezifikationen (`docs/`), die Engine wird
danach sauber und modular neu geschrieben. Originalgetreues Verhalten wird durch Tests gesichert,
nicht durch übernommenen Code.

## Begründung
- Lesbarer, erweiterbarer Code (Projektziel) statt 1998er-Strukturen mit globalem Zustand.
- Saubere Trennung Simulation/Darstellung ist mit (a)/(b) kaum erreichbar, für Multiplayer aber nötig.
- Rechtlich sauberer.

## Folgen
- Paritätstests sind Pflicht: gleiche Ausgangslage im Original (Wine) und in Open1602, Zustand nach
  N Ticks vergleichen (`tests/parity/`, ab M3).
- Originalfehler werden in `docs/re/bugs.md` gesammelt und in Open1602 behoben; wo das das Spielgefühl
  ändert, per Option „Originalverhalten“ abschaltbar.
