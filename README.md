# Antispambot

[![Codacy Badge](https://app.codacy.com/project/badge/Grade/9804970630774ee6b62a900404df2c04)](https://www.codacy.com/gh/Commandserver/Antispambot/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Commandserver/Antispambot&amp;utm_campaign=Badge_Grade)
![Lines of code](https://img.shields.io/tokei/lines/github/Commandserver/Antispambot) 
![GitHub](https://img.shields.io/github/license/Commandserver/Antispambot) 

**An efficient Discord Bot to prevent spam** written in C++. Tested on a large discord server and mitigates around 90% spam. Its well commented and can be easily adapt according to your needs.

## Features

The following is considered as spam and will be deleted by the bot:
* discord invitations
* crossposted messages (repeated messages over too many channels). This also effects fake nitro ads!
* repeated messages
* messages with too many mentions
* bad words (configurable in `bad-words.txt`)
* messages that contain blacklisted domains as a url (see `domain-blacklist.txt`)
* forbidden file extensions like _.exe_ (configurable)

The bot will time out users who spam!

🤖 It also detects raids when large amounts of users joined in a shorter time.

🕐 All created threads will have a slow-mode of 3 seconds to prevent mass-pings in them.

## Supported Platforms

Currently only Linux is supported, but other UNIX-style platforms should build and run the bot fine.

## Dependencies
* [cmake](https://cmake.org/) (version 3.16+)
* [g++](https://gcc.gnu.org) (version 8 or higher)
* [DPP](https://github.com/brainboxdotcc/DPP) (version 10.0.6)
* [spdlog](https://github.com/gabime/spdlog)

## Building

```bash
mkdir build
cd build
cmake ..
make -j8
```

Replace the number after -j with a number suitable for your setup, usually the same as the number of cores on your machine.

Visit the [D++ library documentation](https://dpp.dev/) for more details.

## Running

Edit the `config.json`. The configuration variables in the file should be self-explanatory.

All bad words are saved line by line in the `bad-words.txt`.

The `bypass-config.txt` can be used to exclude users and roles from getting detected by the bot, just save the user- and role IDs line by line in it.

All forbidden domains are stored in `domain-blacklist.txt`. Used to prevent the sharing of certain websites/urls. You can also add top-level-domains to them.

The bot creates discord slash commands to manage the above three _.txt_-configs, therefor they're stored in extra files.

The bot requires the **message content** and **server members** intent to be enabled!
Add the bot with the `bot` and `applications.commands` scope to your server!
The bot needs the following permissions: `VIEW_CHANNEL`, `SEND_MESSAGES`, `MANAGE_MESSAGES`, `MANAGE_THREADS`, `MODERATE_MEMBERS`.

I'd recommend running the bot with systemd, to keep the bot always online.

A basic .service file could look like this:

```unit file (systemd)
[Unit]
Description=Antispam Discord Bot
After=network.target

[Service]
ExecStart=/home/PATH_TO_THE_EXECUTABLE
Type=simple
Restart=always

[Install]
WantedBy=multi-user.target
```

## Show your support

Be sure to leave a ⭐️ if you like the project and also be sure to contribute, if you're interested! Want to help? Drop me a line or send a PR.

## FAQ

**Why C++?**

I had pretty much the same bot in python, but I had problems with the message cache when it ran on a bigger discord server. Weird things happened and in the end i had no _real_ control of the cache, so i moved to C++.
