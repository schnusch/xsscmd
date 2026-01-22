# xsscmd – X11 Screen Saver extension command

Run shell commands based on X11 Screen Saver extension's idle time.

## Usage

```
Usage: xsscmd [-t <timeout>] <idle_cmd> [<wake_cmd>]
```

## Installation

To add this repo to your Debian 13 or Ubuntu 24.04 system create
`/etc/apt/sources.list.d/xsscmd.sources` with the following contents (replace
`$VERSION_CODENAME` according to your distribution):

```
Types: deb
URIs: https://schnusch.github.io/xsscmd/$VERSION_CODENAME/
Suites: $VERSION_CODENAME
Components: main
Trusted: yes
```
