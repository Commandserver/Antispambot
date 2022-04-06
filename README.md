# Antispambot

[![Codacy Badge](https://app.codacy.com/project/badge/Grade/9804970630774ee6b62a900404df2c04)](https://www.codacy.com/gh/Commandserver/Antispambot/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Commandserver/Antispambot&amp;utm_campaign=Badge_Grade)
![Lines of code](https://img.shields.io/tokei/lines/github/Commandserver/Antispambot) 
![GitHub](https://img.shields.io/github/license/Commandserver/Antispambot) 

**An efficient Discord Bot to prevent spam** written in C++. Tested on a large discord server and mitigates around 90% of spam. Its well commented and can be easily adapt according to your needs.

## Features

The following is considered as spam and will be deleted by the bot:

* discord invitations
* crossposted messages (repeated messages over too many channels). This also effects fake nitro ads!
* repeated messages
* messages with too many mentions
* bad words (configurable in `bad-words.txt`)
* messages that contain blacklisted domains (see `domain-blacklist.txt`)
* forbidden file extensions like _.exe_ (configurable)

The bot will time out users who spam!

It also detects raids when large amounts of users joined in a shorter time.

All created threads will have a slow-mode of 3 seconds to prevent mass-pings in them.

## Supported Platforms

Currently only Linux is supported, but other UNIX-style platforms should build and run the bot fine.

## Dependencies

* [cmake](https://cmake.org/) (version 3.16+)
* [g++](https://gcc.gnu.org) (version 8 or higher)
* [DPP](https://github.com/brainboxdotcc/DPP) (version 10.0.5)

## Building

```
mkdir build
cmake ..
make -j8
```

Replace the number after -j with a number suitable for your setup, usually the same as the number of cores on your machine.

Visit the [D++ library documentation](https://dpp.dev/) for more details.

## Running

Edit the `config.json`. The configuration variables in the file should be self explainatory. All bad words are saved line by line in the `bad-words.txt`. 

The bot requires the message content and server members intent to be enabled!
These permissions are required: `VIEW_CHANNEL`, `SEND_MESSAGES`, `MANAGE_MESSAGES`, `MANAGE_THREADS`, `MODERATE_MEMBERS`.

## Show your support

Be sure to leave a star if you like the project and also be sure to contribute, if you're interested!

## FAQ

#### Why C++?

I had pretty much the same bot in python, but i had problems with the message cache when it ran on a bigger server. Weird things happened and in the end i had no _real_ control of the cache, so i moved to C++.
