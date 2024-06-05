# Star Ocean - The Last Hope Fix
[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/W7W01UAI9)</br>
[![Github All Releases](https://img.shields.io/github/downloads/Lyall/SO4Fix/total.svg)](https://github.com/Lyall/SO4Fix/releases)

This is a fix that adds custom resolutions, ultrawide support and more to STAR OCEAN - THE LAST HOPE - 4K & Full HD Remaster.

## Features
- Custom resolution support.
- Borderless fullscreen.
- Ultrawide/narrow aspect ratio support.
- Correct FOV at any aspect ratio.
- Fixes many HUD issues at non-16:9 aspect ratios.
- Skip intro logos.
- Fixes bug with 4x shadow buffer option.

## Installation
- Grab the latest release of SO4Fix from [here.](https://github.com/Lyall/SO4Fix/releases)
- Extract the contents of the release zip in to the the game folder. e.g. "**steamapps\common\STAR OCEAN - THE LAST HOPE - 4K & Full HD Remaster**" for Steam).

### Steam Deck/Linux Additional Instructions
🚩**You do not need to do this if you are using Windows!**
- Open up the game properties in Steam and add `WINEDLLOVERRIDES="dinput8=n,b" %command%` to the launch options.

## Configuration
- See **SO4Fix.ini** to adjust settings for the fix.

## Known Issues
Please report any issues you see.
This list will contain bugs which may or may not be fixed.

## Screenshots

|  |
|:--:|
| Gameplay |

## Credits
[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) for ASI loading. <br />
[inipp](https://github.com/mcmtroffaes/inipp) for ini reading. <br />
[spdlog](https://github.com/gabime/spdlog) for logging. <br />
[safetyhook](https://github.com/cursey/safetyhook) for hooking.
