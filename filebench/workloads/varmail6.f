#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

set $dir=/ssd-data
set $nfiles=1000
set $meandirwidth=20
set $filesize=16384#cvar(type=cvar-gamma,parameters=mean:16384;gamma:1.5)
set $nthreads=1
set $iosize=0
set $meanappendsize=16k

#define fileset name=CFS9,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80
#define fileset name=CFS8,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80
#define fileset name=CFS7,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80
define fileset name=CFS6,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80
define fileset name=CFS5,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80
define fileset name=CFS4,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80
define fileset name=CFS3,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80
define fileset name=CFS2,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80
define fileset name=CFS1,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80

define process name=filereader1,instances=1
{
  thread name=filereaderthread1,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile1,filesetname=CFS1
    flowop createfile name=createfile2,filesetname=CFS1,fd=1
    #flowop openfile name=openfile2,filesetname=CFS1,fd=1
    flowop appendfilerand name=appendfilerand2,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile2,fd=1
    flowop closefile name=closefile2,fd=1
    flowop openfile name=openfile3,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile3,fd=1,iosize=$iosize
    flowop appendfilerand name=appendfilerand3,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile3,fd=1
    flowop closefile name=closefile3,fd=1
    flowop openfile name=openfile4,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile4,fd=1,iosize=$iosize
    flowop closefile name=closefile4,fd=1
  }
}

define process name=filereader2,instances=1
{
  thread name=filereaderthread2,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile11,filesetname=CFS2
    flowop createfile name=createfile12,filesetname=CFS2,fd=1
    #flowop openfile name=openfile12,filesetname=CFS2,fd=1
    flowop appendfilerand name=appendfilerand12,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile12,fd=1
    flowop closefile name=closefile12,fd=1
    flowop openfile name=openfile13,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile13,fd=1,iosize=$iosize
    flowop appendfilerand name=appendfilerand13,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile13,fd=1
    flowop closefile name=closefile13,fd=1
    flowop openfile name=openfile14,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile14,fd=1,iosize=$iosize
    flowop closefile name=closefile14,fd=1
  }
}

define process name=filereader3,instances=1
{
  thread name=filereaderthread3,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile21,filesetname=CFS3
    flowop createfile name=createfile22,filesetname=CFS3,fd=1
    #flowop openfile name=openfile22,filesetname=CFS3,fd=1
    flowop appendfilerand name=appendfilerand22,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile22,fd=1
    flowop closefile name=closefile22,fd=1
    flowop openfile name=openfile23,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile23,fd=1,iosize=$iosize
    flowop appendfilerand name=appendfilerand23,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile23,fd=1
    flowop closefile name=closefile23,fd=1
    flowop openfile name=openfile24,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile24,fd=1,iosize=$iosize
    flowop closefile name=closefile24,fd=1
  }
}

define process name=filereader4,instances=1
{
  thread name=filereaderthread4,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile31,filesetname=CFS4
    flowop createfile name=createfile32,filesetname=CFS4,fd=1
    #flowop openfile name=openfile32,filesetname=CFS4,fd=1
    flowop appendfilerand name=appendfilerand32,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile32,fd=1
    flowop closefile name=closefile32,fd=1
    flowop openfile name=openfile33,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile33,fd=1,iosize=$iosize
    flowop appendfilerand name=appendfilerand33,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile33,fd=1
    flowop closefile name=closefile33,fd=1
    flowop openfile name=openfile34,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile34,fd=1,iosize=$iosize
    flowop closefile name=closefile34,fd=1
  }
}

define process name=filereader5,instances=1
{
  thread name=filereaderthread5,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile41,filesetname=CFS5
    flowop createfile name=createfile42,filesetname=CFS5,fd=1
    #flowop openfile name=openfile42,filesetname=CFS5,fd=1
    flowop appendfilerand name=appendfilerand42,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile42,fd=1
    flowop closefile name=closefile42,fd=1
    flowop openfile name=openfile43,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile43,fd=1,iosize=$iosize
    flowop appendfilerand name=appendfilerand43,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile43,fd=1
    flowop closefile name=closefile43,fd=1
    flowop openfile name=openfile44,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile44,fd=1,iosize=$iosize
    flowop closefile name=closefile44,fd=1
  }
}

define process name=filereader6,instances=1
{
  thread name=filereaderthread6,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile51,filesetname=CFS6
    flowop createfile name=createfile52,filesetname=CFS6,fd=1
    #flowop openfile name=openfile52,filesetname=CFS6,fd=1
    flowop appendfilerand name=appendfilerand52,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile52,fd=1
    flowop closefile name=closefile52,fd=1
    flowop openfile name=openfile53,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile53,fd=1,iosize=$iosize
    flowop appendfilerand name=appendfilerand53,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile53,fd=1
    flowop closefile name=closefile53,fd=1
    flowop openfile name=openfile54,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile54,fd=1,iosize=$iosize
    flowop closefile name=closefile54,fd=1
  }
}

define process name=filereader7,instances=0
{
  thread name=filereaderthread7,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile61,filesetname=CFS7
    flowop createfile name=createfile62,filesetname=CFS7,fd=1
    #flowop openfile name=openfile62,filesetname=CFS7,fd=1
    flowop appendfilerand name=appendfilerand62,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile62,fd=1
    flowop closefile name=closefile62,fd=1
    flowop openfile name=openfile63,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile63,fd=1,iosize=$iosize
    flowop appendfilerand name=appendfilerand63,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile63,fd=1
    flowop closefile name=closefile63,fd=1
    flowop openfile name=openfile64,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile64,fd=1,iosize=$iosize
    flowop closefile name=closefile64,fd=1
  }
}

define process name=filereader8,instances=0
{
  thread name=filereaderthread8,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile71,filesetname=CFS8
    flowop createfile name=createfile72,filesetname=CFS8,fd=1
    #flowop openfile name=openfile72,filesetname=CFS8,fd=1
    flowop appendfilerand name=appendfilerand72,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile72,fd=1
    flowop closefile name=closefile72,fd=1
    flowop openfile name=openfile73,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile73,fd=1,iosize=$iosize
    flowop appendfilerand name=appendfilerand73,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile73,fd=1
    flowop closefile name=closefile73,fd=1
    flowop openfile name=openfile74,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile74,fd=1,iosize=$iosize
    flowop closefile name=closefile74,fd=1
  }
}

define process name=filereader9,instances=0
{
  thread name=filereaderthread9,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile81,filesetname=CFS9
    flowop createfile name=createfile82,filesetname=CFS9,fd=1
    #flowop openfile name=openfile82,filesetname=CFS9,fd=1
    flowop appendfilerand name=appendfilerand82,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile82,fd=1
    flowop closefile name=closefile82,fd=1
    flowop openfile name=openfile83,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile83,fd=1,iosize=$iosize
    flowop appendfilerand name=appendfilerand83,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile83,fd=1
    flowop closefile name=closefile83,fd=1
    flowop openfile name=openfile84,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile84,fd=1,iosize=$iosize
    flowop closefile name=closefile84,fd=1
  }
}

echo  "Varmail Version 3.0 personality successfully loaded"

run 5
