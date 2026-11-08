# Translating Artisans of Skyrim

Want to play Artisans in your own language, or share a translation with
everyone? Good news: you don't need the Creation Kit, xEdit or any coding.
Every text of the mod lives in one simple text file.

That includes the service menu, the messages, the courier letters and the
whole MCM.

## The file

You'll find it here after installing the mod:

```
Data/Interface/Translations/ArtisansOfSkyrim_ENGLISH.txt
```

The mod also installs a copy for each game language (`FRENCH`, `GERMAN`,
`ITALIAN`, `SPANISH`, `POLISH`, `RUSSIAN`, `JAPANESE`, `CHINESE`). They all start
in English. The game reads the one that matches the language in your
`Skyrim.ini` (`sLanguage`).

Playing in a language Skyrim doesn't officially have, like Brazilian
Portuguese? Most people in that case play with `sLanguage=ENGLISH`, so just
translate the `ENGLISH` file.

## How a line looks

Each line has a key, then a TAB, then the text:

```
$AOS_Craft equipment	Craft equipment
$AOS_{} improved to {}.	{} improved to {}.
```

The rules are simple:

- **Don't touch anything before the TAB.** That's the key the mod looks for.
- **Translate only what comes after the TAB.**
- **Keep every `{}`.** Each one becomes a number or a name in the game, in the
  same order as in English.
- **One line per text.** No line breaks inside a text.
- Lines starting with `$AOS_MCM_` are the MCM. The rest is the menu, messages
  and letters.

Skipped a line? No problem, it just shows in English. A half-done translation
never breaks anything.

## Step by step

1. Open `ArtisansOfSkyrim_ENGLISH.txt` in Notepad++.
2. Save a copy with your language in the name, for example
   `ArtisansOfSkyrim_GERMAN.txt` (or keep `ENGLISH`, as explained above).
3. Translate the text after the TAB, line by line.
4. Save it as **UTF-16 LE BOM** (Notepad++: *Encoding > UTF-16 LE BOM*).
   This one matters: with any other encoding the game ignores the whole file.
5. Put the file in a mod of its own, at
   `Interface/Translations/ArtisansOfSkyrim_<LANGUAGE>.txt`, and let it win
   over Artisans of Skyrim in your mod manager.
6. Jump in game, open a service and the MCM, and check that everything fits.

Nothing changed in game? Check the file name and the encoding first. That's
almost always it.

## Good to know

- Item, NPC and place names come from the game itself, so they're already in
  your language and aren't in this file. The same goes for vanilla effect
  descriptions.
- The installer and the log file are not translated.
- When I update the mod and add new texts, they show up in English until the
  translation is updated. Compare your file with the new English one to find
  them.
- Skyrim stores translated words without caring about upper/lower case. If a
  short word ever shows up in ALL CAPS, another mod's file already has that
  word in capitals. The menu fixes the common cases, and picking a slightly
  different word fixes the rest.

Made a translation? Let me know on the mod page, I'd be happy to link it!
