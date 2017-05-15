
/* This file is derived from (actually an earlier version of)... */

/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * $Id: tga.c,v 1.5 1998/05/26 08:46:36 yosh Exp $
 * TrueVision Targa loading and saving file filter for the Gimp.
 * Targa code Copyright (C) 1997 Raphael FRANCOIS and Gordon Matzigkeit
 *
 * The Targa reading and writing code was written from scratch by
 * Raphael FRANCOIS <fraph@ibm.net> and Gordon Matzigkeit
 * <gord@gnu.ai.mit.edu> based on the TrueVision TGA File Format
 * Specification, Version 2.0:
 *
 *   <URL:ftp://ftp.truevision.com/pub/TGA.File.Format.Spec/>
 *
 * It does not contain any code written for other TGA file loaders.
 * Not even the RLE handling. ;)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include stuff that should be in <GL/gl.h> that might not be in your version. */

/* EXT_bgra defines from <GL/gl.h> */
#ifndef GL_EXT_bgra
#define GL_BGR_EXT                          0x80E0
#define GL_BGRA_EXT                         0x80E1
#endif

#include "tga.h"

static char error[256];
static unsigned int verbose = 0;
static int totbytes = 0;

typedef struct {
  unsigned char *statebuf;
  int statelen;
  int laststate;
} RLEstate;

static int
std_fread(RLEstate *rleInfo, void *buf, size_t datasize, size_t nelems, FILE *fp)
{
  if (verbose > 1) {
    totbytes += nelems * datasize;
    printf("TGA: std_fread %d (total %d)\n",
      (int)nelems * datasize, totbytes);
  }
  return fread(buf, datasize, nelems, fp);
}

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define RLE_PACKETSIZE 0x80

/* Decode a bufferful of file. */
static int
rle_fread(RLEstate *rleInfo, void *vbuf, size_t datasize, size_t nelems, FILE *fp)
{

  unsigned char *buf = (unsigned char*) vbuf;
  int j, k;
  int buflen, count, bytes, curbytes;
  unsigned char *p;

  /* Scale the buffer length. */
  buflen = nelems * datasize;

  j = 0;
  curbytes = totbytes;
  while (j < buflen) {
    if (rleInfo->laststate < rleInfo->statelen) {
      /* Copy bytes from our previously decoded buffer. */
      bytes = MIN(buflen - j, rleInfo->statelen - rleInfo->laststate);
      memcpy(buf + j, rleInfo->statebuf + rleInfo->laststate, bytes);
      j += bytes;
      rleInfo->laststate += bytes;

      /* If we used up all of our state bytes, then reset them. */
      if (rleInfo->laststate >= rleInfo->statelen) {
        rleInfo->laststate = 0;
        rleInfo->statelen = 0;
      }

      /* If we filled the buffer, then exit the loop. */
      if (j >= buflen) break;
    }

    /* Decode the next packet. */
    count = fgetc(fp);
    if (count == EOF) {
      if (verbose) printf("TGA: hit EOF while looking for count\n");
      return j / datasize;
    }

    /* Scale the byte length to the size of the data. */
    bytes = ((count & ~RLE_PACKETSIZE) + 1) * datasize;

    if (j + bytes <= buflen) {
      /* We can copy directly into the image buffer. */
      p = buf + j;
    } else {
#ifdef PROFILE
      printf("TGA: needed to use statebuf for %d bytes\n", buflen - j);
#endif
      /* Allocate the state buffer if we haven't already. */
      if (!rleInfo->statebuf) {
        rleInfo->statebuf = (unsigned char *) malloc(RLE_PACKETSIZE * datasize);
      }
      p = rleInfo->statebuf;
    }

    if (count & RLE_PACKETSIZE) {
      /* Fill the buffer with the next value. */
      if (fread(p, datasize, 1, fp) != 1) {
        if (verbose) {
          printf("TGA: EOF while reading %d/%d element RLE packet\n",
            bytes, datasize);
        }
        return j / datasize;
      }

      /* Optimized case for single-byte encoded data. */
      if (datasize == 1) {
        memset(p + 1, *p, bytes - 1);
      } else {
        for (k = datasize; k < bytes; k += datasize) {
          memcpy(p + k, p, datasize);
        }
      }
    } else {
      /* Read in the buffer. */
      if (fread(p, bytes, 1, fp) != 1) {
        if (verbose) {
          printf("TGA: EOF while reading %d/%d element raw packet\n",
            bytes, datasize);
        }
        return j / datasize;
      }
    }

    if (verbose > 1) {
      totbytes += bytes;
      if (verbose > 2) {
        printf("TGA: %s packet %d/%d\n",
          (count & RLE_PACKETSIZE) ? "RLE" : "raw",
          bytes, totbytes);
      }
    }

    /* We may need to copy bytes from the state buffer. */
    if (p == rleInfo->statebuf) {
      rleInfo->statelen = bytes;
    } else {
      j += bytes;
    }
  }

  if (verbose > 1) {
    printf("TGA: rle_fread %d/%d (total %d)\n",
      nelems * datasize, totbytes - curbytes, totbytes);
  }
  return nelems;
}

gliGenericImage *
gliReadTGA(FILE *fp, char *name)
{
  TgaHeader tgaHeader;
  TgaFooter tgaFooter;
  char horzrev, vertrev;
  int width, height, bpp;
  int start, end, dir;
  int i, j, k;
  int pelbytes, wbytes;
  GLenum format;
  int components;
  RLEstate rleRec;
  RLEstate *rleInfo;
  int rle;
  int index, colors, length;
  GLubyte *cmap, *pixels, *data;
  int (*myfread)(RLEstate *rleInfo, void*, size_t, size_t, FILE*);
  gliGenericImage *genericImage;

  /* Check the footer. */
  if (fseek(fp, 0L - sizeof(tgaFooter), SEEK_END)
      || fread(&tgaFooter, sizeof(tgaFooter), 1, fp) != 1) {
    sprintf(error, "TGA: Cannot read footer from \"%s\"", name);
    if (verbose) printf("%s\n", error);
    return NULL;
  }  

  /* Check the signature. */
  if (memcmp(tgaFooter.signature, TGA_SIGNATURE,
             sizeof(tgaFooter.signature)) == 0) {
    if (verbose) printf("TGA: found New TGA\n");
  } else {
    if (verbose) printf("TGA: found Original TGA\n");
  }

  if (fseek(fp, 0, SEEK_SET) ||
      fread(&tgaHeader, sizeof(tgaHeader), 1, fp) != 1) {
    sprintf(error, "TGA: Cannot read header from \"%s\"", name);
    if (verbose) printf("%s\n", error);
    return NULL;
  }

  if (verbose && tgaHeader.idLength) {
    char *idString = (char*) malloc(tgaHeader.idLength);
    
    if (fread(idString, tgaHeader.idLength, 1, fp) != 1) {
      sprintf(error, "TGA: Cannot read ID field in \"%s\"", name);
      printf("%s\n", error);
    } else {
      printf("TGA: ID field: \"%*s\"\n", tgaHeader.idLength, idString);
    }
    free(idString);
  } else {
    /* Skip the image ID field. */
    if (tgaHeader.idLength && fseek(fp, tgaHeader.idLength, SEEK_CUR)) {
      sprintf(error, "TGA: Cannot skip ID field in \"%s\"", name);
      if (verbose) printf("%s\n", error);
      return NULL;
    }
  }
  
  /* Reassemble the multi-byte values correctly, regardless of
     host endianness. */
  width = (tgaHeader.widthHi << 8) | tgaHeader.widthLo;
  height = (tgaHeader.heightHi << 8) | tgaHeader.heightLo;
  bpp = tgaHeader.bpp;
  if (verbose) {
    printf("TGA: width=%d, height=%d, bpp=%d\n", width, height, bpp);
  }

  horzrev = tgaHeader.descriptor & TGA_DESC_HORIZONTAL;
  vertrev = !(tgaHeader.descriptor & TGA_DESC_VERTICAL);
  if (verbose && horzrev) printf("TGA: horizontal reversed\n");
  if (verbose && vertrev) printf("TGA: vertical reversed\n");

  rle = 0;
  switch (tgaHeader.imageType) {
  case TGA_TYPE_MAPPED_RLE:
    rle = 1;
    if (verbose) printf("TGA: run-length encoded\n");
  case TGA_TYPE_MAPPED:
    /* Test for alpha channel. */
    format = GL_COLOR_INDEX;
    components = 1;
    if (verbose) {
      printf("TGA: %d bit indexed image (%d bit palette)\n",
        tgaHeader.colorMapSize, bpp);
    }
    break;

  case TGA_TYPE_GRAY_RLE:
    rle = 1;
    if (verbose) printf("TGA: run-length encoded\n");
  case TGA_TYPE_GRAY:
    format = GL_LUMINANCE;
    components = 1;
    if (verbose) printf("TGA: %d bit grayscale image\n", bpp);
    break;

  case TGA_TYPE_COLOR_RLE:
    rle = 1;
    if (verbose) printf("TGA: run-length encoded\n");
  case TGA_TYPE_COLOR:
    /* Test for alpha channel. */
    if (bpp == 32) {
      format = GL_BGRA_EXT;
      components = 4;
      if (verbose) {
        printf("TGA: %d bit color image with alpha channel\n", bpp);
      }
    } else {
      format = GL_BGR_EXT;
      components = 3;
      if (verbose) printf("TGA: %d bit color image\n", bpp);
    }
    break;

  default:
    sprintf(error,
      "TGA: unrecognized image type %d\n", tgaHeader.imageType);
    if (verbose) printf("%s\n", error);
    return NULL;
  }

  if ((format == GL_BGRA_EXT && bpp != 32) ||
      (format == GL_BGR_EXT && bpp != 24) ||
      ((format == GL_LUMINANCE || format == GL_COLOR_INDEX) && bpp != 8)) {
    /* FIXME: We haven't implemented bit-packed fields yet. */
    sprintf(error, "TGA: channel sizes other than 8 bits are unimplemented");
    if (verbose) printf("%s\n", error);
    return NULL;
  }

  /* Check that we have a color map only when we need it. */
  if (format == GL_COLOR_INDEX) {
    if (tgaHeader.colorMapType != 1) {
      sprintf(error, "TGA: indexed image has invalid color map type %d\n",
        tgaHeader.colorMapType);
      if (verbose) printf("%s\n", error);
      return NULL;
    }
  } else if (tgaHeader.colorMapType != 0) {
    sprintf(error, "TGA: non-indexed image has invalid color map type %d\n",
      tgaHeader.colorMapType);
    if (verbose) printf("%s\n", error);
    return NULL;
  }

  if (tgaHeader.colorMapType == 1) {
    /* We need to read in the colormap. */
    index = (tgaHeader.colorMapIndexHi << 8) | tgaHeader.colorMapIndexLo;
    length = (tgaHeader.colorMapLengthHi << 8) | tgaHeader.colorMapLengthLo;

    if (verbose) {
      printf("TGA: reading color map (%d + %d) * (%d / 8)\n",
        index, length, tgaHeader.colorMapSize);
    }
    if (length == 0) {
      sprintf(error, "TGA: invalid color map length %d", length);
      if (verbose) printf("%s\n", error);
      return NULL;
    }
    if (tgaHeader.colorMapSize != 24) {
      /* We haven't implemented bit-packed fields yet. */
      sprintf(error, "TGA: channel sizes other than 8 bits are unimplemented");
      if (verbose) printf("%s\n", error);
      return NULL;
    }

    pelbytes = tgaHeader.colorMapSize / 8;
    colors = length + index;
    cmap = (unsigned char*) malloc (colors * pelbytes);

    /* Zero the entries up to the beginning of the map. */
    memset(cmap, 0, index * pelbytes);

    /* Read in the rest of the colormap. */
    if (fread(cmap, pelbytes, length, fp) != (size_t) length) {
      sprintf(error, "TGA: error reading colormap (ftell == %ld)\n",
        ftell (fp));
      if (verbose) printf("%s\n", error);
      return NULL;
    }

    if (pelbytes >= 3) {
      /* Rearrange the colors from BGR to RGB. */
      int tmp;
      for (j = index; j < length * pelbytes; j += pelbytes) {
        tmp = cmap[j];
        cmap[j] = cmap[j + 2];
        cmap[j + 2] = tmp;
      }
    }
  } else {
    colors = 0;
    cmap = NULL;
  }

  /* Allocate the data. */
  pelbytes = bpp / 8;
  pixels = (unsigned char *) malloc (width * height * pelbytes);

  if (rle) {
    rleRec.statebuf = 0;
    rleRec.statelen = 0;
    rleRec.laststate = 0;
    rleInfo = &rleRec;
    myfread = rle_fread;
  } else {
    rleInfo = NULL;
    myfread = std_fread;
  }

  wbytes = width * pelbytes;

  if (vertrev) {
    start = 0;
    end = height;
    dir = 1;
  } else {
    /* We need to reverse loading order of rows. */
    start = height-1;
    end = -1;
    dir = -1;
  }

  for (i = start; i != end; i += dir) {
    data = pixels + i*wbytes;

    /* Suck in the data one row at a time. */
    if (myfread(rleInfo, data, pelbytes, width, fp) != width) {
      /* Probably premature end of file. */
      if (verbose) {
        printf ("TGA: error reading (ftell == %ld, width=%d)\n",
          ftell(fp), width);
      }
      return NULL;
    }  

    if (horzrev) {
      /* We need to mirror row horizontally. */
      for (j = 0; j < width/2; j++) {
        GLubyte tmp;

        for (k = 0; k < pelbytes; k++) {
          tmp = data[j*pelbytes+k];
          data[j*pelbytes+k] = data[(width-j-1)*pelbytes+k];
          data[(width-j-1)*pelbytes+k] = tmp;
        }
      }
    }
  }

  if (rle) {
    free(rleInfo->statebuf);
  }

  if (fgetc (fp) != EOF) {
    if (verbose) printf ("TGA: too much input data, ignoring extra...\n");
  }

  genericImage = (gliGenericImage*) malloc(sizeof(gliGenericImage));
  genericImage->width = width;
  genericImage->height = height;
  genericImage->format = format;
  genericImage->components = components;
  genericImage->cmapEntries = colors;
  genericImage->cmapFormat = GL_BGR_EXT;  // XXX fix me
  genericImage->cmap = cmap;
  genericImage->pixels = pixels;

  return genericImage;
}

int
gliVerbose(int newVerbose)
{
  int oldVerbose = verbose;
  verbose = newVerbose;
  return verbose;
}
