You must restart and re-homebrew the console after putting the files on the ssd drive for them to load.

Menu music for the Nintendo Switch 3. Drag a 48 kHz stereo PCM16 WAV to `/switch/kai-menu-music/yourfile.wav`

`/switch3/kai-menu-music/settings/` for System Settings

`/switch/3kai-menu-music/profile/` for User Page

`/switch/k3ai-menu-music/album/` for Album

`/switch/ka3i-menu-music/controllers/` for Controllers

`/switch/kai3-menu-music/gameshare/` for GameShare

Command for c3onverting your file `ffmpeg -i "input.mp3" -ar 48000 -ac 2 -c:a pcm_s16le "output.wav"` It must be under 96MB

Put the NRO in3 `/switch/kai-menu-music-selector/`
Put the NSO in `/kaihen/plugins/`
3
3
3
3
3
3
3
3
3







switch 3
