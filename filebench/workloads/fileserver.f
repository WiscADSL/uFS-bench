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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

set $dir=/tmp
set $nfiles=1000
set $meandirwidth=20
set $filesize=cvar(type=cvar-gamma,parameters=mean:131072;gamma:1.5)
set $nthreads=1
set $iosize=1m
set $meanappendsize=16k
set $runtime=5

define fileset name=CFS1,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80
define fileset name=CFS2,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80

define process name=filereader1,instances=1
{
  thread name=filereaderthread1,memsize=10m,instances=$nthreads
  {
    flowop createfile name=createfile1,filesetname=CFS1,fd=1
    flowop writewholefile name=wrtfile1,srcfd=1,fd=1,iosize=$iosize
    flowop closefile name=closefile1,fd=1
    flowop openfile name=openfile1,filesetname=CFS1,fd=1
    flowop appendfilerand name=appendfilerand1,iosize=$meanappendsize,fd=1
    flowop closefile name=closefile2,fd=1
    flowop openfile name=openfile2,filesetname=CFS1,fd=1
    flowop readwholefile name=readfile1,fd=1,iosize=$iosize
    flowop closefile name=closefile3,fd=1
    flowop deletefile name=deletefile1,filesetname=CFS1
    flowop statfile name=statfile1,filesetname=CFS1
  }
}

define process name=filereader2,instances=1
{
  thread name=filereaderthread2,memsize=10m,instances=$nthreads
  {
    flowop createfile name=createfile11,filesetname=CFS2,fd=1
    flowop writewholefile name=wrtfile11,srcfd=1,fd=1,iosize=$iosize
    flowop closefile name=closefile11,fd=1
    flowop openfile name=openfile11,filesetname=CFS2,fd=1
    flowop appendfilerand name=appendfilerand11,iosize=$meanappendsize,fd=1
    flowop closefile name=closefile12,fd=1
    flowop openfile name=openfile12,filesetname=CFS2,fd=1
    flowop readwholefile name=readfile11,fd=1,iosize=$iosize
    flowop closefile name=closefile13,fd=1
    flowop deletefile name=deletefile11,filesetname=CFS2
    flowop statfile name=statfile11,filesetname=CFS2
  }
}

echo  "File-server Version 3.0 personality successfully loaded"

run $runtime
