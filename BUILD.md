# Build and install

Use the official [ounce-devkit](https://github.com/ouncebrew/ounce-devkit),
set `DEVKITPRO`, and run `make`. The output is `kai-menu-music.nso`.
The tested toolkit uses libounce and libkaihen packages `20261117.1-1`.

Install the NSO at `/kaihen/plugins/kai-menu-music.nso` on an already working
kaiHEN SD card, replacing the previous plugin. Restart kaiHEN after replacing it.
Do not keep multiple copies of the NSO under different names in plugins.

Supply one WAV in `/switch/kai-menu-music/`. Any visible filename ending in
`.wav` (case-insensitive) is accepted. If multiple WAVs exist, the first by
bytewise filename order is selected; subdirectories and hidden files are ignored.
Use RIFF/WAVE uncompressed PCM signed 16-bit stereo, 48000 Hz, up to 96 MiB.
Example conversion on a computer:

```sh
ffmpeg -i your-song.mp3 -ar 48000 -ac 2 -c:a pcm_s16le yourfile.wav
```

The complete WAV loads into RAM, then its SD handles close. HOME activity
controls playback. Volume is fixed at half amplitude. There is no overlay,
settings file, network access, or plugin-owned persistent write. The code
opens only the SD filesystem, directory and music file for reading, and
queries firmware, audio and PDM services. This describes the plugin's requests;
Nintendo system services and kaiHEN have their own behavior.

The original fixed-path audio-only version was reported working on a user's
Switch 2. This release restores that implementation and adds filename discovery;
the filename discovery change is cross-compiled and host-tested, but awaits a
console test. The failed experimental overlay is not included.

Host checks:

```sh
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  source/core.c tests/test_core.c -o /tmp/kai-menu-core-test
/tmp/kai-menu-core-test
```

Project source is MIT licensed. The Makefile is adapted from ounce-devkit's
MIT plugin example. libkaihen and libounce/libnx use ISC licenses; notices are
in `licenses/`. No music, firmware, Nintendo SDK or other console assets are
included in the repository or release.
