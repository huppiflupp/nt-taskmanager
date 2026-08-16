# NT Task Manager

Ein Systemmonitor in der Form des Windows-NT-4.0-Taskmanagers: Prozesse
mit CPU-Last und Speicherbelegung, Beenden per Knopf, und die Dienste des
Systems.

![Processes](doc/processes.png)

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

## Bauen und installieren

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/nt-taskmanager
```

Auf Fedora braucht es dafür `qt6-qtbase-devel`, sonst nichts. Zum
Installieren:

```bash
sudo cmake --install build                          # nach /usr/local
cmake --install build --prefix ~/.local             # oder ins Benutzerverzeichnis
```

Beides legt das Programm und die `.desktop`-Datei ab; danach steht *Task
Manager* im Anwendungsstarter.

### Aufrufoptionen

| Option | Wirkung |
|---|---|
| `--obenauf` | Fenster im Vordergrund halten (siehe unten) |
| `--reiter <n>` | mit welchem Reiter das Fenster öffnet, `0` ist *Processes* |
| `--bild <datei>` | Fenster aufbauen, einmal messen, fotografieren, beenden |

`--bild` ist der Weg, wie die Bilder in dieser Datei entstehen. Nebenbei
schreibt es die Kennzahlen auf die Standardausgabe — so lässt sich
nachrechnen, ob die Zahlen im Fenster stimmen:

```
$ ./build/nt-taskmanager --bild x.png
Prozesse: 502  CPU: 41 %  Speicher: 35 %  Dienste: 239
```

## Was drin ist

| Reiter | Inhalt |
|---|---|
| **Processes** | Name, Benutzer, CPU-Last, Speicher, PID. Sortierbar, Prozess beenden. |
| **Services** | Die systemd-Units vom Typ `.service` mit Beschreibung und Status. |
| **Performance** | Prozessor, Grafikkarte und Arbeitsspeicher als Balken mit Verlauf. |
| **Networking** | Ethernet, WLAN und Bluetooth: je ein Verlauf, darunter die Tabelle. |

![Networking](doc/networking.png)

![Performance](doc/performance.png)

Im Menü: *New Task (Run…)*, *Always On Top*, *Refresh Now* (F5) und die
Taktstufen *High / Normal / Low / Paused*. Nicht übernommen sind
*Minimize On Use* und *Hide When Minimized* — beides regelt unter Plasma
der Fenstermanager, und kein Programm sollte sich das selbst nehmen.

![Services](doc/services.png)

Zur CPU-Spalte: Die Werte summieren sich über alle Prozesse auf 100 %,
nicht auf 100 % je Kern. So macht es der Windows-Taskmanager; `top` macht
es andersherum, und wer beide nebeneinander laufen lässt, sollte wissen
warum die Zahlen auseinandergehen.

Beendet wird mit `SIGTERM`, nicht `SIGKILL` — der Prozess soll aufräumen
dürfen. Gehört er einem anderen Benutzer, läuft der Aufruf über `pkexec`,
das den Authentifizierungsdialog des Systems öffnet.

## Die Grafikkarte

Die einzige Zeile im Performance-Reiter, die es 1996 nicht gab.

| Hersteller | Weg |
|---|---|
| NVIDIA | NVML (`libnvidia-ml`), zur Laufzeit nachgeladen |
| AMD | `/sys/class/drm/cardN/device/gpu_busy_percent` |
| Intel | fehlt — siehe unten |

NVML wird mit `dlopen` geholt und nicht dazugelinkt: Sonst liefe das
Programm auf keinem Rechner ohne NVIDIA-Treiber, und das sind die
meisten. Die beiden benötigten Strukturen sind selbst deklariert, weil
der NVML-Header nur im CUDA-Werkzeugkasten liegt — gegen die Bibliothek
geprüft, der Wert deckt sich aufs Prozent mit `nvidia-smi`.

Intel fehlt mit Absicht: Die Auslastung liegt dort hinter dem i915-PMU,
das ohne erhöhte Rechte nicht lesbar ist. Ein Systemmonitor, der nach dem
Passwort fragt, um einen Balken zu zeichnen, wäre die falsche Antwort.

## Warum grün auf schwarz

Balken und Verlauf sind die eine Stelle, an der das Programm doch selbst
zeichnet — die liefert kein Widget-Stil. Ihre Farben folgen deshalb
**nicht** dem Farbschema, sondern bleiben grün auf schwarz.

Das ist kein Versehen: Windows hat es genauso gehalten. Die Fensterfarben
folgten dem eingestellten Schema, die Anzeigen im Taskmanager blieben
grün. Wer sie einfärbt, verliert genau das Bild, das jeder kennt.

Der Rahmen um beide kommt dagegen sehr wohl vom Stil
(`QStyle::PE_Frame`), damit die Vertiefung dieselbe ist wie an jeder
Tabelle im Fenster.

## Das Netzwerk

Drei Arten von Adapter, drei Quellen:

| | Zähler | Zustand, Geschwindigkeit |
|---|---|---|
| Ethernet, WLAN | `/proc/net/dev` | `/sys/class/net/…` |
| Bluetooth | `ioctl HCIGETDEVINFO` | dasselbe `ioctl` |

Bluetooth taucht in `/proc/net/dev` nicht auf — dort stünde allenfalls
`bnep0`, und auch das nur, solange gerade eine PAN-Verbindung besteht.
Die Zähler des Adapters holt deshalb dasselbe `ioctl`, das auch
`hciconfig` benutzt; es braucht keine erhöhten Rechte. Die Struktur ist
selbst deklariert (der Header gehört zu `bluez-libs-devel`) und gegen
`hciconfig` geprüft: RX 2 988 490 und TX 30 409, auf das Byte gleich.

Die Auslastung in Prozent gibt es nur, wo eine Linkgeschwindigkeit
gemeldet wird — bei Ethernet also, bei WLAN und Bluetooth nicht. Dort
skaliert der Verlauf auf die höchste bisher gesehene Rate und schreibt
das in den Titel; ohne diesen Hinweis sähe eine Spitze bei 100 % nach
einer ausgelasteten Leitung aus, und sie heißt nur „so viel wie noch
nie".

## Always On Top unter Wayland

`Qt::WindowStaysOnTopHint` bleibt unter Wayland wirkungslos: Das
Protokoll kennt kein „immer oben", ein Fenster kann seine Lage im Stapel
dort grundsätzlich nicht selbst bestimmen.

Der Fenstermanager kann es sehr wohl. KWin nimmt dafür Anweisungen über
D-Bus entgegen, also lädt das Programm ein winziges Skript, das sein
eigenes Fenster an der Prozesskennung erkennt und `keepAbove` setzt.
Unter X11 genügt weiterhin die Fensterfahne.

Als Startoption gibt es dasselbe: `--obenauf`.

## Was nicht drin ist

**Applications.** Der Reiter mit der Fensterliste fehlt, und das hat einen
technischen Grund: Unter X11 wäre er drei Zeilen (`_NET_CLIENT_LIST`),
unter Wayland gibt es diesen Weg nicht mehr. Die Fensterliste käme dort
nur über ein KWin-Skript, das man per D-Bus in den Fenstermanager lädt und
das sich zurückmeldet — für den Nutzen zu viel Maschinerie.

## Wo was steht

| Datei | Zuständig für |
|---|---|
| `src/hauptfenster.*` | Fenster, Reiter, Menü, Statuszeile |
| `src/prozessquelle.*` | `/proc` lesen: Prozesse, CPU-Last, Speicher |
| `src/prozessmodell.*` | die Prozesstabelle als Qt-Modell |
| `src/dienstemodell.*` | systemd-Units über D-Bus |
| `src/leistungsseite.*` | der Reiter *Performance* |
| `src/netzquelle.*` | `/proc/net/dev`, sysfs und das Bluetooth-`ioctl` |
| `src/netzseite.*` | der Reiter *Networking* |
| `src/anzeigen.*` | Balken und Verlauf — das einzige selbst Gezeichnete |
| `src/gpuquelle.*` | NVML und der AMD-sysfs-Weg |

Die Quellen messen, die Seiten zeigen an. Wer eine weitere Größe
aufnehmen will, schreibt eine Quelle und hängt sie an eine Seite — die
Messung gehört nicht in den Zeichencode.

Alle Zahlen entstehen **einmal je Takt** und werden weitergereicht, nicht
an jeder Anzeigestelle neu gelesen. Sonst stünden im selben Fenster zwei
Werte für dieselbe Größe, die sich um ein Prozent unterscheiden.

## Was geprüft ist

Kein automatischer Test, aber jede Zahl ist gegen ein vorhandenes
Werkzeug gegengerechnet — auf demselben Rechner, im selben Zeitfenster:

| Wert | eigen | Gegenprobe |
|---|---|---|
| Prozesse | 512 | `ps ax` → 512 |
| Dienste | 240 | `systemctl` → 240 |
| Speicher | 54 % | `free` → 54 % |
| CPU | 47 % | `top` → 46 % |
| Prozesszeit | 78 s | `ps -o cputimes` → 78 s |
| GPU-Last | 33 % | `nvidia-smi` → 33 % |
| Bluetooth RX/TX | 2 988 490 / 30 409 | `hciconfig` → gleich |

## Fallstricke, die hier schon bezahlt sind

Falls jemand daran weiterbaut — das sind die Stellen, die stillschweigend
falsch rechnen, statt zu scheitern:

- **`QFile::atEnd()` ist bei `/proc` und `/sys` unbrauchbar.** Die
  Dateien melden Größe 0, also sagt `atEnd()` schon vor dem ersten Lesen
  ja, und die Schleife läuft null Mal durch. Immer `readAll()`.
- **Der Prozessname in `comm` ist auf 15 Zeichen begrenzt.** Aus
  `plasma-systemmonitor` wird `plasma-systemmo`. Der Name kommt deshalb
  aus dem `exe`-Symlink.
- **`QFileInfo::exists()` folgt dem Symlink.** Bei fremden Prozessen ohne
  Leserecht sagt es ebenfalls nein — Kernprozesse erkennt man an der
  leeren `cmdline`, nicht am fehlenden `exe`.
- **Der Name in `/proc/PID/stat` darf Klammern und Leerzeichen
  enthalten** („(Web Content)"). Deshalb wird hinter der *letzten*
  schließenden Klammer weitergelesen.
- **`MemFree` ist nicht der belegte Speicher.** Über `MemFree` gerechnet
  meldete ein gesundes System 95 % Belegung; richtig ist `MemAvailable`.
- **`Qt::WindowStaysOnTopHint` wirkt unter Wayland nicht** — siehe oben.

## Lizenz

GPL-2.0-or-later. Das Programm enthält kein fremdes Material.
