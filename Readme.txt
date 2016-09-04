 
VDR-convert
===========
vdr-convert is a shell script with associated tools to transcode the content of VDR1.x and VDR2.x recordings as accurately as possible, including all streams, audio, AD, subtitles, and metadata into more compressed format, maintaining perceived quality.  H264 and AAC are the chosen codecs for the main streams. SD recordings are reduced to anywhere from 35% - 90% of original size depending on content and compression settings.  On average, you can expect to save 1/3 of the disk space used for SD recordings.

The user can leave the transcoded files in-place for use by VDR or use them with external players such as Kodi, MPC-HC, or VLC

Refer to the WIKI here for full usage instructions
https://projects.vdr-developer.org/projects/vdr-convert/wiki

Software required
=================

All recordings: System running Linux/bash shell, FFMPEG 3.x, VDR 2.2x, core Linux utilities such as nice, timeout etc

Additionally for VDR 1.x recordings:

a) Modified GENINDEX (0.2) to convert subtitles to standard ETSI EN300743 format, and remove initial short segment sequences that currently prevent ffmpeg reliably detecting subtitle streams

b) MPLEX13818 from http://www.scara.com/~schirmer/o/mplex13818 to reliably convert recordings into an mpegts container. ffmpeg doesn't handle dvbsubs in a program stream (.vdr native format), and sometimes fails to probe .vdr files correctly. In testing, this muxer produced much more reliable .ts files from VDR recordings than the myriad of versions of "ps2ts" etc. out there in the web.

Optionally NCFTP if you want to upload files after conversion

Configuring
===========
vdr-convert is a standalone bash script - no spcific installation
It should be placed in the system path, somewhere like /usr/local/bin
Make sure it's executable!

There are a few parameters at the top of vdr-convert to configure:

"ffmpeg" - set to the path of the version of ffmpeg you plan to use. 
 (See "patching" below) Don't alter the ffmpeg parameters on the end of the line!

"LOGFILE" - a place where system logs are kept, e.g. /var/log. 
 Make sure it's writable by whatever user you run vdr-convert under (e.g. vdr)

"aac" set to libfdk_aac if you built and linked your ffmpeg with the non-free Fraunhofer AAC library 

set default langauages if you are missing VDR "info" files.

batch.sh if you use it 
- configure with the root of your VDR recording directory so that it can trigger VDR re-reads


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
You may consider patching ffmpeg if you have "broken" recordings - e.g where there was reception interference during recording. Stock ffmpeg is intolerant of subtitles streams with any DTS timestamp errors.

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
The wiki provides details of how to use this, make sure it is copied somehwere in the path, and that it's executable

If you want to run vdr-convert automatically after each recording, the supplied script "vdr-auto" can be used. The wiki describes it's use. The version supplied is what the author uses daily, and includes a call to the venerable "noad" utility as well, to mark commercial breaks.

Noad completes very much more quickly than vdr-convert because the ffmpeg libraries are doing much less work than libx264.  It will therefore complete long before the recording file is replaced by vdr-convert, avoiding conflicts.  
noad or similar utilities can of course be run on the converted files too, and in test noad produces the same output (within a second). For this reason vdr-convert does not re-run noad on converted files, just copies any marks.vdr file over (if required for VDR1.x recordings).

RF August 2016
