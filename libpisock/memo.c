/*
 * memo.c:  Translate Pilot memopad data formats
 *
 * Copyright (c) 1996, Kenneth Albanowski
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pi-source.h"
#include "pi-socket.h"
#include "pi-dlp.h"
#include "pi-memo.h"

void
free_Memo(struct Memo *a)
{
   if (a->text)
      free(a->text);
}

int
unpack_Memo(struct Memo *a, unsigned char *buffer, int len)
{
   if (len < 1)
      return 0;
   a->text = strdup((char *) buffer);
   return strlen((char *) buffer) + 1;
}

int
pack_Memo(struct Memo *a, unsigned char *buffer, int len)
{
   int destlen = (a->text ? strlen(a->text) : 0) + 1;

   if (!buffer)
      return destlen;
   if (len < destlen)
      return 0;
   if (a->text) {
      if (buffer)
	 strcpy((char *) buffer, a->text);
      return strlen(a->text) + 1;
   }
   else {
      if (buffer)
	 buffer[0] = 0;
      return 1;
   }
}

int
unpack_MemoAppInfo(struct MemoAppInfo *ai, unsigned char *record, int len)
{
   unsigned char *start = record;
   int i = unpack_CategoryAppInfo(&ai->category, record, len);

   if (!i)
      return i;
   record += i;
   len -= i;
   if (len >= 4) {
      record += 2;
      ai->sortByAlpha = get_byte(record);
      record += 2;
   }
   else {
      ai->sortByAlpha = 0;
   }
   return (record - start);
}

int
pack_MemoAppInfo(struct MemoAppInfo *ai, unsigned char *record, int len)
{
   int i;
   unsigned char *start = record;

   i = pack_CategoryAppInfo(&ai->category, record, len);
   if (!record)
      return i + 4;
   if (i == 0)			/* category pack failed */
      return 0;
   record += i;
   len -= i;
   if (len < 4)
      return (record - start);
   set_short(record, 0);	/* gapfil new for 2.0 */
   record += 2;
   set_byte(record, ai->sortByAlpha);	/* new for 2.0 */
   record++;
   set_byte(record, 0);		/* gapfil new for 2.0 */
   record++;

   return (record - start);
}