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
set $nfiles=500
set $meandirwidth=1000000
set $meanfilesize=16k
set $nthreads=1
set $meaniosize=16k
set $iosize=1m

define fileset name=CFS1,path=$dir,size=$meanfilesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80
define fileset name=CFS2,path=$dir,size=$meanfilesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80
define fileset name=CFS3,path=$dir,size=$meanfilesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80

define process name=proxycache1,instances=1
{
  thread name=proxycache1,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile1,filesetname=CFS1
    flowop createfile name=createfile1,filesetname=CFS1,fd=1
    flowop appendfilerand name=appendfilerand1,iosize=$meaniosize,fd=1
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
    flowop opslimit name=limit1
  }
}

define process name=proxycache2,instances=1
{
  thread name=proxycache2,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile11,filesetname=CFS2
    flowop createfile name=createfile11,filesetname=CFS2,fd=1
    flowop appendfilerand name=appendfilerand11,iosize=$meaniosize,fd=1
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
    flowop opslimit name=limit2
  }
}

define process name=proxycache3,instances=0
{
  thread name=proxycache3,memsize=10m,instances=$nthreads
  {
    flowop deletefile name=deletefile21,filesetname=CFS3
    flowop createfile name=createfile21,filesetname=CFS3,fd=1
    flowop appendfilerand name=appendfilerand21,iosize=$meaniosize,fd=1
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
    flowop opslimit name=limit3
  }
}

echo  "Web proxy-server Version 3.0 personality successfully loaded"

run 5
