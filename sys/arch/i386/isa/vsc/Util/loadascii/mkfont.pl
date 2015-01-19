#	$NecBSD: mkfont.pl,v 1.7 1998/03/14 07:09:37 kmatsuda Exp $
#	$NetBSD$
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1996, 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
# 
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

$start = 0;

while(<>)
{
	chop($_);

	@x = split(/ +/, $_);

	if ($x[0] eq 'ENCODING')
	{
		print "{\n";
		print "	", $x[1], ",\n";
		print "	FONTASCII,\n";
		print "	0,\n";
		next;
	}

	if ($x[0] eq 'BITMAP')
	{
		$start = 16;
		print "		{\n";
		next;
	}

	if ($start > 0)
	{
		print "			0x",$x[0],$x[0], ",\n";
		$start --;
		if ($start == 0)
		{
			print "		},\n";
			print "	0,\n";
			print "},\n\n";
		}
	}
}
