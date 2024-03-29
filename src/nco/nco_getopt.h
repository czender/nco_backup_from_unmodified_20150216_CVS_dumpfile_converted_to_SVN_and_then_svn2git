/* $Header: /data/zender/nco_20150216/nco/src/nco/nco_getopt.h,v 1.2 2003-01-09 01:05:05 zender Exp $ */

/* "my_getopt" package is "drop-in" replacement for GNU getopt() 
   by Benjamin Sittler <bsittler@iname.com> downloaded from
   http://www.geocities.com/ResearchTriangle/Node/9405/#my_getopt
   It is distributed under BSD-like license. 
   Modifications:
   20030101: Downloaded source
   20030108: Renamed my_getopt.h, my_getopt.c to nco_getopt.c, nco_getopt.h */

/* Original, unmodified license header: */
/*
 *  my_getopt.h - interface to my re-implementation of getopt.
 *  Copyright 1997, 2000, 2001, 2002, Benjamin Sittler
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

#ifndef MY_GETOPT_H_INCLUDED
#define MY_GETOPT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/* UNIX-style short-argument parser */
extern int my_getopt(int argc, char * argv[], const char *opts);

extern int my_optind, my_opterr, my_optopt;
extern char *my_optarg;

struct option {
  const char *name;
  int has_arg;
  int *flag;
  int val;
};

/* human-readable values for has_arg */
#undef no_argument
#define no_argument 0
#undef required_argument
#define required_argument 1
#undef optional_argument
#define optional_argument 2

/* GNU-style long-argument parsers */
extern int my_getopt_long(int argc, char * argv[], const char *shortopts,
                       const struct option *longopts, int *longind);

extern int my_getopt_long_only(int argc, char * argv[], const char *shortopts,
                            const struct option *longopts, int *longind);

extern int _my_getopt_internal(int argc, char * argv[], const char *shortopts,
                            const struct option *longopts, int *longind,
                            int long_only);

#undef getopt
#define getopt my_getopt
#undef getopt_long
#define getopt_long my_getopt_long
#undef getopt_long_only
#define getopt_long_only my_getopt_long_only
#undef _getopt_internal
#define _getopt_internal _my_getopt_internal
#undef opterr
#define opterr my_opterr
#undef optind
#define optind my_optind
#undef optopt
#define optopt my_optopt
#undef optarg
#define optarg my_optarg


#ifdef __cplusplus
}
#endif

#endif /* MY_GETOPT_H_INCLUDED */
