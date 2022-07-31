
VDR-convert Version 2.3
=======================
vdr-convert is a set of tools to accurately transcode VDR1.x and VDR2.x TV recordings, including all valid streams - video, audio (including AC3/DTS 5.1), Audio Description (AD), and DVB subtitles - into a more compressed and accessible format, while maintaining perceived quality with good compatibility. H264 and AAC are the default codecs for the main streams, but H265 is available from V2 onwards

The user can leave the transcoded files in-place for use by VDR or use them with external players such as Kodi, MPC-HC, or VLC.

Refer to the WIKI here for full usage instructions
https://github.com/vdr-projects/vdr-convert/wiki

A new podcast mode was addd in V2.2:
https://github.com/vdr-projects/vdr-convert/wiki/Podcast-mode


Software required
=================
If keeping recordings for use inside VDR, you need VDR 2.2x installed on the conversion machine, otherwise not.

All recordings: System running Linux/bash shell, FFMPEG 3.x, core Linux utilities such as nice, timeout etc
Optionally NCFTP if you want to upload files after conversion

Additionally for VDR 1.x recordings:

a) Modified GENINDEX (0.2) to convert subtitles to standard ETSI EN300743 format, and remove initial short segment sequences that currently prevent ffmpeg reliably detecting subtitle streams, and remove substream headers from AC3/DTS streams for ffmpeg compatibility.

b) MPLEX13818 from http://www.scara.com/~schirmer/o/mplex13818 to reliably convert recordings into an mpegts container. ffmpeg doesn't handle dvbsubs in a program stream (.vdr native format), and sometimes fails to probe .vdr files correctly. In testing, this muxer produced much more reliable .ts files from VDR recordings than the myriad of versions of "ps2ts" etc. out there in the web.

Configuring
===========
vdr-convert is a standalone bash script - no spcific installation
It should be placed in the system path, somewhere like /usr/local/bin
Make sure it's executable!

There are a few parameters at the top of vdr-convert to configure:

"ffmpeg" - set to the path of the version of ffmpeg you plan to use.
 (See "patching" below)

"LOGFILE" - a place where detailed ffmpeg logs are kept, e.g. /var/log.
 Make sure it's writable by whatever user you run vdr-convert under (e.g. vdr)

"Log_facility" - identify where you want the script's syslog messages to be logged (default local2)

set default langauages if you are missing VDR "info" files.

"filesystem", default is 190. This is the number of characters that the filesystem can handle, plus ~60 for date, time, epiosde number. Most filesystems limit the total to 255.

"email" address if you want to be emailed about significant conversion failures (those tagged "error")

Default conversion parameters
=============================
Command line options may modify several of these, see
https://github.com/vdr-projects/vdr-convert/wiki/Options

Note The H264/265 presets are chosen to be optimal at the time of writing based on testing and web reviews
Optimal H265 presets have changed as H265 develops

"qualityx265" is an adder to the familiar x264 CRF values to align them with those used in x265.
So you use the familiar x264 CRF's with the -q option to acheive approx the same quality with x265

"vcodec" 264 or 265 depending on your preference, and what your ffmpeg is built with.
See https://github.com/vdr-projects/vdr-convert/wiki/Howto#h265-hevc

"acodec" set to libfdk_aac if you built and linked your ffmpeg with the non-free Fraunhofer AAC library

"ext" is the default output file format/extension.
ts is always used for kept files, others are supported for single-use

"podcastaudioprofile" is the codec config string for audio podcasts when the --podcast option is used
More description in the script itself - there are usable strings for default HE-AAC v2, MP3 and Opus

"podcastcmd" defines podcast post processing commands, the default is for a local Squuezeserver (LMS)

See https://github.com/vdr-projects/vdr-convert/wiki/Options

----
In batch.sh (if you use it)
- configure with the root of your VDR recording directory so that it can trigger VDR directory re-reads


Building genindex
=================
Required to convert VDR1.x recordings.
Copy the genindex source supplied in this project, and then type "make"
Copy the executable genindex to somewhere in the system path - e.g. /usr/local/bin
Make sure there are no other older copies or versions of genindex on the system!

Building MPLEX13818
===================
Required to convert VDR1.x recordings.
Obtain a copy from the URL above
Build with the makefile supplied
Several executables are built, but vdr-convert only needs "iso13818ts"
Copy the executable iso13818ts to somewhere in the system path - e.g. /usr/local/bin

Patching
========
FFMPEG
------
You should consider patching ffmpeg if you have "broken" recordings - e.g where there was reception interference during recording. Stock ffmpeg is intolerant of subtitles streams with any DTS timestamp errors.  Some broadcasters regularly transmit programmes with DTS problems, you may therefore need to patch.

To do this:
Get an up-to-date copy of ffmpeg, e.g.
* git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg or see https://ffmpeg.org/download.html
* build it using ffmpeg instructions
* add the file FFMPEG-libavformat-reduce-exits-on-subs-error.patch to the libavformat subdirectory
* patch -p1<FFMPEG-libavformat-reduce-exits-on-subs-error.patch
* Rebuild (quick)

VDR
----
VDR benefits from a couple of minor patches too:

a) to suppress warnings when reindexing, and, at the time of writing

b) to allow re-indexing of audio-only recordings.

The patches are in the VDR patches subdirectory
Copy them to the VDR source directory, and patch as ffmpeg above
Rebuild VDR as usual.

Other utilities
===============
You are more than likely wanting to convert a whole selection in a recordings together.
A small script called "batch.sh" is provided to assist.
The wiki provides details of how to use this, make sure it is copied somewhere in the path, and that it's executable

If you want to run vdr-convert automatically after each recording, the supplied script "vdr-auto" can be used. The wiki describes it's use. The version supplied is what the author uses daily, and includes a call to the venerable "noad" utility as well, to mark commercial breaks.

Noad completes very much more quickly than vdr-convert because the ffmpeg libraries are doing much less work than libx264.  It will therefore complete long before the recording file is replaced by vdr-convert, avoiding conflicts.  noad or similar utilities can of course be run on the converted files too, and in test noad produces the same output (within a second). For this reason vdr-convert does not re-run noad on converted files, just copies any marks.vdr file over (if required for VDR1.x recordings).

RF V1 August 2016
V1 Updated Dec 2016
V2 Updated March 2017
V2.2 Updated Sept 2018
