qBittorrent Enhanced Edition
------------------------------------------
# Note
### To user:

Please do not use this modification bittorrent client for Private Trackers, you will get banned. 

### To tracker operator:

qBittorrent Enhanced is based on qBittorrent, it's aimed at block a leeching clients automatically.

Also, qBittorrent Enhanced have a different ID announce to trackers.

User agent: `qBittorrent Enhanced/LATEST_RELEASE_VERSION`, example: `qBittorrent Enhanced/4.2.1.10`

PeerID: `-qB421[A-Z]-`, example: `-qB421A-`
********************************
# Features: 
1. Auto Ban Xunlei, QQ, Baidu, Xfplay, DLBT and Offline downloader

2. [Temporary IP Filter API for advanced user](https://github.com/c0re100/qBittorrent-EE-API)

3. Update MessageBox with changelog if NEW version is available

4. _Auto Ban Unknown Peer from China_ Option(Default: OFF)

5. Show Tracker Authentication Window(Default: ON)

6. Auto Update Public Trackers List(Default: OFF)**

7. Multiple qBittorrent instances
********************************
### Description:
qBittorrent is a bittorrent client programmed in C++ / Qt that uses
libtorrent (sometimes called libtorrent-rasterbar) by Arvid Norberg.

It aims to be a good alternative to all other bittorrent clients
out there. qBittorrent is fast, stable and provides unicode
support as well as many features.

This product includes GeoLite data created by MaxMind, available from
https://www.maxmind.com/

### Installation:
For installation, follow the instructions from INSTALL file, but simple:

```
./configure
make && make install
qbittorrent
```

will install and execute qBittorrent hopefully without any problem.

## Repository

If you are using a desktop Linux distribution without any special demands, you can use AppImage from release page.

Latest AppImage download: [qBittorrent-Enhanced-Edition.AppImage](https://github.com/c0re100/qBittorrent-Enhanced-Edition/releases/latest/download/qBittorrent-Enhanced-Edition.AppImage)

### Arch Linux (Maintainer: [c0re100](https://github.com/c0re100))

[AUR](https://aur.archlinux.org/packages/qbittorrent-enhanced-git/)

[nox AUR](https://aur.archlinux.org/packages/qbittorrent-enhanced-nox-git/)

### Debian (Maintainer: [yangfl](https://github.com/yangfl))

[repo](https://repo.debiancn.org/pool/main/q/qbittorrent-enhanced/)

### openSUSE/RPM-based Linux distro (Maintainer: [PhoenixEmik](https://github.com/PhoenixEmik))

[openSUSE repo](https://build.opensuse.org/package/show/home:PhoenixEmik/qbittorrent-enhanced-edition)

### Ubuntu (Maintainer: [poplite](https://github.com/poplite))

[PPA](https://launchpad.net/~poplite/+archive/ubuntu/qbittorrent-enhanced)

### macOS (Homebrew) (Maintainer: [AlexaraWu](https://github.com/AlexaraWu))
```
brew cask install c0re100-qbittorrent
```

### Windows (Chocolatey) (Maintainer: [iYato](https://github.com/iYato))

```
choco install qbittorrent-enhanced
```

### Misc:
For more information please visit:
https://www.qbittorrent.org

or our wiki here:
http://wiki.qbittorrent.org

Use the forum for troubleshooting before reporting bugs:
http://forum.qbittorrent.org

Please report any bug (or feature request) to:
http://bugs.qbittorrent.org

For extra features bug(such as Auto Ban, API, Auto Update Tracker lists...), please report to: 
https://github.com/c0re100/qBittorrent-Enhanced-Edition/issues

