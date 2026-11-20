# Build and install

Build with [ounce-devkit](https://github.com/ouncebrew/ounce-devkit), libounce and libkaihen `20261117.1-1`:

```sh
make
```

Install:

| File | SD path |
| --- | --- |
| `kai-menu-music.nso` | `/kaihen/plugins/kai-menu-music.nso` |
| `kai-menu-music-selector.nro` | `/switch/kai-menu-music-selector/kai-menu-music-selector.nro` |

Replace the existing plugin, restart kaiHEN, and open **Menu Music** from the homebrew menu.

| Control | Action |
| --- | --- |
| Up/Down | Browse |
| A | Select a song and turn shuffle off |
| Y | Toggle shuffle |
| X | Refresh |
| B / + | Exit |

Return to HOME to listen. Selection and shuffle settings are saved on SD in `/switch/kai-menu-music/selected.txt` and `/switch/kai-menu-music/shuffle.txt`. Shuffle skips unsupported files and avoids immediate repeats when another valid song exists.

Put WAV files in `/switch/kai-menu-music/`. Format: RIFF/WAVE, PCM signed 16-bit, stereo, 48000 Hz, up to 96 MiB.

```sh
ffmpeg -i your-song.mp3 -ar 48000 -ac 2 -c:a pcm_s16le yourfile.wav
```

The plugin loads one track into RAM and plays on HOME. The selector can request a live reload. Without a saved choice, the first visible WAV in filename order is used. Volume is fixed at half amplitude. No NAND writes, network access, or overlay.

Host tests:

```sh
clang -std=gnu11 -Wall -Wextra -Werror -fsanitize=address,undefined source/core.c tests/test_core.c -o /tmp/menu-core-test
/tmp/menu-core-test
clang -std=gnu11 -Wall -Wextra -Werror -fsanitize=address,undefined -I tests/mock source/core.c source/songs.c tests/test_songs.c -o /tmp/menu-songs-test
/tmp/menu-songs-test
```

Source is MIT licensed. Dependency notices are in `licenses/`. Music and console assets are not included.
