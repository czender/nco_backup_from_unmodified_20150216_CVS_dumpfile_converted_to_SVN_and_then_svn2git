/* $Header: /data/zender/nco_20150216/nco/src/nco/nco_cln_utl.h,v 1.42 2015-02-04 02:44:45 zender Exp $ */

/* Purpose: Calendar utilities */

/* Copyright (C) 1995--2015 Charlie Zender
   This file is part of NCO, the netCDF Operators. NCO is free software.
   You may redistribute and/or modify NCO under the terms of the 
   GNU General Public License (GPL) Version 3 with exceptions described in the LICENSE file */

/* Usage:
   #include "nco_cln_utl.h" *//* Calendar utilities */

#ifndef NCO_CLN_UTL_H
#define NCO_CLN_UTL_H

#ifdef HAVE_CONFIG_H
# include <config.h> /* Autotools tokens */
#endif /* !HAVE_CONFIG_H */

/* Standard header files */
#include <ctype.h> /* isalnum(), isdigit(), tolower() */
#include <stdio.h> /* stderr, FILE, NULL, printf */
#include <stdlib.h> /* strtod, strtol, malloc, getopt, exit */
#include <string.h> /* strcmp() */
#ifdef HAVE_STRINGS_H
# include <strings.h> /* strcasecmp() */
#endif /* !HAVE_STRINGS_H */

/* 3rd party vendors */
#ifdef ENABLE_UDUNITS
# ifdef HAVE_UDUNITS2_H
#  include <udunits2.h> /* Unidata units library */
# else
#  include <udunits.h> /* Unidata units library */
# endif /* !HAVE_UDUNITS2_H */
#endif /* !ENABLE_UDUNITS */

/* Personal headers */
#include "nco.h" /* netCDF Operator (NCO) definitions */
#include "nco_ctl.h" /* Program flow control functions */
#include "nco_sng_utl.h" /* String utilities */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Calendar types */
typedef enum {
  tm_year=1,    
  tm_month,
  tm_day,
  tm_hour,
  tm_min,
  tm_sec,
  tm_void  /* No time units matched */  
} tm_typ;

typedef struct {
  tm_typ sc_typ;
  nco_cln_typ sc_cln;
  int year;
  int month;
  int day;
  int hour;
  int min;
  float sec;
  double value;
} tm_cln_sct;

int /* O [nbr] Number of days to end of month */
nco_nd2endm /* [fnc] Compute number of days to end of month */
(const int mth, /* I [mth] Month */
 const int day); /* I [day] Current day */

nco_int /* O [YYMMDD] Date a specified number of days from input date */
nco_newdate /* [fnc] Compute date a specified number of days from input date */
(const nco_int date, /* I [YYMMDD] Date */
 const nco_int day_srt); /* I [day] Days ahead of input date */

int               /* [flg] NCO_NOERR or NCO_ERR */ 
nco_cln_clc_dff( /* [fnc] difference between two co-ordinate units */
const char *fl_unt_sng, /* I [ptr] units attribute string from disk */
const char *fl_bs_sng,  /* I [ptr] units attribute string from disk */
double crr_val, /* I [dbl] input units value */
double *rgn_val); /* O difference between two units string */

int /* [flg] NCO_NOERR or NCO_ERR */
nco_cln_prs_tm( /* Extract time stamp from a parsed udunits string */
const char *unt_sng, /* I [ptr] units attribute string */  
tm_cln_sct *tm_in); /*  O [sct] struct to be populated */

tm_typ /* [enum] Units type */
nco_cln_get_tm_typ( /* returns time unit type or tm_void if not found */
const char *ud_sng); /* I [ptr] units string */

nco_cln_typ /* [enum] Calendar type */    
nco_cln_get_cln_typ( /* [fnc] Calendar type or cln_nil if not found */
const char *ud_sng); /* I [ptr] units string */

int /* O [int] number of days */
nco_cln_days_in_year_prior_to_given_month( /* [fnc] Number of days in year prior to month */
nco_cln_typ lmt_cln, /* [enum] calendar type */
int mth_idx); /* I [idx] Month (1-based counting, December == 12) */

void
nco_cln_pop_val( /* [fnc] Calculate value in cln_sct */ 
tm_cln_sct *cln_sct);/* I/O [ptr] Calendar structure */

double /* O [dbl] Relative time */
nco_cln_rel_val( /* [fnc] */
double offset, /* I [dbl] time in base units */
nco_cln_typ lmt_cln, /* I [enum] Calendar type */
tm_typ bs_tm_typ); /* I [enum] Time units */

int /* O [flg] NCO_NOERR or NCO_ERR */ 
nco_cln_clc_tm( /* [fnc] Difference between two time coordinate units */
const char *fl_unt_sng, /* I [ptr] user units attribute string */
const char *fl_bs_sng, /* I [ptr] units attribute string from disk  */     
nco_cln_typ lmt_cln, /* [enm] Calendar type of coordinate var */ 
double *rgn_val); /* O [ptr] time diff in units based on fl_bs_sng */ 

int /* O [flg] NCO_NOERR or NCO_ERR */ 
nco_cln_clc_org( /* [fnc] Difference between two generic coordinate units */      
const char *fl_unt_sng, /* I [ptr] units attribute string from disk */
const char *fl_bs_sng, /* I [ptr] units attribute string from disk */
nco_cln_typ lmt_cln, /* I [enum] Calendar type of coordinate var */ 
double *og_val); /* O [ptr] */

int /* O [flg] String is calendar date */
nco_cln_chk_tm /* [fnc] Is string a UDUnits-compatible calendar format, e.g., "PERIOD since REFERENCE_DATE" */
(const char *unit_sng); /* I [sng] Units string */

int /* [rcd] Return code */
nco_cln_sng_rbs /* [fnc] Rebase calendar string for legibility */
(const ptr_unn val, /* I [sct] Value to rebase */
 const long val_idx, /* I [idx] Index into 1-D array of values */
 const nc_type val_typ, /* I [enm] Value type */
 const char *unit_sng, /* I [sng] Units string */
 char *lgb_sng); /* O [sng] Legible version of input string */

#ifdef __cplusplus
} /* end extern "C" */
#endif /* __cplusplus */

#endif /* NCO_CLN_UTL_H */
