You must restart and re-homebrew the console after putting the files on the sdcard for them to load.

Menu music for the Nintendo Switch 2. Drag a 48 kHz stereo PCM16 WAV to `/switch/kai-menu-music/yourfile.wav`
`/switch/kai-menu-music/settings/` for System Settings
`/switch/kai-menu-music/profile/` for User Page
`/switch/kai-menu-music/album/` for Album
`/switch/kai-menu-music/controllers/` for Controllers
`/switch/kai-menu-music/gameshare/` for GameShare

Command for converting your file `ffmpeg -i "input.mp3" -ar 48000 -ac 2 -c:a pcm_s16le "output.wav"` It must be under 96MB

Put the NRO in `/switch/kai-menu-music-selector/`
Put the NSO in `/kaihen/plugins/`

