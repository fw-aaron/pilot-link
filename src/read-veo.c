/* ex: set tabstop=4 expandtab: */
/*
 * read-veo.c
 *
 * Copyright (c) 2003-2004, Angus Ainslie
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "getopt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "pi-source.h"
#include "pi-veo.h"
#include "pi-file.h"
#include "pi-header.h"

#ifdef HAVE_PNG
# include "png.h"
# if (PNG_LIBPNG_VER < 10201)
#  define png_voidp_NULL (png_voidp)NULL
#  define png_error_ptr_NULL (png_error_ptr)NULL
# endif
#endif

#define pi_mktag(c1,c2,c3,c4) (((c1)<<24)|((c2)<<16)|((c3)<<8)|(c4))

/* Declare prototypes */
static void display_help (char *progname);
void print_splash (char *progname);
int pilot_connect (char *port);
int Decode (unsigned char *inP, unsigned char *outP, short w);
int Gen24bitRow (long flags, int r, struct Veo *v, unsigned char *row);
void WritePicture (int sd, int db, int type, char *name, char *progname,
				   long flags);
int protect_files (char *name, char *extension);

struct option options[] =
{
	 {"help", no_argument, NULL, 'h'},
	 {"version", no_argument, NULL, 'v'},
	 {"port", required_argument, NULL, 'p'},
	 {"list", no_argument, NULL, 'l'},
	 {"type", required_argument, NULL, 't'},
	 {"name", required_argument, NULL, 'n'},
	 {"colour", no_argument, NULL, 'c'},
	 {"bias", required_argument, NULL, 'b'},
	 {NULL, 0, NULL, 0}
};

static const char *optstring = "hvp:lt:n:cb:";

#define VEO_COLOUR_CORRECT 0x01
#define VEO_BIAS           0x12

uint8_t redLUT[256], greenLUT[256], blueLUT[256];

double bias_factor = 0.50;

/***********************************************************************
 *
 * Function:    display_help
 *
 * Summary:     Print out the --help options and arguments
 *
 * Parameters:  None
 *
 * Returns:     Nothing
 *
 ***********************************************************************/
static void display_help (char *progname)
{
   printf("   Syncronize your Veo Traveler databases with your desktop machine\n\n");
   printf("   Usage: %s -p /dev/pilot [options]\n\n", progname);
   printf("   Options:\n");
   printf("     -p, --port <port>       Use device file <port> to communicate with Palm\n");
   printf("     -h, --help              Display help information for %s\n", progname);
   printf("     -v, --version           Display %s version information\n\n", progname);
#ifdef HAVE_PNG
   printf("     -t, --type [type],      Specify picture output type (ppm or png)\n");
#endif
   printf("     -l                      List Photos on device\n");
   printf("     -b, --bias [num],       lighten or darken the image\n");
   printf("                             0..50   darken image\n");
   printf("                             50..100 lighten image\n");
   printf("     -c, --colour,           colour correct the output colours\n");
   printf("     -n, --name [name],      Specify output picture by name\n");
   printf("   Examples: %s -p /dev/pilot -l\n\n", progname);

   exit (0);
}

/***********************************************************************
 *
 * Function:    fmt_date
 *
 * Summary:     Format the output date on the images
 *
 * Parameters:
 *
 * Returns:
 *
 ***********************************************************************/
static const char *fmt_date (struct Veo *v)
{
   static char buf[24];

   sprintf (buf, "%d-%02d-%02d", v->year, v->month, v->day);
   return buf;

}

/***********************************************************************
 *
 * Function:	protect_files
 *
 * Summary:     Adjust output file name so as to not overwrite an exsisting 
 *              file
 *
 * Parameters:  filename and file extension
 *
 * Returns:     1 file name protected
 *              0 no alernate name found
 *
 ***********************************************************************/
int protect_files (char *name, char *extension)
{
   char *save_name, c = 1;

   save_name = strdup (name);

   if (NULL == save_name)
	 {
		printf ("Failed to generate filename %s%s\n", name, extension);
		return (0);
	 }

   sprintf (name, "%s%s", save_name, extension);

   while (access (name, F_OK) == 0)
	 {
		sprintf (name, "%s_%02d%s", save_name, c, extension);

		c++;

		if (c == 'z' + 1)
		  c = 'A';

		if (c == 'Z' + 1)
		  {
			 printf ("Failed to generate filename %s\n", name);
			 return (0);
		  }
	 }

   free (save_name);

   return (1);
}

/***********************************************************************
 *
 * Function:	Decode
 *
 * Summary:	Decode one record from the Veo database.
 *
 * Parameters:	inP - the compressed record
 *              outP - the uncompressed record
 *              w - the width of the picture
 * Returns:	1 always
 *
 ***********************************************************************/
int Decode (unsigned char *inP, unsigned char *outP, short w)
{
   short shifter = 7, index = 0;
   unsigned short tmp0, tmp3, tmp4, i, j;	/* d3 */
   unsigned char *origOutP = outP;

   shifter = 7;

   for (j = 0; j < 2; j++)
	 {
		if (j == 1)
		  origOutP += w * 2;

		for (i = 0; i < 2; i++)
		  {
			 switch (i)
			   {
				case 0:
				  outP = (unsigned char *) (origOutP + 1);
				  break;
				case 1:
				  outP = (unsigned char *) (origOutP + w);
				  break;
			   }

			 if (shifter != 7)
			   {
				  shifter = 7;
				  inP++;
			   }

			 *outP = *inP++;

			 outP += 2;		/* a3 */
			 index = 2;

			 while (index < w)
			   {
				  /* Top of decompress loop */
				  tmp3 = 1 << shifter;

				  tmp3 &= *inP;

				  if ((shifter & 0xFF) <= 0)
					{
					   shifter = 7;
					   inP++;
					}
				  else
					{
					   shifter--;
					}

				  if ((tmp3 & 0xFF) != 0)
					{
					   *outP = *(outP - 2);
					}
				  else
					{
					   tmp0 = inP[1];
					   tmp4 = inP[0] << 8;

					   tmp4 |= tmp0;

					   tmp4 = tmp4 << (7 - shifter);

					   if (shifter >= 5)
						 {
							shifter -= 5;
						 }
					   else
						 {
							inP++;
							shifter += 3;
						 }

					   tmp4 = tmp4 >> 11;

					   /* if((char)tmp4 <= 0) */
					   if ((char) tmp4 == 0)
						 {
							tmp3 = inP[0] << 8 | inP[1];

							tmp3 = tmp3 << (7 - shifter);

							if (shifter >= 8)
							  {
								 shifter -= 8;
							  }
							else
							  {
								 inP++;
								 shifter &= 0xf;
							  }

							*outP = tmp3 >> 8;
						 }
					   else
						 {
							if (tmp4 & 0x10)
							  {
								 tmp4 &= 0xf;

								 *outP = *(outP - 2) - tmp4;
							  }
							else
							  {
								 tmp4 &= 0xf;

								 *outP = *(outP - 2) + tmp4;
							  }
						 }
					}
				  outP += 2;
				  index += 2;
			   }
			 /* End 'while (index < w)' */
		  }

		outP = origOutP;

		if (shifter != 7)
		  {
			 shifter = 7;
			 inP++;
		  }

		*outP = *inP++;

		if (shifter != 7)
		  {
			 shifter = 7;
			 inP++;
		  }

		outP[w + 1] = *inP++;

		outP += 2;		/* a3 */
		index = 2;

		while (index < w)
		  {
	  /* Top of decompress loop */
			 tmp3 = 1 << shifter;

			 tmp3 &= *inP;

			 if ((shifter & 0xFF) <= 0)
			   {
				  shifter = 7;
				  inP++;
			   }
			 else
			   {
				  shifter--;
			   }

			 if ((tmp3 & 0xFF) != 0)
			   {
				  *outP = outP[w - 1];
			   }
			 else
			   {
				  tmp0 = inP[1];
				  tmp4 = inP[0] << 8;

				  tmp4 |= tmp0;

				  tmp4 = tmp4 << (7 - shifter);

				  if (shifter >= 5)
					{
					   shifter -= 5;
					}
				  else
					{
					   inP++;
					   shifter += 3;
					}

				  tmp4 = tmp4 >> 11;

				  if ((char) tmp4 == 0)
					{
					   tmp3 = inP[0] << 8 | inP[1];

					   tmp3 = tmp3 << (7 - shifter);

					   if (shifter >= 8)
						 {
							shifter -= 8;
						 }
					   else
						 {
							inP++;
							shifter &= 0xf;
						 }

					   *outP = tmp3 >> 8;
					}
				  else
					{
					   if (tmp4 & 0x10)
						 {
							tmp4 &= 0xf;

							*outP = outP[w - 1] - tmp4;
						 }
					   else
						 {
							tmp4 &= 0xf;
							*outP = outP[w - 1] + tmp4;
						 }
					}
			   }

			 tmp3 = 1 << shifter;
			 tmp3 &= *inP;

			 if ((shifter & 0xFF) <= 0)
			   {
				  shifter = 7;
				  inP++;
			   }
			 else
			   {
				  shifter--;
			   }

			 if ((tmp3 & 0xFF) != 0)
			   {
				  outP[w + 1] = *outP;
			   }
			 else
			   {
				  tmp0 = inP[1];
				  tmp4 = inP[0] << 8;

				  tmp4 |= tmp0;

				  tmp4 = tmp4 << (7 - shifter);

				  if (shifter >= 5)
					{
					   shifter -= 5;
					}
				  else
					{
					   inP++;
					   shifter += 3;
					}

				  tmp4 = tmp4 >> 11;

				  /* if((char)tmp4 <= 0) */
				  if ((char) tmp4 == 0)
					{
					   tmp3 = inP[0] << 8 | inP[1];

					   tmp3 = tmp3 << (7 - shifter);

					   if (shifter >= 8)
						 {
							shifter -= 8;
						 }
					   else
						 {
							inP++;
							shifter &= 0xf;
						 }

					   outP[w + 1] = tmp3 >> 8;
					}
				  else
					{
					   if (tmp4 & 0x10)
						 {
							tmp4 &= 0xf;

							outP[w + 1] = *outP - tmp4;
						 }
					   else
						 {
							tmp4 &= 0xf;

							outP[w + 1] = *outP + tmp4;
						 }
					}
			   }
			 outP += 2;
			 index += 2;
		  }
		/* End 'while (index < w)' */

	 }
   /* End 'for (j = 0; j < 2; j++)' */
   return (1);
}

/***********************************************************************
 *
 * Function:	GetPicData
 *
 * Summary:     Requests one record of bayer data from the palm
 *              and then decodes it.
 *
 * Parameters:  r - the row we are looking for
 *              v - veo record
 *              row - the bayer data containg our row
 *
 * Returns:     the size of the encoded record
 *
 ***********************************************************************/
static int
  GetPicData (uint32_t flags, int r, struct Veo *v, unsigned char *row)
{
   int attr, category, i, len;
   /* The compressed record can be upto twice as large as the
    * uncompressed record ??? */
   unsigned char tmpRow[5120];

   if (!v->sd)
	 return (-1);

   /* Each record contains four rows of bayer data */
   len = dlp_ReadRecordByIndex (v->sd, v->db, 1 + r / 4, tmpRow, 0, 0,
								&attr, &category);

   if (len < 0)
	 return 0;

   Decode (tmpRow, row, v->width);

   return (len);
}

#define max(a,b) (( a > b ) ? a : b )
#define min(a,b) (( a < b ) ? a : b )

/***********************************************************************
 *
 * Function:    Bias
 * 
 *              Bias is based on the Fast Alternative to Perlin's Bias 
 *              algorithm in Graphics Gems IV by
 *              Christophe Schlick schlick@labri.u-bordeuax.fr
 *
 * Summary:     Lighten or darken the image
 *
 * Parameters:
 *
 * Returns:
 *
 ***********************************************************************/
static void Bias( double bias, int width, int height, uint8_t *data )
{
   int i;
   double num, denom, t;
      
   for( i=0; i<width*height; i++ )
     {
	t = (double)data[i]/256.0;
	num = t;
	denom = (1.0/bias - 2) * (1.0 - t) + 1;
	data[i] = num/denom * 256.0;     
     }
}

int ColourCorrect (struct Veo *v, uint8_t *red, uint8_t *green, uint8_t *blue, long flags )
{
	uint8_t *tmpRow;
	uint8_t gMin, gMax, rMin, rMax, bMin, bMax;
	float gInc, rInc, bInc, gCur, rCur, bCur;
	float rMean = 0, gMean = 0, bMean = 0, maxMean;
	uint16_t	width = v->width;
	uint16_t	height = v->height;
	int i;
   	float redCeiling = 254;
   	float greenCeiling = 252;
   	float blueCeiling = 255;

	memset( red, 0, 256 * sizeof( uint8_t ));
	memset( green, 0, 256 * sizeof( uint8_t ));
	memset( blue, 0, 256 * sizeof( uint8_t ));

	gMin = rMin = bMin = 255;
	gMax = rMax = bMax = 0;
		
	tmpRow = malloc( 2560 );
	
	GetPicData( 0, 0, v, tmpRow );
	
	for( i=0; i<width; i += 2 )
	{
		gMin = min( gMin, tmpRow[i] );
		rMin = min( rMin, tmpRow[i+width] );
		bMin = min( bMin, tmpRow[i+1] );
		gMin = min( gMin, tmpRow[i+width+1] );
		gMax = max( gMax, tmpRow[i] );
		rMax = max( rMax, tmpRow[i+width] );
		bMax = max( bMax, tmpRow[i+1] );
		gMax = max( gMax, tmpRow[i+width+1] );
		
		rMean += tmpRow[i+width];
		gMean += tmpRow[i];
		gMean += tmpRow[i+width+1];
		bMean += tmpRow[i+1];
	}	
	
	
	if( width == 640 )
	{
		for( i=width*2; i<width*3; i += 2 )
		{
			gMin = min( gMin, tmpRow[i] );
			rMin = min( rMin, tmpRow[i+width] );
			bMin = min( bMin, tmpRow[i+1] );
			gMin = min( gMin, tmpRow[i+width+1] );
			gMax = max( gMax, tmpRow[i] );
			rMax = max( rMax, tmpRow[i+width] );
			bMax = max( bMax, tmpRow[i+1] );
			gMax = max( gMax, tmpRow[i+width+1] );		

			rMean += tmpRow[i+width];
			gMean += tmpRow[i];
			gMean += tmpRow[i+width+1];
			bMean += tmpRow[i+1];
		}
	}	

	GetPicData( 0, height/2, v, tmpRow );
	
	for( i=0; i<width; i += 2 )
	{
		gMin = min( gMin, tmpRow[i] );
		rMin = min( rMin, tmpRow[i+width] );
		bMin = min( bMin, tmpRow[i+1] );
		gMin = min( gMin, tmpRow[i+width+1] );
		gMax = max( gMax, tmpRow[i] );
		rMax = max( rMax, tmpRow[i+width] );
		bMax = max( bMax, tmpRow[i+1] );
		gMax = max( gMax, tmpRow[i+width+1] );		

		rMean += tmpRow[i+width];
		gMean += tmpRow[i];
		gMean += tmpRow[i+width+1];
		bMean += tmpRow[i+1];
	}	
	
	if( width == 640 )
	{
		for( i=width*2; i<width*3; i += 2 )
		{
			gMin = min( gMin, tmpRow[i] );
			rMin = min( rMin, tmpRow[i+width] );
			bMin = min( bMin, tmpRow[i+1] );
			gMin = min( gMin, tmpRow[i+width+1] );
			gMax = max( gMax, tmpRow[i] );
			rMax = max( rMax, tmpRow[i+width] );
			bMax = max( bMax, tmpRow[i+1] );
			gMax = max( gMax, tmpRow[i+width+1] );		

			rMean += tmpRow[i+width];
			gMean += tmpRow[i];
			gMean += tmpRow[i+width+1];
			bMean += tmpRow[i+1];
		}
	}	

	GetPicData( 0, height-1, v, tmpRow );
	
	for( i=0; i<width; i += 2 )
	{
		gMin = min( gMin, tmpRow[i] );
		rMin = min( rMin, tmpRow[i+width] );
		bMin = min( bMin, tmpRow[i+1] );
		gMin = min( gMin, tmpRow[i+width+1] );
		gMax = max( gMax, tmpRow[i] );
		rMax = max( rMax, tmpRow[i+width] );
		bMax = max( bMax, tmpRow[i+1] );
		gMax = max( gMax, tmpRow[i+width+1] );		

		rMean += tmpRow[i+width];
		gMean += tmpRow[i];
		gMean += tmpRow[i+width+1];
		bMean += tmpRow[i+1];
	}	
	
	if( width == 640 )
	{
		for( i=width*2; i<width*3; i += 2 )
		{
			gMin = min( gMin, tmpRow[i] );
			rMin = min( rMin, tmpRow[i+width] );
			bMin = min( bMin, tmpRow[i+1] );
			gMin = min( gMin, tmpRow[i+width+1] );
			gMax = max( gMax, tmpRow[i] );
			rMax = max( rMax, tmpRow[i+width] );
			bMax = max( bMax, tmpRow[i+1] );
			gMax = max( gMax, tmpRow[i+width+1] );		

			rMean += tmpRow[i+width];
			gMean += tmpRow[i];
			gMean += tmpRow[i+width+1];
			bMean += tmpRow[i+1];
		}
	}	

	rMean = rMean / ( 640 * 3 );
	gMean = gMean / ( 640 * 6 );
	bMean = bMean / ( 640 * 3 );
	
	maxMean = max( gMean-gMin, max( bMean-bMin, rMean-rMin ));
//	maxMean = max( gMean, max( bMean, rMean ));
		
	rInc = maxMean / (rMean-rMin);
   	gInc = maxMean / (gMean-gMin);
   	bInc = maxMean / (bMean-bMin);
	
   rCur = 0;
   gCur = 0;
   bCur = 0;

   for (i = 0; i<256; i++)
	 {
	 	if( i < rMin )
		 	red[i] = 0;
		else 
		{
			if( rCur < redCeiling )
			  red[i] = rCur;
			else
			  red[i] = redCeiling;

			rCur += rInc;
		}

		if( i < gMin )
			green[i] = 0;
		else
		{
			if( gCur < greenCeiling )
			  green[i] = gCur;
			else
			  green[i] = greenCeiling;

			gCur += gInc;
		}
		
		if( i < bMin )
			blue[i] = 0;
		else
		{
			if( bCur < blueCeiling )
			  blue[i] = bCur;
			else
			  blue[i] = blueCeiling;

			bCur += bInc;
		}
	 }

	free( tmpRow );

	return( 1 );
}

/***********************************************************************
 *
 * Function:	Gen24bitRow
 *
 * Summary:     It requests some decoded bayer pattern data from the palm
 *              and then interpolates one RGB row from that data.
 *
 * Parameters:  r - the row to be interpolated
 *              v - veo record
 *              row - the returned RGB data
 * Returns:     1 success
 *              -1 failure
 *
 ***********************************************************************/
int Gen24bitRow (long flags, int r, struct Veo *v, unsigned char *row)
{
   int i, rawW, rawH, modR = r % 4;

   unsigned char rowA[2560], rowB[2560];
   unsigned char *rAP, *rBP, *rCP;

   rawW = v->width / 2;
   rawH = v->height / 2;

   if (r == 0)
	 {
		if (-1 == GetPicData (flags, r, v, rowB))
		  return (-1);
		rAP = rBP = rowB;
		rCP = rowB + v->width;

	 }
   else if (r == (v->height - 1))
	 {
		if (-1 == GetPicData (flags, r, v, rowA))
		  return (-1);
		rAP = rowA + v->width * 2;
		rCP = rBP = rowA + v->width * 3;

	 }
   else if (modR == 0)
	 {
		if (-1 == GetPicData (flags, r - 1, v, rowA))
		  return (-1);
		rAP = rowA + v->width * 3;
		if (-1 == GetPicData (flags, r, v, rowB))
		  return (-1);
		rBP = rowB;
		rCP = rowB + v->width;

	 }
   else if (modR == 3)
	 {
		if (-1 == GetPicData (flags, r, v, rowA))
		  return (-1);
		rAP = rowA + v->width * 2;
		rBP = rowA + v->width * 3;
		if (-1 == GetPicData (flags, r + 1, v, rowB))
		  return (-1);
		rCP = rowB;

	 }
   else
	 {
		if (-1 == GetPicData (flags, r, v, rowA))
		  return (-1);
		rAP = rowA + v->width * (modR - 1);
		rBP = rowA + v->width * modR;
		rCP = rowA + v->width * (modR + 1);
	 }

   /* Bayer Pattern
    * GBGB
    * RGRG
    * GBGB
    * RGRG */
   if (r % 2 == 0)
	 {
      /* green blue center */
		row[0] = (rAP[0] + rCP[0]) >> 1;
		row[1] = rBP[0];
		row[2] = rBP[1];

      /* blue center */
		row[3] = (rAP[0] + rCP[0] + rAP[2] + rCP[2]) >> 2;
		row[4] = (rAP[1] + rBP[0] + rBP[2] + rCP[1]) >> 2;
		row[5] = rBP[1];

		for (i = 1; i < rawW - 1; i++)
		  {
	  /* green blue center */
			 row[i * 6] = (rAP[i * 2] + rCP[i * 2]) >> 1;
			 row[i * 6 + 1] = rBP[i * 2];
			 row[i * 6 + 2] = (rBP[i * 2 - 1] + rBP[i * 2 + 1]) >> 1;

	  /* blue center */
			 row[i * 6 + 3] =
			   (rAP[i * 2] + rCP[i * 2] + rAP[i * 2 + 2] + rCP[i * 2 + 2]) >> 2;
			 row[i * 6 + 4] =
			   (rAP[i * 2 + 1] + rBP[i * 2] + rBP[i * 2 + 2] +
				rCP[i * 2 + 1]) >> 2;
			 row[i * 6 + 5] = rBP[i * 2 + 1];
		  }

		i = rawW - 1;
      /* green blue center */
		row[i * 6] = (rAP[i * 2] + rCP[i * 2]) >> 1;
		row[i * 6 + 1] = rBP[i * 2];
		row[i * 6 + 2] = (rBP[i * 2 - 1] + rBP[i * 2 + 1]) >> 1;

      /* blue center */
		row[i * 6 + 3] = (rAP[i * 2] + rCP[i * 2]) >> 1;
		row[i * 6 + 4] = (rAP[i * 2 + 1] + rBP[i * 2] + rCP[i * 2 + 1]) / 3;
		row[i * 6 + 5] = rBP[i * 2 + 1];

	 }
   else
	 {
      /* red center */
		row[0] = rBP[0];
		row[1] = (rAP[0] + rBP[1] + rCP[0]) / 3;
		row[2] = (rAP[1] + rCP[1]) >> 1;

      /* green red center */
		row[3] = (rBP[0] + rBP[2]) >> 1;
		row[4] = rBP[1];
		row[5] = (rAP[1] + rCP[1]) >> 1;

		for (i = 1; i < rawW - 1; i++)
		  {
	  /* red center */
			 row[i * 6] = rBP[i * 2];
			 row[i * 6 + 1] =
			   (rAP[i * 2] + rBP[i * 2 - 1] + rBP[i * 2 + 1] + rCP[i * 2]) >> 2;
			 row[i * 6 + 2] =
			   (rAP[i * 2 - 1] + rAP[i * 2 + 1] + rCP[i * 2 - 1] +
				rCP[i * 2 + 1]) >> 2;

	  /* green red center */
			 row[i * 6 + 3] = (rBP[i * 2] + rBP[i * 2 + 2]) >> 1;
			 row[i * 6 + 4] = rBP[i * 2 + 1];
			 row[i * 6 + 5] = (rAP[i * 2 + 1] + rCP[i * 2 + 1]) >> 1;
		  }

		i = rawW - 1;

      /* red center */
		row[i * 6] = rBP[i * 2];
		row[i * 6 + 1] =
		  (rAP[i * 2] + rBP[i * 2 - 1] + rBP[i * 2 + 1] + rCP[i * 2]) >> 2;
		row[i * 6 + 2] =
		  (rAP[i * 2 - 1] + rAP[i * 2 + 1] + rCP[i * 2 - 1] +
		   rCP[i * 2 + 1]) >> 2;

      /* green red center */
		row[i * 6 + 3] = rBP[i * 2];
		row[i * 6 + 4] = rBP[i * 2 + 1];
		row[i * 6 + 5] = (rAP[i * 2 + 1] + rCP[i * 2 + 1]) >> 1;
	 }

   if (flags & VEO_COLOUR_CORRECT)
	 {
		for (i = 0; i < v->width * 3; i += 3)
		  {
			 row[i] = redLUT[row[i]];
			 row[i + 1] = greenLUT[row[i + 1]];
			 row[i + 2] = blueLUT[row[i + 2]];
		  }
	 }

   if (flags & VEO_BIAS)
	 Bias( bias_factor, v->width*3, 1, row );
   
   return (1);
}

/***********************************************************************
 *
 * Function:    write_png
 *
 * Summary:
 *
 * Parameters:  None
 *
 * Returns:     Nothing
 *
 ***********************************************************************/
#ifdef HAVE_PNG
void write_png (FILE * f, struct Veo *v, long flags)
{
   unsigned char outBuf[2560];
   int i;
   png_structp png_ptr;
   png_infop info_ptr;

   png_ptr = png_create_write_struct
	 (PNG_LIBPNG_VER_STRING, png_voidp_NULL,
	  png_error_ptr_NULL, png_error_ptr_NULL);

   if (!png_ptr)
	 return;

   info_ptr = png_create_info_struct (png_ptr);
   if (!info_ptr)
	 {
		png_destroy_write_struct (&png_ptr, (png_infopp) NULL);
		return;
	 }

   if (setjmp (png_jmpbuf (png_ptr)))
	 {
		png_destroy_write_struct (&png_ptr, &info_ptr);
		fclose (f);
		return;
	 }

   png_init_io (png_ptr, f);

   png_set_IHDR (png_ptr, info_ptr, v->width, v->height,
				 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
				 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

   png_write_info (png_ptr, info_ptr);

   for (i = 0; i < v->height; i++)
	 {
		Gen24bitRow (flags, i, v, outBuf);
		png_write_row (png_ptr, outBuf);
		png_write_flush (png_ptr);
	 }

   png_write_end (png_ptr, info_ptr);
   png_destroy_write_struct (&png_ptr, &info_ptr);

}
#endif

/***********************************************************************
 *
 * Function:    write_ppm
 *
 * Summary:
 *
 * Parameters:  None
 *
 * Returns:     Nothing
 *
 ***********************************************************************/
void write_ppm (FILE * f, struct Veo *v, long flags)
{
   unsigned char outBuf[2560];
   int i;

   fprintf (f, "P6\n# ");

   if (v->name != NULL)
	 fprintf (f, "%s (created on %s)\n", v->name, fmt_date (v));

   fprintf (f, "%d %d\n255\n", v->width, v->height);

   for (i = 0; i < v->height; i++)
	 {
		Gen24bitRow (flags, i, v, outBuf);

		fwrite (outBuf, v->width * 3, 1, f);
	 }
}

/***********************************************************************
 *
 * Function:    WritePicture
 *
 * Summary:	FIXME
 *
 * Parameters:
 *
 * Returns:
 *
 ***********************************************************************/
void WritePicture (int sd, int db, int type, char *name, char *progname, long flags)
{
   char fname[FILENAME_MAX];
   FILE *f;
   char extension[8];
   static int i = 1, len;
   struct Veo v;
   unsigned char inBuf[2560];
   unsigned char outBuf[2560];
   int attr, category;

   if (type == VEO_OUT_PNG)
	 sprintf (extension, ".png");
   else if (type == VEO_OUT_PPM)
	 sprintf (extension, ".ppm");

   sprintf (fname, "%s", name);
   strcpy (v.name, name);

   protect_files (fname, extension);

   printf ("Generating %s...\n", fname);

   f = fopen (fname, "wb");

   if (f)
	 {
		if (sd)
		  {
			 len =
			   dlp_ReadRecordByIndex (sd, db, 0, inBuf, 0, 0, &attr, &category);
			 unpack_Veo (&v, inBuf, len);
			 v.sd = sd;
			 v.db = db;
		  }
		else
		  return;

		ColourCorrect (&v, redLUT, greenLUT, blueLUT, flags);

		if (type == VEO_OUT_PNG)
		  write_png (f, &v, flags);
		else if (type == VEO_OUT_PPM)
		  write_ppm (f, &v, flags);

		fclose (f);
	 }
   else
	 {
		fprintf (stderr, "%s: can't write to %s\n", progname, fname);
	 }

}

int main (int argc, char *argv[])
{
   int c,			/* switch */
	 db,
	 i = 0,
	 sd = -1, 
	 action = VEO_ACTION_OUTPUT, 
	 dbcount = 0, 
	 type = VEO_OUT_PPM,
	 bias = 50;
   long flags = 0;
   struct DBInfo info;

   char *progname = argv[0], *port = NULL, *picname = NULL;

   struct PilotUser User;
   unsigned char buffer[0xffff];

   while ((c = getopt_long (argc, argv, optstring, options, NULL)) != -1)
	 {
		switch (c)
		  {
		   case 'h':
			 display_help (progname);
			 return 0;
		   case 'v':
			 print_splash (progname);
			 return 0;
		   case 'p':
			 free (port);
			 port = strdup (optarg);
			 break;
		   case 'n':
			 action = VEO_ACTION_OUTPUT_ONE;
			 picname = optarg;
			 break;
		   case 'l':
			 action = VEO_ACTION_LIST;
			 break;
		   case 'b':
			 bias = atoi( optarg );
			 if( bias > 100 || bias < 0 )
			   {
  				  fprintf (stderr, "Bad bias\n");
				  exit( EXIT_FAILURE );
			   }
			 
			 bias_factor = (double)bias / 100.0;
			 flags |= VEO_BIAS;
			 break;
		   case 'c':
			 flags |= VEO_COLOUR_CORRECT;
			 break;
		   case 't':
			 if (!strncmp ("png", optarg, 3))
			   {
#ifdef HAVE_PNG
				  type = VEO_OUT_PNG;
#else
				  fprintf (stderr, "read-veo was built without png support\n");
#endif
			   }
			 else if (!strncmp ("ppm", optarg, 3))
			   {
				  type = VEO_OUT_PPM;
			   }
			 else
			   {
				  fprintf (stderr, "Unknown output type defaulting to ppm\n");
				  type = VEO_OUT_PPM;
			   }
			 break;
		  }
	 }

   sd = pilot_connect (port);

   if (sd < 0)
	 goto error;

   if (dlp_ReadUserInfo (sd, &User) < 0)
	 goto error_close;

   for (;;)
	 {
		if (dlp_ReadDBList (sd, 0, 0x80, i, &info) < 0)
		  break;
		i = info.index + 1;
		if (info.type == pi_mktag ('E', 'Z', 'V', 'I')
			&& info.creator == pi_mktag ('O', 'D', 'I', '2'))
		  {
			 dbcount++;
			 switch (action)
			   {
				case VEO_ACTION_LIST:
				  printf ("%s\n", info.name);
				  break;

				case VEO_ACTION_OUTPUT_ONE:
				  if (strcmp (info.name, picname))
					break;

				case VEO_ACTION_OUTPUT:
				  if (dlp_OpenDB (sd, 0, 0x80 | 0x40, info.name, &db) < 0)
					{
					   puts ("Unable to open Veo database");
					   dlp_AddSyncLogEntry (sd, "Unable to open Veo database.\n");
					   exit (EXIT_FAILURE);
					}

				  dlp_ReadAppBlock (sd, db, 0, buffer, 0xffff);

				  WritePicture (sd, db, type, info.name, progname, flags);


				  if (sd)
					{
					   /* Close the database */
					   dlp_CloseDB (sd, db);
					}

				  break;
			   }
		  }
	 }

   if (sd)
	 {
		dlp_AddSyncLogEntry (sd,
							 "Successfully read Veo photos from Palm.\n"
							 "Thank you for using pilot-link.");
		dlp_EndOfSync (sd, 0);
		pi_close (sd);
	 }

   printf ("\nList complete. %d files found.\n", dbcount);

   return 0;

error_close:
   pi_close (sd);

error:
   return -1;

}
