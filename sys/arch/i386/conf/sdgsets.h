/*	$NecBSD: sdgsets.h,v 3.7 1998/03/14 07:07:09 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*************************************************************
 * SCSI GEOMETRY TABLE SET01   
 *************************************************************/
static struct sdg_conv_element sdg_element_array_set01[] = {
	/******** example *********
	{ { 2, 65, }, { 4, 64, } },		set01 matching pattern 0

	{ { 3, 99, }, { 2, 64, } },		set01 matching pattern 1
	
		.........
	***************************/
};

struct sdg_conv_table sdg_conv_table_set01 = {
	SDG_CONV_ELEMENT_SZ(sdg_element_array_set01),
	&sdg_element_array_set01[0]
};

/*************************************************************
 * SCSI GEOMETRY TABLE SET02  
 *************************************************************/
static struct sdg_conv_element sdg_element_array_set02[] = {

};

static struct sdg_conv_table sdg_conv_table_set02 = {
	SDG_CONV_ELEMENT_SZ(sdg_element_array_set02),
	&sdg_element_array_set02[0]
};


/* please add .... SET03 SET04 .... SET15 */

/*************************************************************
 * SCSI GEOMETRY SET INDEX TABLE
 *************************************************************/
struct sdg_conv_table *sdg_conv_table_array[16] =
{
	NULL,		/* The first slot must be NULL */
	&sdg_conv_table_set01,
	&sdg_conv_table_set02,
};
