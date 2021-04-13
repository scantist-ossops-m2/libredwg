/*****************************************************************************/
/*  LibreDWG - free implementation of the DWG file format                    */
/*                                                                           */
/*  Copyright (C) 2013, 2018-2020, 2023 Free Software Foundation, Inc.       */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 3 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/

/*
 * reedsolomon.c: Reed-Solomon (255,239) en-,decoding
 * written by Alex Papazoglou
 * needs C99
 */

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#  include <malloc.h>
#endif
#include "common.h"
#include "logging.h"
#include "common.h"
#include "reedsolomon.h"

#define debug(fmt, ...)                                                       \
  HANDLER (OUTPUT, "%s:%d: %s() - " fmt, __FILE__, __LINE__, __func__,        \
           ##__VA_ARGS__)
#define POLY_LENGTH 32
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

typedef unsigned char *Poly;
typedef Poly *PolyRow;
typedef PolyRow *PolyMatrix;

/*
 * Private functions
 */
static int degree (Poly);
static void rowop (PolyMatrix, int, int);
static int fix_errors (unsigned char *, unsigned char *, unsigned char *);
static void solve_key_equation (unsigned char *, unsigned char *,
                                unsigned char *);
static unsigned char evaluate (Poly, int, unsigned char);
static void free_matrix (PolyMatrix);
static PolyMatrix initialize_matrix (unsigned char *);
static unsigned char f256_multiply (unsigned char, unsigned char);
static void dump_syndrome (unsigned char *);

/*
 * Tables for dealing with the finite field. The specific representation
 * of F_256 in this case is the residue field of
 *
 *     X^8 + X^6 + X^5 + X^4 + 1      (0x0169 hex)
 *
 * which is primitive (e.g. X is a generator for the multiplicative group).
 */
static const unsigned char f256_inverse[] = {
  0, /* not defined */
  0x01, 0xb4, 0xd8, 0x5a, 0x48, 0x6c, 0x78, 0x2d, 0x28, 0x24, 0xa4, 0x36, 0xc9,
  0x3c, 0x38, 0xa2, 0xcf, 0x14, 0x6a, 0x12, 0x6b, 0x52, 0x47, 0x1b, 0xd1, 0xd0,
  0x18, 0x1e, 0xbb, 0x1c, 0xba, 0x51, 0xb1, 0xd3, 0xa6, 0x0a, 0xa5, 0x35, 0x69,
  0x09, 0x2c, 0x81, 0x4f, 0x29, 0x08, 0x97, 0x67, 0xb9, 0xca, 0xdc, 0x45, 0x68,
  0x26, 0x0c, 0xc8, 0x0f, 0x3d, 0xe9, 0x61, 0x0e, 0x39, 0x5d, 0xfe, 0x9c, 0x8d,
  0xec, 0xe1, 0xdd, 0x33, 0x53, 0x17, 0x05, 0x5b, 0xe6, 0xcc, 0xae, 0xf8, 0x80,
  0x2b, 0xb0, 0x20, 0x16, 0x46, 0xf4, 0xe4, 0x93, 0x98, 0xa0, 0x9f, 0x04, 0x49,
  0xff, 0x3e, 0x87, 0xc5, 0xe8, 0x3b, 0x65, 0x6f, 0x6e, 0x62, 0x96, 0x2f, 0x34,
  0x27, 0x13, 0x15, 0x06, 0x79, 0x64, 0x63, 0xb3, 0xfa, 0xaa, 0x94, 0xc0, 0xeb,
  0x84, 0xdf, 0x07, 0x6d, 0xa8, 0xc3, 0x9a, 0xe2, 0x7f, 0x7e, 0x4e, 0x2a, 0xf2,
  0xc6, 0x76, 0xde, 0xc4, 0x5e, 0xda, 0xef, 0xad, 0xfc, 0x9d, 0x41, 0xbf, 0xd7,
  0xb6, 0xf1, 0x99, 0x56, 0x73, 0xab, 0x66, 0x2e, 0x57, 0x92, 0x7c, 0xe3, 0x40,
  0x8c, 0xa1, 0x59, 0x58, 0x9e, 0x10, 0xce, 0x0b, 0x25, 0x23, 0xd2, 0x7a, 0xc2,
  0x72, 0x95, 0xfd, 0x8a, 0x4c, 0xf9, 0x50, 0x21, 0xfb, 0x70, 0x02, 0xd9, 0x90,
  0xf0, 0xcb, 0x30, 0x1f, 0x1d, 0xf7, 0xd4, 0xd6, 0x8e, 0x74, 0xea, 0xa9, 0x7b,
  0x86, 0x5f, 0x83, 0xf3, 0x37, 0x0d, 0x31, 0xb8, 0x4b, 0xe7, 0xa3, 0x11, 0x1a,
  0x19, 0xa7, 0x22, 0xbd, 0xf6, 0xbe, 0x8f, 0x03, 0xb5, 0x88, 0xee, 0x32, 0x44,
  0x85, 0x77, 0xed, 0x43, 0x7d, 0x9b, 0x55, 0xf5, 0x4a, 0xcd, 0x60, 0x3a, 0xc1,
  0x75, 0x42, 0xe0, 0xdb, 0x89, 0xb7, 0x91, 0x82, 0xc7, 0x54, 0xe5, 0xd5, 0xbc,
  0x4d, 0xaf, 0x71, 0xb2, 0x8b, 0xac, 0x3f, 0x5c,
};

const unsigned char f256_power[] = {
  0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x69, 0xd2, 0xcd, 0xf3, 0x8f,
  0x77, 0xee, 0xb5, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0xe9, 0xbb, 0x1f,
  0x3e, 0x7c, 0xf8, 0x99, 0x5b, 0xb6, 0x05, 0x0a, 0x14, 0x28, 0x50, 0xa0, 0x29,
  0x52, 0xa4, 0x21, 0x42, 0x84, 0x61, 0xc2, 0xed, 0xb3, 0x0f, 0x1e, 0x3c, 0x78,
  0xf0, 0x89, 0x7b, 0xf6, 0x85, 0x63, 0xc6, 0xe5, 0xa3, 0x2f, 0x5e, 0xbc, 0x11,
  0x22, 0x44, 0x88, 0x79, 0xf2, 0x8d, 0x73, 0xe6, 0xa5, 0x23, 0x46, 0x8c, 0x71,
  0xe2, 0xad, 0x33, 0x66, 0xcc, 0xf1, 0x8b, 0x7f, 0xfe, 0x95, 0x43, 0x86, 0x65,
  0xca, 0xfd, 0x93, 0x4f, 0x9e, 0x55, 0xaa, 0x3d, 0x7a, 0xf4, 0x81, 0x6b, 0xd6,
  0xc5, 0xe3, 0xaf, 0x37, 0x6e, 0xdc, 0xd1, 0xcb, 0xff, 0x97, 0x47, 0x8e, 0x75,
  0xea, 0xbd, 0x13, 0x26, 0x4c, 0x98, 0x59, 0xb2, 0x0d, 0x1a, 0x34, 0x68, 0xd0,
  0xc9, 0xfb, 0x9f, 0x57, 0xae, 0x35, 0x6a, 0xd4, 0xc1, 0xeb, 0xbf, 0x17, 0x2e,
  0x5c, 0xb8, 0x19, 0x32, 0x64, 0xc8, 0xf9, 0x9b, 0x5f, 0xbe, 0x15, 0x2a, 0x54,
  0xa8, 0x39, 0x72, 0xe4, 0xa1, 0x2b, 0x56, 0xac, 0x31, 0x62, 0xc4, 0xe1, 0xab,
  0x3f, 0x7e, 0xfc, 0x91, 0x4b, 0x96, 0x45, 0x8a, 0x7d, 0xfa, 0x9d, 0x53, 0xa6,
  0x25, 0x4a, 0x94, 0x41, 0x82, 0x6d, 0xda, 0xdd, 0xd3, 0xcf, 0xf7, 0x87, 0x67,
  0xce, 0xf5, 0x83, 0x6f, 0xde, 0xd5, 0xc3, 0xef, 0xb7, 0x07, 0x0e, 0x1c, 0x38,
  0x70, 0xe0, 0xa9, 0x3b, 0x76, 0xec, 0xb1, 0x0b, 0x16, 0x2c, 0x58, 0xb0, 0x09,
  0x12, 0x24, 0x48, 0x90, 0x49, 0x92, 0x4d, 0x9a, 0x5d, 0xba, 0x1d, 0x3a, 0x74,
  0xe8, 0xb9, 0x1b, 0x36, 0x6c, 0xd8, 0xd9, 0xdb, 0xdf, 0xd7, 0xc7, 0xe7, 0xa7,
  0x27, 0x4e, 0x9c, 0x51, 0xa2, 0x2d, 0x5a, 0xb4,
};

static const unsigned char f256_residue[] = {
  0x00, 0x69, 0xd2, 0xbb, 0xcd, 0xa4, 0x1f, 0x76, 0xf3, 0x9a, 0x21, 0x48, 0x3e,
  0x57, 0xec, 0x85, 0x8f, 0xe6, 0x5d, 0x34, 0x42, 0x2b, 0x90, 0xf9, 0x7c, 0x15,
  0xae, 0xc7, 0xb1, 0xd8, 0x63, 0x0a, 0x77, 0x1e, 0xa5, 0xcc, 0xba, 0xd3, 0x68,
  0x01, 0x84, 0xed, 0x56, 0x3f, 0x49, 0x20, 0x9b, 0xf2, 0xf8, 0x91, 0x2a, 0x43,
  0x35, 0x5c, 0xe7, 0x8e, 0x0b, 0x62, 0xd9, 0xb0, 0xc6, 0xaf, 0x14, 0x7d, 0xee,
  0x87, 0x3c, 0x55, 0x23, 0x4a, 0xf1, 0x98, 0x1d, 0x74, 0xcf, 0xa6, 0xd0, 0xb9,
  0x02, 0x6b, 0x61, 0x08, 0xb3, 0xda, 0xac, 0xc5, 0x7e, 0x17, 0x92, 0xfb, 0x40,
  0x29, 0x5f, 0x36, 0x8d, 0xe4, 0x99, 0xf0, 0x4b, 0x22, 0x54, 0x3d, 0x86, 0xef,
  0x6a, 0x03, 0xb8, 0xd1, 0xa7, 0xce, 0x75, 0x1c, 0x16, 0x7f, 0xc4, 0xad, 0xdb,
  0xb2, 0x09, 0x60, 0xe5, 0x8c, 0x37, 0x5e, 0x28, 0x41, 0xfa, 0x93, 0xb5, 0xdc,
  0x67, 0x0e, 0x78, 0x11, 0xaa, 0xc3, 0x46, 0x2f, 0x94, 0xfd, 0x8b, 0xe2, 0x59,
  0x30, 0x3a, 0x53, 0xe8, 0x81, 0xf7, 0x9e, 0x25, 0x4c, 0xc9, 0xa0, 0x1b, 0x72,
  0x04, 0x6d, 0xd6, 0xbf, 0xc2, 0xab, 0x10, 0x79, 0x0f, 0x66, 0xdd, 0xb4, 0x31,
  0x58, 0xe3, 0x8a, 0xfc, 0x95, 0x2e, 0x47, 0x4d, 0x24, 0x9f, 0xf6, 0x80, 0xe9,
  0x52, 0x3b, 0xbe, 0xd7, 0x6c, 0x05, 0x73, 0x1a, 0xa1, 0xc8, 0x5b, 0x32, 0x89,
  0xe0, 0x96, 0xff, 0x44, 0x2d, 0xa8, 0xc1, 0x7a, 0x13, 0x65, 0x0c, 0xb7, 0xde,
  0xd4, 0xbd, 0x06, 0x6f, 0x19, 0x70, 0xcb, 0xa2, 0x27, 0x4e, 0xf5, 0x9c, 0xea,
  0x83, 0x38, 0x51, 0x2c, 0x45, 0xfe, 0x97, 0xe1, 0x88, 0x33, 0x5a, 0xdf, 0xb6,
  0x0d, 0x64, 0x12, 0x7b, 0xc0, 0xa9, 0xa3, 0xca, 0x71, 0x18, 0x6e, 0x07, 0xbc,
  0xd5, 0x50, 0x39, 0x82, 0xeb, 0x9d, 0xf4, 0x4f, 0x26,
};

const unsigned char f256_logarithm[] = {
  0x00, /* not defined */
  0x00, 0x01, 0x10, 0x02, 0x20, 0x11, 0xcc, 0x03, 0xdc, 0x21, 0xd7, 0x12, 0x7d,
  0xcd, 0x30, 0x04, 0x40, 0xdd, 0x77, 0x22, 0x99, 0xd8, 0x8d, 0x13, 0x91, 0x7e,
  0xec, 0xce, 0xe7, 0x31, 0x19, 0x05, 0x29, 0x41, 0x4a, 0xde, 0xb6, 0x78, 0xf7,
  0x23, 0x26, 0x9a, 0xa1, 0xd9, 0xfc, 0x8e, 0x3d, 0x14, 0xa4, 0x92, 0x50, 0x7f,
  0x87, 0xed, 0x6b, 0xcf, 0x9d, 0xe8, 0xd3, 0x32, 0x62, 0x1a, 0xa9, 0x06, 0xb9,
  0x2a, 0x58, 0x42, 0xaf, 0x4b, 0x72, 0xdf, 0xe1, 0xb7, 0xad, 0x79, 0xe3, 0xf8,
  0x5e, 0x24, 0xfa, 0x27, 0xb4, 0x9b, 0x60, 0xa2, 0x85, 0xda, 0x7b, 0xfd, 0x1e,
  0x8f, 0xe5, 0x3e, 0x97, 0x15, 0x2c, 0xa5, 0x39, 0x93, 0x5a, 0x51, 0xc2, 0x80,
  0x08, 0x88, 0x66, 0xee, 0xbb, 0x6c, 0xc6, 0xd0, 0x4d, 0x9e, 0x47, 0xe9, 0x74,
  0xd4, 0x0d, 0x33, 0x44, 0x63, 0x36, 0x1b, 0xb1, 0xaa, 0x55, 0x07, 0x65, 0xba,
  0xc5, 0x2b, 0x38, 0x59, 0xc1, 0x43, 0x35, 0xb0, 0x54, 0x4c, 0x46, 0x73, 0x0c,
  0xe0, 0xac, 0xe2, 0x5d, 0xb8, 0x57, 0xae, 0x71, 0x7a, 0x1d, 0xe4, 0x96, 0xf9,
  0xb3, 0x5f, 0x84, 0x25, 0xa0, 0xfb, 0x3c, 0x28, 0x49, 0xb5, 0xf6, 0x9c, 0xd2,
  0x61, 0xa8, 0xa3, 0x4f, 0x86, 0x6a, 0xdb, 0xd6, 0x7c, 0x2f, 0xfe, 0x0f, 0x1f,
  0xcb, 0x90, 0xeb, 0xe6, 0x18, 0x3f, 0x76, 0x98, 0x8c, 0x16, 0x8a, 0x2d, 0xc9,
  0xa6, 0x68, 0x3a, 0xf4, 0x94, 0x82, 0x5b, 0x6f, 0x52, 0x0a, 0xc3, 0xbf, 0x81,
  0x6e, 0x09, 0xbe, 0x89, 0xc8, 0x67, 0xf3, 0xef, 0xf0, 0xbc, 0xf1, 0x6d, 0xbd,
  0xc7, 0xf2, 0xd1, 0xa7, 0x4e, 0x69, 0x9f, 0x3b, 0x48, 0xf5, 0xea, 0x17, 0x75,
  0x8b, 0xd5, 0x2e, 0x0e, 0xca, 0x34, 0x53, 0x45, 0x0b, 0x64, 0xc4, 0x37, 0xc0,
  0x1c, 0x95, 0xb2, 0x83, 0xab, 0x5c, 0x56, 0x70
};

/*
 * The generator polynomial
 */
static const unsigned char rsgen[] = {
  0x6a, 0xe3, 0x63, 0x1f, 0xa1, 0x24, 0x9e, 0x44, 0x13,
  0x1e, 0x2f, 0xfc, 0xfd, 0xce, 0xa9, 0xdb, 0x01,
};

static unsigned char
f256_multiply (unsigned char a, unsigned char b)
{
  unsigned int prod = 0;
  unsigned int A = a;
  unsigned int B = b;

  while (B != 0)
    {
      if (B & 1)
        prod ^= A;

      B >>= 1;
      A <<= 1;
    }

  prod ^= f256_residue[prod >> 8];
  return prod & 0xff;
}

/*
 * Encode a block. Only the trailing 16 parity bytes are computed in
 * a buffer which caller pREALLOCates.
 */
void
rs_encode_block (unsigned char *parity, unsigned char *src, int count)
{
  int i, j;
  unsigned char leader, coeff;

  memset (parity, 0, 16);

  i = count;
  /* Horner's method / long division */
  while (i--)
    {
      leader = parity[15];
      for (j = 15; j > 0; j--)
        parity[j] = parity[j - 1] ^ f256_multiply (leader, rsgen[j]);

      parity[0] = src[i] ^ f256_multiply (leader, rsgen[0]);
    }

  for (i = 16; i > 0; i--)
    {
      leader = parity[15];
      for (j = 15; j > 0; j--)
        parity[j] = parity[j - 1] ^ f256_multiply (leader, rsgen[j]);
      parity[0] = f256_multiply (leader, rsgen[0]);
    }
}

/*
 * Decode a 255-byte block in place. Returns number of corrected errors,
 * if fix is set, or -1 in case of unfixed errors.
 */
int
rs_decode_block (unsigned char *blk, int fix)
{
  int i, j;
  unsigned char leader;
  unsigned char *synbuf, *sigma, *omega;
  int errflag = 0;

  synbuf = (unsigned char *)MALLOC (POLY_LENGTH);
  memset (synbuf, 0, 16);

  for (j = 0; j < 16; j++)
    {
      synbuf[j] = evaluate (blk, 255, f256_power[j + 1]);
      if (synbuf[j] != 0)
        errflag = 1;
    }

  if (!errflag)
    {
      debug ("No error in Reed-Solomon block\n");
      FREE (synbuf);
      return 0;
    }

  debug ("Errors detected in Reed-Solomon block\n");
  dump_syndrome (synbuf);

  if (!fix)
    {
      FREE (synbuf);
      return -1;
    }

  sigma = (unsigned char *)MALLOC (POLY_LENGTH);
  memset (sigma, 0, POLY_LENGTH);

  omega = (unsigned char *)MALLOC (POLY_LENGTH);
  memset (omega, 0, POLY_LENGTH);

  solve_key_equation (synbuf, sigma, omega);
  i = fix_errors (blk, sigma, omega);

  FREE (sigma);
  FREE (omega);
  FREE (synbuf);
  if (i < 0)
    debug ("Errors in Reed-Solomon block are not recoverable\n");
  else
    debug ("Fixed errors in Reed-Solomon block: %d\n", i);

  return i;
}

static int
degree (Poly poly)
{
  int i;
  i = POLY_LENGTH - 1;

  while (i--)
    if (poly[i] != 0)
      break;
  return i;
}

static void
rowop (PolyMatrix matrix, int dst, int src)
{
  int dstd, srcd;
  int limit;
  int power;
  unsigned char coeff;
  int i, j, k, l;

  dstd = degree (matrix[dst][2]); // 0..31
  srcd = degree (matrix[src][2]); // 0..31
  power = dstd - srcd;            // -31..31

  coeff = f256_inverse[matrix[src][2][srcd]];
  coeff = f256_multiply (coeff, matrix[dst][2][dstd]);

  /* row operation */
  GCC14_DIAG_IGNORE (-Wanalyzer-possible-null-dereference)
  for (j = 0; j < 3; j++)
    {
      limit = MIN (17 - power, POLY_LENGTH); // -14..48
      for (i = 0; i < limit; i++)
        {
          // i + power: -31..78
          matrix[dst][j][i + power]
              ^= f256_multiply (coeff, matrix[src][j][i]);
        }
    }
  GCC14_DIAG_RESTORE
}

static PolyMatrix
initialize_matrix (unsigned char *s)
{
  int deg;
  PolyMatrix matrix;
  int i, j;

  matrix = (PolyMatrix)CALLOC (2, sizeof (PolyRow));
  if (!matrix)
    return NULL;
  for (i = 0; i < 2; i++)
    {
      matrix[i] = (PolyRow)CALLOC (3, sizeof (Poly));
      if (!matrix[i])
        {
          free_matrix (matrix);
          return NULL;
        }
      for (j = 0; j < 3; j++)
        {
          matrix[i][j] = (Poly)CALLOC (1, POLY_LENGTH);
          if (!matrix[i][j])
            {
              free_matrix (matrix);
              return NULL;
            }
        }
    }

  /* Initialize matrix */
  matrix[0][0][0] = 1;
  memcpy (matrix[0][2], s, 16);

  matrix[1][1][0] = 1;
  matrix[1][2][16] = 1;

  return matrix;
}

static void
free_matrix (PolyMatrix matrix)
{
  int i, j;

  for (i = 0; i < 2; i++)
    {
      for (j = 0; j < 2; j++)
        {
          GCC14_DIAG_IGNORE (-Wanalyzer-null-dereference)
          if (matrix[i][j])
            FREE (matrix[i][j]);
          GCC14_DIAG_RESTORE
        }
      GCC14_DIAG_IGNORE (-Wanalyzer-malloc-leak)
      if (matrix[i])
        FREE (matrix[i]);
      GCC14_DIAG_RESTORE
    }
  FREE (matrix);
}

static unsigned char
evaluate (Poly poly, int deg, unsigned char x)
{
  unsigned char y;

  y = poly[deg];
  while (deg >= 0)
    {
      y = f256_multiply (x, y) ^ poly[deg];
      deg--;
    }
  return y;
}

static void
solve_key_equation (unsigned char *s, unsigned char *sigma,
                    unsigned char *omega)
{
  int fixed_row;
  PolyMatrix matrix;
  unsigned char leader0, leader1;
  int x, i;

  matrix = initialize_matrix (s);
  if (!matrix)
    return;
  fixed_row = 0;

  while (degree (matrix[fixed_row][0]) > 8
         || degree (matrix[fixed_row][2]) > 7)
    {
      if (degree (matrix[0][2]) < degree (matrix[1][2]))
        {
          fixed_row = 0;
          rowop (matrix, 1, 0);
        }
      else
        {
          fixed_row = 1;
          rowop (matrix, 0, 1);
        }
    }

  memcpy (sigma, matrix[fixed_row][0], POLY_LENGTH);
  memcpy (omega, matrix[fixed_row][2], POLY_LENGTH);

  free_matrix (matrix);
}

static int
fix_errors (unsigned char *blk, unsigned char *sigma, unsigned char *omega)
{
  int i, j, x, d;
  int nerr;
  unsigned char errorval;
  unsigned char roots[8];
  unsigned char chaff;

  nerr = 0;

  for (x = 0; x < 256; x++)
    {
      if (evaluate (sigma, 8, x) == 0)
        roots[nerr++] = x;
    }

  d = degree (sigma);
  if (nerr != d)
    return -1; /* if sigma doesn't split, the error is not recoverable */

  /* differentiate sigma in place*/
  for (i = 0; i < 8; i++)
    {
      sigma[2 * i] = sigma[2 * i + 1];
      sigma[2 * i + 1] = 0;
    }

  for (i = 0; i < nerr; i++)
    {
      int pos;
      unsigned char sigmaval = evaluate (sigma, 16, roots[i]);
      unsigned char omegaval = evaluate (omega, 16, roots[i]);

      chaff = f256_inverse[sigmaval];
      errorval = f256_multiply (chaff, omegaval);
      pos = f256_logarithm[f256_inverse[roots[i]]];

      blk[pos] ^= errorval;
    }
  return nerr;
}

/*
 * Some debugging code, for developer use.
 */
#ifdef DEBUG
static void
debug_row (PolyRow row)
{
  int j, k;
  for (j = 0; j < 4; j++)
    {
      for (k = 0; k < 3; k++)
        {
          HANDLER (OUTPUT, " %02x %02x %02x %02x ", row[k][4 * j + 0],
                   row[k][4 * j + 1], row[k][4 * j + 2], row[k][4 * j + 3]);
          if (k != 2)
            {
              HANDLER (OUTPUT, " | ");
            }
        }
      HANDLER (OUTPUT, "\n");
    }
  HANDLER (OUTPUT, " %02x           |  %02x           |  %02x \n", row[0][16],
           row[1][16], row[2][16]);
}

static void
dump_matrix (PolyMatrix matrix)
{
  debug ("Matrix:\n");
  debug_row (matrix[0]);
  HANDLER (OUTPUT, "\n");
  debug_row (matrix[1]);
}

#endif

static void
dump_syndrome (unsigned char *s)
{
  int i;
  debug ("Syndrome: ");
  for (i = 0; i < 16; i++)
    {
      if (i)
        HANDLER (OUTPUT, "-");
      HANDLER (OUTPUT, "%02x", s[i]);
    }
  HANDLER (OUTPUT, "\n");
}
