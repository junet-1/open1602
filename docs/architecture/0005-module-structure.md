# 0005 – Modulstruktur und Abhängigkeitsregeln

Status: angenommen · 16.10.2026

```
            core            Logging, Dateien, Fehler (Result), Text, Festkomma, RNG
              ▲
            assets          Loader für Originalformate (keine Plattform-/Grafikabhängigkeit)
              ▲
             sim            Deterministische Simulation (Welt, Inseln, Gebäude, Wirtschaft, KI …)
           ▲     ▲
         net   script       Lockstep-Multiplayer / Modding
           ▲
platform ◄─ render, audio, ui ◄─ app
```

Regeln:
- Pfeile zeigen auf Abhängigkeiten; Zyklen sind verboten.
- `sim` hängt **nie** von `platform`, `render`, `audio`, `ui` ab (Headless-Fähigkeit, Determinismus).
- Jedes Modul ist eine eigene CMake-Bibliothek `o1602::<modul>` mit `include/o1602/<modul>/` als
  öffentlicher Schnittstelle; Interna liegen in `src/`.
- Namensraum `o1602::<modul>`.

Bereits umgesetzt: `core`, `assets`, `sim` (Inselraster), `render` (Software-Renderer). Die übrigen Module sind in `engine/<modul>/README.md` beschrieben
und werden mit ihren Meilensteinen angelegt.
