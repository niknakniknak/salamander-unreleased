Salamander (Life Force weapons, unreleased)
===========================================

A version of Salamander that was never released: Salamander's stages with the
Life Force weapon system (power-up bar) and Life Force's bonus life options.
Its code looks like a genuine Konami build from the time, between Salamander
and the Japanese Life Force. It survives only inside Gradius Origins.

This folder does not contain the game. salamandgo builds it from files you
already own and checks every ROM by CRC.

What you need
-------------
  origins.exe    the Gradius Origins executable
  salamand.zip   MAME Salamander (for the speech ROM 587-d08.8g and 587-c01.10a)
  lifefrce.zip   MAME Life Force (for the speech ROM 587-k08.8g)
                 lifefrcej.zip works too, and a merged salamand.zip has both

How to use it
-------------
  Windows: put the three files next to salamandgo.exe and run it, or drop
           them onto it.
  Linux:   ./salamandgo origins.exe salamand.zip lifefrce.zip
  Run it with nothing next to it and it lists what is missing.

It writes two zips next to the program: salamandgo.zip for the MiSTer and
mame/lifefrcej.zip for MAME. Add -mister or -mame to write only one of them.

On the MiSTer:
  salamandgo.zip  -> games/mame/
  the .mra file   -> _Arcade/ (or a folder of your own)
The .mra file is what you pick in the MiSTer menu to start the game. It tells
the Salamander arcade core how to load the ROMs from salamandgo.zip.

In MAME it runs as Life Force (Japan): mame lifefrcej -rompath mame
MAME warns the checksums are wrong; press a key to go on. Keep this zip in its
own folder, apart from your real lifefrcej.zip.

The speech ROM
--------------
Gradius Origins plays the voices from its own WAV files, but the game still
triggers them the arcade way, so salamandgo builds a speech ROM for it. It holds
Life Force's speech plus three Salamander lines (go to the right, go up, and an
intruder has penetrated our force field), taken from the two real speech ROMs.

If Gradius Origins is updated
-----------------------------
salamandgo finds the ROMs by checksum, not by position, so an update that
only moves things still works. If it says the executable did not have the ROMs,
they are now stored differently and the program needs updating.

Source: source/salamandgo.c (plain C, no libraries). Licence: GPL v3 or later, see LICENSE.txt.
  gcc -O2 -s -o salamandgo source/salamandgo.c
  x86_64-w64-mingw32-gcc -O2 -s -o salamandgo.exe source/salamandgo.c

Chris Watson (NIKNAK) <197681753+niknakniknak@users.noreply.github.com>, 2026
