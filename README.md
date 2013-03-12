conrad
======

Console app to play internet radio streams.

## Contents  

1. Introduction  
2. Building conrad  
3. Running conrad  
4. Release Notes  

## 1. Introduction  

Conrad is a console app to play shoutcast internet radio streams.

**Goals**  
- Minimalistic console-UI.
- Glitch-free playback.
- Low memory footprint.
- OS/Distro agnostic.


## 2. Building conrad  

**Dependencies**  

- libfmodex (bundled)

    FmodEx libraries are part of FMOD Sound System.  
    Copyright Firelight Technologies.  
    www.fmod.org

- libcurl

    Fetching and installing libcurl:  
    On Ubuntu 11.10, the following command does the job  
    `sudo apt-get install libcurl4-gnutls-dev`


To build conrad, execute `make` in the root of the repository. Ensure that the aforementioned dependencies are satisfied. Checkout `Makefile` for more details.


## 3. Running conrad  

`./conrad -s <station-url>`

conrad saves the encoded audio stream of the current session in a local file "wave.dat" in the current directory.

## 4. Release Notes  

- **v0.1** (Initial Commit)




