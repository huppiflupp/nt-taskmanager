# NT Task Manager

Ein Systemmonitor in der Form des Windows-NT-4.0-Taskmanagers: Prozesse
mit CPU-Last und Speicherbelegung, Beenden per Knopf, und die Dienste des
Systems.

Gedacht als Beiwerk zum Plasma-Design
[NT Legacy](https://github.com/huppiflupp/NiceOS9-theme) — es läuft aber
unabhängig davon und sieht dann eben aus wie der Rest deines Desktops.

---

## Das Programm zeichnet nichts selbst

Das ist die eine Entscheidung, aus der alles andere folgt.

Es gibt in diesem Quelltext kein Stylesheet, keine gesetzte Palette, keine
eigenen Farben und keine gemalten Rahmen. Alles, was das Fenster nach
Windows NT aussehen lässt — die versenkte Tabelle, die 3D-Spaltenköpfe,
die Reiterleiste, die dreigeteilte Statuszeile — kommt vom **Widget-Stil**
des Systems. Das Design NT Legacy setzt `widgetStyle=Windows`, und der
Qt-Stil dieses Namens zeichnet genau diese Formen.

Der Gewinn: Das Fenster wechselt die Farbwelt mit, wenn du im Design von
Petrol auf Wüste oder auf eine Nachtfassung umschaltest. Ohne eine Zeile
Code dafür.

Der Preis, und der ist bewusst bezahlt: Unter Breeze sieht es aus wie
Breeze. Ein Programm, das sein Aussehen erzwingt, passt genau einmal — und
danach nie wieder.

## Abhängigkeiten

Genau eine: **Qt 6** (Widgets und DBus).

Naheliegend wären `libksysguard` für die Prozesse und `libtaskmanager` für
die Fensterliste; beide liegen auf jedem Plasma-System. Gebraucht werden
sie nicht. systemd spricht D-Bus, und D-Bus steckt in Qt. Die Prozessdaten
stehen in `/proc`, und die Formate sind in `proc(5)` festgeschrieben.

## Bauen

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/nt-taskmanager
```

Auf Fedora braucht es dafür `qt6-qtbase-devel`.

## Was drin ist

| Reiter | Inhalt |
|---|---|
| **Processes** | Name, Benutzer, CPU-Last, Speicher, PID. Sortierbar, Prozess beenden. |
| **Services** | Die systemd-Units vom Typ `.service` mit Beschreibung und Status. |

Zur CPU-Spalte: Die Werte summieren sich über alle Prozesse auf 100 %,
nicht auf 100 % je Kern. So macht es der Windows-Taskmanager; `top` macht
es andersherum, und wer beide nebeneinander laufen lässt, sollte wissen
warum die Zahlen auseinandergehen.

Beendet wird mit `SIGTERM`, nicht `SIGKILL` — der Prozess soll aufräumen
dürfen. Gehört er einem anderen Benutzer, läuft der Aufruf über `pkexec`,
das den Authentifizierungsdialog des Systems öffnet.

## Was nicht drin ist

**Applications.** Der Reiter mit der Fensterliste fehlt, und das hat einen
technischen Grund: Unter X11 wäre er drei Zeilen (`_NET_CLIENT_LIST`),
unter Wayland gibt es diesen Weg nicht mehr. Die Fensterliste käme dort
nur über ein KWin-Skript, das man per D-Bus in den Fenstermanager lädt und
das sich zurückmeldet — für den Nutzen zu viel Maschinerie.

**Performance.** Die Verlaufsgraphen sind das Einzige, was der Widget-Stil
nicht geschenkt liefert; sie müssten gezeichnet werden. Vielleicht später.

## Lizenz

GPL-2.0-or-later.
