# ![logo](https://raw.githubusercontent.com/azerothcore/azerothcore.github.io/master/images/logo-github.png) AzerothCore

## mod-mounts-on-account

- Latest build status with azerothcore:

[![Build Status](https://github.com/pangolp/mod-mounts-on-account/workflows/core-build/badge.svg?branch=master&event=push)](https://github.com/pangolp/mod-mounts-on-account)

## Description

The purpose of this module is to store in a table, in the auth database, the spells of the mounts that an account obtains, either by purchasing them or by obtaining them in-game through loot. Once obtained, the idea is to look for different alternatives, for example, gold, items, missions, achievements, that allow the rest of the players to learn all or the mounts they want.

I'm still analyzing whether to do it by command, an npc with a gossip or when a player of the account starts a session. However, the method also depends on the server. It is not the same a server with few players, that one that has many, so I think that the session start is discarded. Possibly it will be, by means of an npc and a gossip or, a command, but that has a cooldown, so that it cannot be used all the time.

The module is not finished yet, we are not working on it, so if you want to collaborate, you can make a pull request, explaining the changes you want to make in it, so I can review it and merge it. You can also open an issue, although at the moment, not very useful, since it is not finished.
