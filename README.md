Sync your music playlists on Youtube and Spotify

Architecture discussion
-----------------------
Tracked playlists
	update least recently updated (yt or spotify version of playlist) to be equal to the more recently updated one
	both deletion and additions
	efficient way to track changes like git 
	how often to sync? when user calls it? run sync operation in the background periodically?
What do do when user wants to sync two existing playlists?
How to let user easily track/untrack playlists?
Should there be a website interface or just command line?
How to deal with songs only available on one of the two platforms?
How to deal with single video playlists on youtube?

Prototype 1
Display playlists across yt and spotify and let user track some
Can't sync two existing playlists at this stage, only one way

UI discussion
-------------
plsync (playlist sync)
plsync init -> does spotify and youtube oauth flows. must be first command and must be successful to continue
plsync untracked
plsync track ... [...]
FOR NOW, I'M NOT GOING TO USE A CLI PARSER LIB

TODO
----
