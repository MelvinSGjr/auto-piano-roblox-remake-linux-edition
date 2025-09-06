# auto-piano-roblox-remake-linux-edition
Auto Piano Remake for Roblox (Linux *and Unix systems (in theory)*)

## Disclaimer: 
Yes, the Python version of what I remade (the existing auto piano with my improved legit mode) works, but that doesn't mean it's perfect and smooth; it has more bugs than on Windows. So, if you have Linux or another Unix system that supports C, I'm ready to introduce you to this project. And, yes, I did not create this code with the assistance of AI, as I learned to program using AI, but I did write the code myself.

Sure! Here's the text adapted for your `README.md`, reflecting the transition to C with X11 support and the improved legit mode, all in a fun, South African-inspired style:

---

# 🎹 Linux/Unix X11 Piano Bot in C

_Howzit!_ Welcome to the most human, most chaotic, and most beautiful Roblox piano bot you'll ever come across — now rewritten in **C** for proper Linux/Unix systems with **X11 support**, and packed with even more _lekker_ features.

This is a heavily modified fork by **MelvinSGjr** (yep, that oke from GitHub) — now with 200% more realism, jazz, and good old-fashioned human error. No more Python — this thing runs bare-metal and speaks X11 like a native.

## What’s New?

🤖 **Legit Mode 3.0** — Your bot now plays like a real person: random delays, micro-pauses, missed notes, and the occasional _"ag, shame, finger slipped"_. No more robotic perfection!  
🎹 **Auto-Simplify for Insane MIDIs** — If your MIDI is a Beethoven-level monster, this script will gracefully dumb it down: breaks up hefty chords, skips impossible runs, and makes it sound like a real pianist (with two hands, not eight).  
🧠 **Humanization Everywhere** — Chords sometimes become arpeggios, fast passages get a bit messy, and everything is just more... alive.  
💻 **Full C Rewrite** — Better performance, closer to the metal, and proper X11 integration for key simulation.  
🗣️ **All comments and code style** are now in true MelvinSGjr fashion: sarcastic, concise, and (hopefully) useful.

## Usage (Linux & Unix only)

1. **Use the hotkeys** (see console) to control playback, speed, legit mode, and all the fancy stuff.

2. **Brag to your mates** that your bot is more human than theirs — and now it’s written in C. _Jislaaik!_

## Controls

| Key         | Action                      |
|-------------|-----------------------------|
| `DELETE`    | Play/Pause                  |
| `HOME`      | Rewind                      |
| `END`       | Advance                     |
| `PAGE UP`   | Speed up                    |
| `PAGE DOWN` | Slow down                   |
| `INSERT`    | Toggle Legit Mode (become human) |
| `F5`        | Reload song                 |
| `ESC`       | Exit                        |


---

To run these programs, you need to compile them and then run them. Here is a step-by-step guide:

## Compilation

1. **Install the necessary dependencies** (for example Ubuntu/Debian):
```bash
sudo apt-get update
sudo apt-get install build-essential libx11-dev libxtst-dev
```

2. **Compile midi_core.c**:
```bash
gcc -o midi_core midi_core.c
```

3. **Compile play_core.c**:
```bash
gcc -o play_core play_core.c -lX11 -lXtst -lpthread
```

## Running

1. **Process a MIDI file** using midi_core:
```bash
./midi_core path/to/your/file.mid
```
This will create the files `song.txt`, `sheetConversion.txt`, and `midiRecord.txt`.

2. **Start playback** using play_core:
```bash
./play_core
```

## Controls in play_core

- **DELETE** - Play/Pause
- **HOME** - Rewind
- **END** - Advance
- **PAGE UP** - Speed Up
- **PAGE DOWN** - Slow Down
- **INSERT** - Toggle Legit Mode (simulates human-like playing)
- **F5** - Load New Song
- **ESC** - Exit

## Notes

1. Make sure you have MIDI files to process. You can place them in the folder or specify the full path to the file.

2. The play_core program uses X11 to emulate key presses, so it only works on Linux/Unix systems with a graphical environment.

3. If you encounter permission issues, make the files executable:
```bash
chmod +x midi_core play_core
```

4. To process MIDI files, the midi_core program must be run first to create the song.txt file, which play_core then reads.

If you encounter any issues with compilation or running, please report the errors, and I will help resolve them.

---
## Contributions

Fancy making it even more human? PRs and issues are welcome. Just keep it fun, a bit weird, and remember — _’n boer maak ’n plan_.
