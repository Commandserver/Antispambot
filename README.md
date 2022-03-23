# Antispambot

[![Codacy Badge](https://app.codacy.com/project/badge/Grade/9804970630774ee6b62a900404df2c04)](https://www.codacy.com/gh/Commandserver/Antispambot/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Commandserver/Antispambot&amp;utm_campaign=Badge_Grade)
![Lines of code](https://img.shields.io/tokei/lines/github/Commandserver/Antispambot) 
![GitHub](https://img.shields.io/github/license/Commandserver/Antispambot) 

**An efficient Discord Bot to prevent spam** written in C++. Tested on a large discord server and mitigates around 90% of spam. Its well commented and can be easily adapt according to your needs.

## Features

The following is considered as spam and will be deleted by the bot:

* discord invitations
* repeated messages over too many channels. This also effects fake nitro ads!
* multiple repeated messages
* messages with too many mentions
* bad words (configurable in `bad-words.txt`)
* messages that contain blacklisted domains (see `domain-blacklist.txt`)
* forbidden file extensions like _.exe_ (configurable)

The bot will time out users who spam!

It also detects raids when large amounts of users joined in a shorter time.

All created threads will have a slow-mode of 3 seconds to prevent mass-pings in them.

## Build

#### [View D++ library documentation](https://dpp.dev/)

### Dependencies

* [cmake](https://cmake.org/) (version 3.13+)

### Included Dependencies

* [DPP](https://github.com/brainboxdotcc/DPP) (version 10.0.4)

### Set up this bot

The bot requires the message content and server members intent to be enabled!
Some permissions are also required in order to work: `VIEW_CHANNEL`, `SEND_MESSAGES`, `MANAGE_MESSAGES`, `MANAGE_THREADS`, `MODERATE_MEMBERS`.
