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
set $nfiles=10000
set $meandirwidth=100
set $filesize=16k#cvar(type=cvar-gamma,parameters=mean:16384;gamma:1.5)
set $nthreads=1
set $iosize=0
set $meanappendsize=1k

#define fileset name=CFS10,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,readonly
define fileset name=CFS9,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,readonly
define fileset name=CFS8,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,readonly
define fileset name=CFS7,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,readonly
define fileset name=CFS6,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,readonly
define fileset name=CFS5,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,readonly
define fileset name=CFS4,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,readonly
define fileset name=CFS3,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,readonly
define fileset name=CFS2,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,readonly
define fileset name=CFS1,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,readonly
define fileset name=logfiles,path=$dir,size=$filesize,entries=1,dirwidth=$meandirwidth,prealloc

define process name=filereader1,instances=1
{
  thread name=filereaderthread1,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile1,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile1,fd=1,iosize=$iosize
    flowop closefile name=closefile1,fd=1
    flowop openfile name=openfile2,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile2,fd=1,iosize=$iosize
    flowop closefile name=closefile2,fd=1
    flowop openfile name=openfile3,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile3,fd=1,iosize=$iosize
    flowop closefile name=closefile3,fd=1
    flowop openfile name=openfile4,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile4,fd=1,iosize=$iosize
    flowop closefile name=closefile4,fd=1
    flowop openfile name=openfile5,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile5,fd=1,iosize=$iosize
    flowop closefile name=closefile5,fd=1
    flowop openfile name=openfile6,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile6,fd=1,iosize=$iosize
    flowop closefile name=closefile6,fd=1
    flowop openfile name=openfile7,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile7,fd=1,iosize=$iosize
    flowop closefile name=closefile7,fd=1
    flowop openfile name=openfile8,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile8,fd=1,iosize=$iosize
    flowop closefile name=closefile8,fd=1
    flowop openfile name=openfile9,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile9,fd=1,iosize=$iosize
    flowop closefile name=closefile9,fd=1
    flowop openfile name=openfile10,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile10,fd=1,iosize=$iosize
    flowop closefile name=closefile10,fd=1
    flowop appendfilerand name=appendlog1,filesetname=logfiles,iosize=$meanappendsize,fd=2
  }
}

define process name=filereader2,instances=1
{
  thread name=filereaderthread2,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile11,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile11,fd=1,iosize=$iosize
    flowop closefile name=closefile11,fd=1
    flowop openfile name=openfile12,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile12,fd=1,iosize=$iosize
    flowop closefile name=closefile12,fd=1
    flowop openfile name=openfile13,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile13,fd=1,iosize=$iosize
    flowop closefile name=closefile13,fd=1
    flowop openfile name=openfile14,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile14,fd=1,iosize=$iosize
    flowop closefile name=closefile14,fd=1
    flowop openfile name=openfile15,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile15,fd=1,iosize=$iosize
    flowop closefile name=closefile15,fd=1
    flowop openfile name=openfile16,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile16,fd=1,iosize=$iosize
    flowop closefile name=closefile16,fd=1
    flowop openfile name=openfile17,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile17,fd=1,iosize=$iosize
    flowop closefile name=closefile17,fd=1
    flowop openfile name=openfile18,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile18,fd=1,iosize=$iosize
    flowop closefile name=closefile18,fd=1
    flowop openfile name=openfile19,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile19,fd=1,iosize=$iosize
    flowop closefile name=closefile19,fd=1
    flowop openfile name=openfile20,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile20,fd=1,iosize=$iosize
    flowop closefile name=closefile20,fd=1
    flowop appendfilerand name=appendlog2,filesetname=logfiles,iosize=$meanappendsize,fd=2
  }
}

define process name=filereader3,instances=1
{
  thread name=filereaderthread3,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile21,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile21,fd=1,iosize=$iosize
    flowop closefile name=closefile21,fd=1
    flowop openfile name=openfile22,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile22,fd=1,iosize=$iosize
    flowop closefile name=closefile22,fd=1
    flowop openfile name=openfile23,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile23,fd=1,iosize=$iosize
    flowop closefile name=closefile23,fd=1
    flowop openfile name=openfile24,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile24,fd=1,iosize=$iosize
    flowop closefile name=closefile24,fd=1
    flowop openfile name=openfile25,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile25,fd=1,iosize=$iosize
    flowop closefile name=closefile25,fd=1
    flowop openfile name=openfile26,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile26,fd=1,iosize=$iosize
    flowop closefile name=closefile26,fd=1
    flowop openfile name=openfile27,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile27,fd=1,iosize=$iosize
    flowop closefile name=closefile27,fd=1
    flowop openfile name=openfile28,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile28,fd=1,iosize=$iosize
    flowop closefile name=closefile28,fd=1
    flowop openfile name=openfile29,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile29,fd=1,iosize=$iosize
    flowop closefile name=closefile29,fd=1
    flowop openfile name=openfile30,filesetname=CFS3,fd=1
    flowop readwholefile name=readfile30,fd=1,iosize=$iosize
    flowop closefile name=closefile30,fd=1
    flowop appendfilerand name=appendlog3,filesetname=logfiles,iosize=$meanappendsize,fd=2
  }
}

define process name=filereader4,instances=1
{
  thread name=filereaderthread4,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile31,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile31,fd=1,iosize=$iosize
    flowop closefile name=closefile31,fd=1
    flowop openfile name=openfile32,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile32,fd=1,iosize=$iosize
    flowop closefile name=closefile32,fd=1
    flowop openfile name=openfile33,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile33,fd=1,iosize=$iosize
    flowop closefile name=closefile33,fd=1
    flowop openfile name=openfile34,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile34,fd=1,iosize=$iosize
    flowop closefile name=closefile34,fd=1
    flowop openfile name=openfile35,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile35,fd=1,iosize=$iosize
    flowop closefile name=closefile35,fd=1
    flowop openfile name=openfile36,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile36,fd=1,iosize=$iosize
    flowop closefile name=closefile36,fd=1
    flowop openfile name=openfile37,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile37,fd=1,iosize=$iosize
    flowop closefile name=closefile37,fd=1
    flowop openfile name=openfile38,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile38,fd=1,iosize=$iosize
    flowop closefile name=closefile38,fd=1
    flowop openfile name=openfile39,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile39,fd=1,iosize=$iosize
    flowop closefile name=closefile39,fd=1
    flowop openfile name=openfile40,filesetname=CFS4,fd=1
    flowop readwholefile name=readfile40,fd=1,iosize=$iosize
    flowop closefile name=closefile40,fd=1
    flowop appendfilerand name=appendlog4,filesetname=logfiles,iosize=$meanappendsize,fd=2
  }
}

define process name=filereader5,instances=1
{
  thread name=filereaderthread5,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile41,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile41,fd=1,iosize=$iosize
    flowop closefile name=closefile41,fd=1
    flowop openfile name=openfile42,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile42,fd=1,iosize=$iosize
    flowop closefile name=closefile42,fd=1
    flowop openfile name=openfile43,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile43,fd=1,iosize=$iosize
    flowop closefile name=closefile43,fd=1
    flowop openfile name=openfile44,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile44,fd=1,iosize=$iosize
    flowop closefile name=closefile44,fd=1
    flowop openfile name=openfile45,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile45,fd=1,iosize=$iosize
    flowop closefile name=closefile45,fd=1
    flowop openfile name=openfile46,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile46,fd=1,iosize=$iosize
    flowop closefile name=closefile46,fd=1
    flowop openfile name=openfile47,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile47,fd=1,iosize=$iosize
    flowop closefile name=closefile47,fd=1
    flowop openfile name=openfile48,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile48,fd=1,iosize=$iosize
    flowop closefile name=closefile48,fd=1
    flowop openfile name=openfile49,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile49,fd=1,iosize=$iosize
    flowop closefile name=closefile49,fd=1
    flowop openfile name=openfile50,filesetname=CFS5,fd=1
    flowop readwholefile name=readfile50,fd=1,iosize=$iosize
    flowop closefile name=closefile50,fd=1
    flowop appendfilerand name=appendlog5,filesetname=logfiles,iosize=$meanappendsize,fd=2
  }
}

define process name=filereader6,instances=1
{
  thread name=filereaderthread6,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile61,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile61,fd=1,iosize=$iosize
    flowop closefile name=closefile61,fd=1
    flowop openfile name=openfile62,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile62,fd=1,iosize=$iosize
    flowop closefile name=closefile62,fd=1
    flowop openfile name=openfile63,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile63,fd=1,iosize=$iosize
    flowop closefile name=closefile63,fd=1
    flowop openfile name=openfile64,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile64,fd=1,iosize=$iosize
    flowop closefile name=closefile64,fd=1
    flowop openfile name=openfile65,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile65,fd=1,iosize=$iosize
    flowop closefile name=closefile65,fd=1
    flowop openfile name=openfile66,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile66,fd=1,iosize=$iosize
    flowop closefile name=closefile66,fd=1
    flowop openfile name=openfile67,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile67,fd=1,iosize=$iosize
    flowop closefile name=closefile67,fd=1
    flowop openfile name=openfile68,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile68,fd=1,iosize=$iosize
    flowop closefile name=closefile68,fd=1
    flowop openfile name=openfile69,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile69,fd=1,iosize=$iosize
    flowop closefile name=closefile69,fd=1
    flowop openfile name=openfile70,filesetname=CFS6,fd=1
    flowop readwholefile name=readfile70,fd=1,iosize=$iosize
    flowop closefile name=closefile70,fd=1
    flowop appendfilerand name=appendlog6,filesetname=logfiles,iosize=$meanappendsize,fd=2
  }
}

define process name=filereader7,instances=1
{
  thread name=filereaderthread7,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile71,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile71,fd=1,iosize=$iosize
    flowop closefile name=closefile71,fd=1
    flowop openfile name=openfile72,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile72,fd=1,iosize=$iosize
    flowop closefile name=closefile72,fd=1
    flowop openfile name=openfile73,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile73,fd=1,iosize=$iosize
    flowop closefile name=closefile73,fd=1
    flowop openfile name=openfile74,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile74,fd=1,iosize=$iosize
    flowop closefile name=closefile74,fd=1
    flowop openfile name=openfile75,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile75,fd=1,iosize=$iosize
    flowop closefile name=closefile75,fd=1
    flowop openfile name=openfile76,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile76,fd=1,iosize=$iosize
    flowop closefile name=closefile76,fd=1
    flowop openfile name=openfile77,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile77,fd=1,iosize=$iosize
    flowop closefile name=closefile77,fd=1
    flowop openfile name=openfile78,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile78,fd=1,iosize=$iosize
    flowop closefile name=closefile78,fd=1
    flowop openfile name=openfile79,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile79,fd=1,iosize=$iosize
    flowop closefile name=closefile79,fd=1
    flowop openfile name=openfile80,filesetname=CFS7,fd=1
    flowop readwholefile name=readfile80,fd=1,iosize=$iosize
    flowop closefile name=closefile80,fd=1
    flowop appendfilerand name=appendlog7,filesetname=logfiles,iosize=$meanappendsize,fd=2
  }
}

define process name=filereader8,instances=1
{
  thread name=filereaderthread8,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile81,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile81,fd=1,iosize=$iosize
    flowop closefile name=closefile81,fd=1
    flowop openfile name=openfile82,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile82,fd=1,iosize=$iosize
    flowop closefile name=closefile82,fd=1
    flowop openfile name=openfile83,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile83,fd=1,iosize=$iosize
    flowop closefile name=closefile83,fd=1
    flowop openfile name=openfile84,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile84,fd=1,iosize=$iosize
    flowop closefile name=closefile84,fd=1
    flowop openfile name=openfile85,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile85,fd=1,iosize=$iosize
    flowop closefile name=closefile85,fd=1
    flowop openfile name=openfile86,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile86,fd=1,iosize=$iosize
    flowop closefile name=closefile86,fd=1
    flowop openfile name=openfile87,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile87,fd=1,iosize=$iosize
    flowop closefile name=closefile87,fd=1
    flowop openfile name=openfile88,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile88,fd=1,iosize=$iosize
    flowop closefile name=closefile88,fd=1
    flowop openfile name=openfile89,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile89,fd=1,iosize=$iosize
    flowop closefile name=closefile89,fd=1
    flowop openfile name=openfile90,filesetname=CFS8,fd=1
    flowop readwholefile name=readfile90,fd=1,iosize=$iosize
    flowop closefile name=closefile90,fd=1
    flowop appendfilerand name=appendlog8,filesetname=logfiles,iosize=$meanappendsize,fd=2
  }
}

define process name=filereader9,instances=1
{
  thread name=filereaderthread9,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile91,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile91,fd=1,iosize=$iosize
    flowop closefile name=closefile91,fd=1
    flowop openfile name=openfile92,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile92,fd=1,iosize=$iosize
    flowop closefile name=closefile92,fd=1
    flowop openfile name=openfile93,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile93,fd=1,iosize=$iosize
    flowop closefile name=closefile93,fd=1
    flowop openfile name=openfile94,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile94,fd=1,iosize=$iosize
    flowop closefile name=closefile94,fd=1
    flowop openfile name=openfile95,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile95,fd=1,iosize=$iosize
    flowop closefile name=closefile95,fd=1
    flowop openfile name=openfile96,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile96,fd=1,iosize=$iosize
    flowop closefile name=closefile96,fd=1
    flowop openfile name=openfile97,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile97,fd=1,iosize=$iosize
    flowop closefile name=closefile97,fd=1
    flowop openfile name=openfile98,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile98,fd=1,iosize=$iosize
    flowop closefile name=closefile98,fd=1
    flowop openfile name=openfile99,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile99,fd=1,iosize=$iosize
    flowop closefile name=closefile99,fd=1
    flowop openfile name=openfile00,filesetname=CFS9,fd=1
    flowop readwholefile name=readfile00,fd=1,iosize=$iosize
    flowop closefile name=closefile00,fd=1
    flowop appendfilerand name=appendlog9,filesetname=logfiles,iosize=$meanappendsize,fd=2
  }
}

define process name=filereader0,instances=0
{
  thread name=filereaderthread0,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile01,filesetname=CFS10,fd=1
    flowop readwholefile name=readfile01,fd=1,iosize=$iosize
    flowop closefile name=closefile01,fd=1
    flowop openfile name=openfile02,filesetname=CFS10,fd=1
    flowop readwholefile name=readfile02,fd=1,iosize=$iosize
    flowop closefile name=closefile02,fd=1
    flowop openfile name=openfile03,filesetname=CFS10,fd=1
    flowop readwholefile name=readfile03,fd=1,iosize=$iosize
    flowop closefile name=closefile03,fd=1
    flowop openfile name=openfile04,filesetname=CFS10,fd=1
    flowop readwholefile name=readfile04,fd=1,iosize=$iosize
    flowop closefile name=closefile04,fd=1
    flowop openfile name=openfile05,filesetname=CFS10,fd=1
    flowop readwholefile name=readfile05,fd=1,iosize=$iosize
    flowop closefile name=closefile05,fd=1
    flowop openfile name=openfile06,filesetname=CFS10,fd=1
    flowop readwholefile name=readfile06,fd=1,iosize=$iosize
    flowop closefile name=closefile06,fd=1
    flowop openfile name=openfile07,filesetname=CFS10,fd=1
    flowop readwholefile name=readfile07,fd=1,iosize=$iosize
    flowop closefile name=closefile07,fd=1
    flowop openfile name=openfile08,filesetname=CFS10,fd=1
    flowop readwholefile name=readfile08,fd=1,iosize=$iosize
    flowop closefile name=closefile08,fd=1
    flowop openfile name=openfile09,filesetname=CFS10,fd=1
    flowop readwholefile name=readfile09,fd=1,iosize=$iosize
    flowop closefile name=closefile09,fd=1
    flowop openfile name=openfile000,filesetname=CFS10,fd=1
    flowop readwholefile name=readfile000,fd=1,iosize=$iosize
    flowop closefile name=closefile000,fd=1
    flowop appendfilerand name=appendlog0,filesetname=logfiles,iosize=$meanappendsize,fd=2
  }
}

echo  "Web-server Version 3.1 personality successfully loaded"

run 10
