/* $Header: /data/zender/nco_20150216/nco/src/nco++/ncap2.cc,v 1.203 2015-02-07 05:42:36 zender Exp $ */

/* ncap2 -- netCDF arithmetic processor */

/* Purpose: Compute user-defined derived fields using forward algebraic notation applied to netCDF files */

/* Copyright (C) 1995--2015 Charlie Zender
   This file is part of NCO, the netCDF Operators. NCO is free software.
   You may redistribute and/or modify NCO under the terms of the 
   GNU General Public License (GPL) Version 3.
   As a special exception to the terms of the GPL, you are permitted 
   to link the NCO source code with the HDF, netCDF, OPeNDAP, and UDUnits
   libraries and to distribute the resulting executables under the terms 
   of the GPL, but in addition obeying the extra stipulations of the 
   HDF, netCDF, OPeNDAP, and UDUnits licenses.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
   See the GNU General Public License for more details.

   The original author of this software, Charlie Zender, seeks to improve
   it with your suggestions, contributions, bug-reports, and patches.
   Please contact the NCO project at http://nco.sf.net or write to
   Charlie Zender
   Department of Earth System Science
   University of California, Irvine
   Irvine, CA 92697-3100 */

/* Usage:
   ncap2 -O -D 1 -S ~/nco/data/ncap2.in ~/nco/data/in.nc ~/foo.nc
   ncap2 -O -D 1 -v -S ~/nco/data/ncap2_tst.nco  ~/nco/data/in.nc ~/foo.nc
   ncap2 -O -D 1 -v -s two=one+two ~/nco/data/in.nc ~/foo.nc
   ncap2 -O -v -s 'foo=exp(0.61)' ~/nco/data/in.nc ~/foo.nc;ncks -C ~/foo.nc
   ncap2 -O -v -s 'defdim("dmn_tmp")=5;foo[$dmn_tmp]={0,2,4,6,8};foo2=one_dmn_rec_var(foo);print(foo);print(foo2);' ~/nco/data/in.nc ~/foo.nc;
   ncap2 -O -v -s 'foo=0*three_dmn_var_dbl;where(three_dmn_var_dbl>30){foo=three_dmn_var_dbl;}elsewhere{foo=three_dmn_var_dbl@_FillValue;};foo_avg=foo.avg($time);print(foo_avg);' ~/nco/data/in.nc ~/foo.nc
   ncap2 -O -v -D 1 -s 'one_dmn_rec_var(0)=one_dmn_rec_var(0)+1' ~/nco/data/in.nc ~/foo.nc
   ncap2 -O -v -D 1 -s 'three_dmn_rec_var(0,,)=three_dmn_rec_var(0,,)+1' ~/nco/data/in.nc ~/foo.nc */

#ifdef HAVE_CONFIG_H
# include "config.h" /* Autotools tokens */
#endif /* !HAVE_CONFIG_H */

// Standard C++ headers
#include <string>
#include <iterator> /* for inserter */

// Standard C headers
#include <assert.h>  /* assert() debugging macro */
#include <math.h> /* sin cos cos sin 3.14159 */
#include <stdio.h> /* stderr, FILE, NULL, etc. */
#include <stdlib.h> /* atof, atoi, malloc, getopt */
#include <string.h> /* strcmp() */
#include <sys/stat.h> /* stat() */
#include <time.h> /* machine time */
#ifndef _MSC_VER
# include <unistd.h> /* POSIX stuff */
#endif

/* GNU getopt() is independent system header on FREEBSD, LINUX, LINUXALPHA, LINUXAMD, LINUXARM, WIN32
   AT&T getopt() is in unistd.h or stdlib.h on AIX, CRAY, NECSX, SUNMP, SUN4SOL2
   fxm: Unsure what ALPHA and SGI do */
#ifndef HAVE_GETOPT_LONG
# include "nco_getopt.h"
#else /* HAVE_GETOPT_LONG */ 
# ifdef HAVE_GETOPT_H
#  include <getopt.h>
# endif /* !HAVE_GETOPT_H */ 
#endif /* HAVE_GETOPT_LONG */

/* 3rd party vendors */
#ifdef ENABLE_GSL
# include <gsl/gsl_errno.h>
# include <gsl/gsl_rng.h>
#endif // !ENABLE_GSL

/* Personal headers */
/* #define MAIN_PROGRAM_FILE MUST precede #include libnco.h */
#define MAIN_PROGRAM_FILE
#include "libnco++.hh" /* netCDF Operator (NCO) C++ library */
#include "libnco.h" /* netCDF Operator (NCO) library */

/* Global variables */
size_t ncap_ncl_dpt_crr=0UL; /* [nbr] Depth of current #include file (incremented in ncap_lex.l) */
size_t *ncap_ln_nbr_crr; /* [cnt] Line number (incremented in ncap_lex.l) */
char **ncap_fl_spt_glb=NULL_CEWI; /* [fl] Script file */
#ifdef ENABLE_GSL
int ncap_gsl_mode_prec=0; /* Precision for GSL functions with mode_t argument (Airy, hypergeometric) */ 
#endif // !ENABLE_GSL

/* Forward Declaration */
void pop_fmc_vtr(std::vector<fmc_cls> &fmc_vtr, vtl_cls *vfnc);
void ram_vars_add(prs_cls *prs_arg);

int 
main(int argc,char **argv)
{
  const char fnc_nm[]="main"; 
  FILE *yyin; /* File handle used to check file existance */
  int parse_antlr(std::vector<prs_cls> &prs_vtr ,char*,char*);
  
  /* fxm TODO nco652 */
  double rnd_nbr(double);
  
  nco_bool ATT_INHERIT=True;          
  nco_bool ATT_PROPAGATE=True;        
  nco_bool CNV_CCM_CCSM_CF;
  nco_bool EXTRACT_ALL_COORDINATES=False; /* Option c */
  nco_bool EXTRACT_ASSOCIATED_COORDINATES=True; /* Option C */
  nco_bool FL_RTR_RMT_LCN;
  nco_bool FL_LST_IN_FROM_STDIN=False; /* [flg] fl_lst_in comes from stdin */
  nco_bool FORCE_APPEND=False; /* Option A */
  nco_bool FORCE_OVERWRITE=False; /* Option O */
  nco_bool FORTRAN_IDX_CNV=False; /* Option F */
  nco_bool HISTORY_APPEND=True; /* Option h */
  nco_bool FL_OUT_NEW=False;
  nco_bool PRN_FNC_TBL=False; /* Option f */  
  nco_bool PROCESS_ALL_VARS=True; /* Option v */  
  nco_bool RAM_CREATE=False; /* [flg] Create file in RAM */
  nco_bool RAM_OPEN=False; /* [flg] Open (netCDF3-only) file(s) in RAM */
  nco_bool RM_RMT_FL_PST_PRC=True; /* Option R */
  nco_bool WRT_TMP_FL=True; /* [flg] Write output to temporary file */
  nco_bool flg_cln=True; /* [flg] Clean memory prior to exit */
  
  aed_sct att_item; // Used to convert atts in vector to normal form  
  
  char **fl_lst_abb=NULL_CEWI; /* Option n */
  char **fl_lst_in;
  char **var_lst_in=NULL_CEWI;
  char *cmd_ln;
  char *cnk_arg[NC_MAX_DIMS];
  char *cnk_map_sng=NULL_CEWI; /* [sng] Chunking map */
  char *cnk_plc_sng=NULL_CEWI; /* [sng] Chunking policy */
  char *fl_in=NULL_CEWI;
  char *fl_out=NULL_CEWI; /* Option o */
  char *fl_out_tmp;
  char *fl_pth=NULL_CEWI; /* Option p */
  char *fl_pth_lcl=NULL_CEWI; /* Option l */
  char *fl_spt_usr=NULL_CEWI; /* Option s */
  char *lmt_arg[NC_MAX_DIMS];
  char *opt_crr=NULL_CEWI; /* [sng] String representation of current long-option name */
  char *sng_cnv_rcd=NULL_CEWI; /* [sng] strtol()/strtoul() return code */
#define NCAP_SPT_NBR_MAX 100
  char *spt_arg[NCAP_SPT_NBR_MAX]; /* fxm: Arbitrary size, should be dynamic */
  char *spt_arg_cat=NULL_CEWI; /* [sng] User-specified script */
  
  const char * const CVS_Id="$Id: ncap2.cc,v 1.203 2015-02-07 05:42:36 zender Exp $"; 
  const char * const CVS_Revision="$Revision: 1.203 $";
  const char * const att_nm_tmp="eulaVlliF_"; /* For netCDF4 name hack */
  const char * const opt_sht_lst="3467ACcD:FfhL:l:n:Oo:p:Rrs:S:t:vx-:"; /* [sng] Single letter command line options */
  
  cnk_sct cnk; /* [sct] Chunking structure */

  dmn_sct **dmn_in=NULL_CEWI;  /* [lst] Dimensions in input file */
  dmn_sct *dmn_new;
  dmn_sct *dmn_item;

  // Template lists
  NcapVector<dmn_sct*> dmn_in_vtr;  
  NcapVector<dmn_sct*> dmn_out_vtr;  
  
  // Holder for attributes and vectors
  NcapVarVector var_vtr;
  
  // Holder for attributes and vectors in first parse
  NcapVarVector int_vtr;
  
  // Method/function holder
  std::vector<fmc_cls> fmc_vtr;
  
  // Holder for prs_cls, NB: used for OpenMP
  std::vector<prs_cls> prs_vtr;
  
  extern char *optarg;
  extern int optind;
  
  int abb_arg_nbr=0;
  int cnk_map=nco_cnk_map_nil; /* [enm] Chunking map */
  int cnk_nbr=0; /* [nbr] Number of chunk sizes */
  int cnk_plc=nco_cnk_plc_nil; /* [enm] Chunking policy */
  int dfl_lvl=NCO_DFL_LVL_UNDEFINED; /* [enm] Deflate level */
  int fl_nbr=0;
  int fl_in_fmt; /* [enm] Input file format */
  int fl_out_fmt=NCO_FORMAT_UNDEFINED; /* [enm] Output file format */
  int fll_md_old; /* [enm] Old fill mode */
  int in_id;  
  int idx;
  int jdx;
  int lmt_nbr=0; /* Option d. NB: lmt_nbr gets incremented */
  int md_open; /* [enm] Mode flag for nc_open() call */
  int nbr_dmn_ass=int_CEWI;/* Number of dimensions in temporary list */
  int nbr_dmn_in=int_CEWI; /* Number of dimensions in dim_in */
  int nbr_lst_a=0; /* size of xtr_lst_a */
  int nbr_spt=0; /* Option s. NB: nbr_spt gets incremented */
  int nbr_var_fix; /* nbr_var_fix gets incremented */
  int nbr_var_fl;/* number of vars in a file */
  int nbr_var_prc; /* nbr_var_prc gets incremented */
  int xtr_nbr=0; /* xtr_nbr will not otherwise be set for -c with no -v */
  int opt;
  int out_id;  
  int rcd=NC_NOERR; /* [rcd] Return code */
  int var_id;
  int thr_nbr=int_CEWI; /* [nbr] Thread number Option t */
  
  lmt_sct **lmt=NULL_CEWI;
  
  nm_id_sct *dmn_lst=NULL_CEWI;
  nm_id_sct *xtr_lst=NULL_CEWI; /* Non-processed variables to copy to OUTPUT */
  nm_id_sct *xtr_lst_a=NULL_CEWI; /* Initialize to ALL variables in OUTPUT file */
  
  size_t bfr_sz_hnt=NC_SIZEHINT_DEFAULT; /* [B] Buffer size hint */
  size_t cnk_min_byt=NCO_CNK_SZ_MIN_BYT_DFL; /* [B] Minimize size of variable to chunk */
  size_t cnk_sz_byt=0UL; /* [B] Chunk size in bytes */
  size_t cnk_sz_scl=0UL; /* [nbr] Chunk size scalar */
  size_t hdr_pad=0UL; /* [B] Pad at end of header section */
  size_t sng_lng;
  size_t spt_arg_lng=size_t_CEWI;

  var_sct **var;
  var_sct **var_fix;
  var_sct **var_fix_out;
  var_sct **var_out;
  var_sct **var_prc;
  var_sct **var_prc_out;
  
  static struct option opt_lng[]=
    { /* Structure ordered by short option key if possible */
      /* Long options with no argument, no short option counterpart */
      {"cln",no_argument,0,0}, /* [flg] Clean memory prior to exit */
      {"clean",no_argument,0,0}, /* [flg] Clean memory prior to exit */
      {"mmr_cln",no_argument,0,0}, /* [flg] Clean memory prior to exit */
      {"drt",no_argument,0,0}, /* [flg] Allow dirty memory on exit */
      {"dirty",no_argument,0,0}, /* [flg] Allow dirty memory on exit */
      {"mmr_drt",no_argument,0,0}, /* [flg] Allow dirty memory on exit */
      {"hdf4",no_argument,0,0}, /* [flg] Treat file as HDF4 */
      {"hdf_upk",no_argument,0,0}, /* [flg] HDF unpack convention: unpacked=scale_factor*(packed-add_offset) */
      {"hdf_unpack",no_argument,0,0}, /* [flg] HDF unpack convention: unpacked=scale_factor*(packed-add_offset) */
      {"lbr",no_argument,0,0},
      {"library",no_argument,0,0},
      {"ram_all",no_argument,0,0}, /* [flg] Open (netCDF3) and create file(s) in RAM */
      {"create_ram",no_argument,0,0}, /* [flg] Create file in RAM */
      {"open_ram",no_argument,0,0}, /* [flg] Open (netCDF3) file(s) in RAM */
      {"diskless_all",no_argument,0,0}, /* [flg] Open (netCDF3) and create file(s) in RAM */
      {"wrt_tmp_fl",no_argument,0,0}, /* [flg] Write output to temporary file */
      {"write_tmp_fl",no_argument,0,0}, /* [flg] Write output to temporary file */
      {"no_tmp_fl",no_argument,0,0}, /* [flg] Do not write output to temporary file */
      {"version",no_argument,0,0},
      {"vrs",no_argument,0,0},
      /* Long options with argument, no short option counterpart */
      {"bfr_sz_hnt",required_argument,0,0}, /* [B] Buffer size hint */
      {"buffer_size_hint",required_argument,0,0}, /* [B] Buffer size hint */
      {"cnk_byt",required_argument,0,0}, /* [B] Chunk size in bytes */
      {"chunk_byte",required_argument,0,0}, /* [B] Chunk size in bytes */
      {"cnk_dmn",required_argument,0,0}, /* [nbr] Chunk size */
      {"chunk_dimension",required_argument,0,0}, /* [nbr] Chunk size */
      {"cnk_map",required_argument,0,0}, /* [nbr] Chunking map */
      {"chunk_map",required_argument,0,0}, /* [nbr] Chunking map */
      {"cnk_min",required_argument,0,0}, /* [B] Minimize size of variable to chunk */
      {"chunk_min",required_argument,0,0}, /* [B] Minimize size of variable to chunk */
      {"cnk_plc",required_argument,0,0}, /* [nbr] Chunking policy */
      {"chunk_policy",required_argument,0,0}, /* [nbr] Chunking policy */
      {"cnk_scl",required_argument,0,0}, /* [nbr] Chunk size scalar */
      {"chunk_scalar",required_argument,0,0}, /* [nbr] Chunk size scalar */
      {"fl_fmt",required_argument,0,0},
      {"file_format",required_argument,0,0},
      {"hdr_pad",required_argument,0,0},
      {"header_pad",required_argument,0,0},
      /* Long options with short counterparts */
      {"3",no_argument,0,'3'},
      {"4",no_argument,0,'4'},
      {"64bit",no_argument,0,'4'},
      {"netcdf4",no_argument,0,'4'},
      {"7",no_argument,0,'7'},
      {"append",no_argument,0,'A'},
      {"coords",no_argument,0,'c'},
      {"crd",no_argument,0,'c'},
      {"no-coords",no_argument,0,'C'},
      {"no-crd",no_argument,0,'C'},
      {"debug",required_argument,0,'D'},
      {"nco_dbg_lvl",required_argument,0,'D'},
      {"dimension",required_argument,0,'d'},
      {"dmn",required_argument,0,'d'},
      {"fnc_tbl",no_argument,0,'f'},
      {"prn_fnc_tbl",no_argument,0,'f'},
      {"ftn",no_argument,0,'F'},
      {"history",no_argument,0,'h'},
      {"hst",no_argument,0,'h'},
      {"dfl_lvl",required_argument,0,'L'}, /* [enm] Deflate level */
      {"deflate",required_argument,0,'L'}, /* [enm] Deflate level */
      {"local",required_argument,0,'l'},
      {"lcl",required_argument,0,'l'},
      {"nintap",required_argument,0,'n'},
      {"overwrite",no_argument,0,'O'},
      {"ovr",no_argument,0,'O'},
      {"output",required_argument,0,'o'},
      {"fl_out",required_argument,0,'o'},
      {"path",required_argument,0,'p'},
      {"retain",no_argument,0,'R'},
      {"rtn",no_argument,0,'R'},
      {"revision",no_argument,0,'r'},
      {"file",required_argument,0,'S'},
      {"script-file",required_argument,0,'S'},
      {"signal",no_argument,0,'z'},
      {"fl_spt",required_argument,0,'S'},
      {"spt",required_argument,0,'s'},
      {"script",required_argument,0,'s'},
      {"thr_nbr",required_argument,0,'t'},
      {"units",no_argument,0,'u'},
      {"variable",no_argument,0,'v'},
      {"exclude",no_argument,0,'x'},
      {"xcl",no_argument,0,'x'},
      {"help",no_argument,0,'?'},
      {"hlp",no_argument,0,'?'},
      {0,0,0,0}
    }; /* end opt_lng */
  int opt_idx=0; /* Index of current long option into opt_lng array */
  
  /* Start clock and save command line */ 
  cmd_ln=nco_cmd_ln_sng(argc,argv);
  
  /* Get program name and set program enum (e.g., nco_prg_id=ncra) */
  nco_prg_nm=nco_prg_prs(argv[0],&nco_prg_id);
  
  /* Parse command line arguments */
  while(1){
    /* getopt_long_only() allows one dash to prefix long options */
    opt=getopt_long(argc,argv,opt_sht_lst,opt_lng,&opt_idx);
    /* NB: access to opt_crr is only valid when long_opt is detected */
    if(opt == EOF) break; /* Parse positional arguments once getopt_long() returns EOF */
    opt_crr=(char *)strdup(opt_lng[opt_idx].name);
    
    /* Process long options without short option counterparts */
    if(opt == 0){
      if(!strcmp(opt_crr,"bfr_sz_hnt") || !strcmp(opt_crr,"buffer_size_hint")){
	bfr_sz_hnt=strtoul(optarg,&sng_cnv_rcd,NCO_SNG_CNV_BASE10);
	if(*sng_cnv_rcd) nco_sng_cnv_err(optarg,"strtoul",sng_cnv_rcd);
      } /* endif cnk */
      if(!strcmp(opt_crr,"cnk_byt") || !strcmp(opt_crr,"chunk_byte")){
        cnk_sz_byt=strtoul(optarg,&sng_cnv_rcd,NCO_SNG_CNV_BASE10);
        if(*sng_cnv_rcd) nco_sng_cnv_err(optarg,"strtoul",sng_cnv_rcd);
      } /* endif cnk_byt */
      if(!strcmp(opt_crr,"cnk_min") || !strcmp(opt_crr,"chunk_min")){
        cnk_min_byt=strtoul(optarg,&sng_cnv_rcd,NCO_SNG_CNV_BASE10);
        if(*sng_cnv_rcd) nco_sng_cnv_err(optarg,"strtoul",sng_cnv_rcd);
      } /* endif cnk_min */
      if(!strcmp(opt_crr,"cnk_dmn") || !strcmp(opt_crr,"chunk_dimension")){
	/* Copy limit argument for later processing */
	cnk_arg[cnk_nbr]=(char *)strdup(optarg);
	cnk_nbr++;
      } /* endif cnk_dmn */
      if(!strcmp(opt_crr,"cnk_scl") || !strcmp(opt_crr,"chunk_scalar")){
	cnk_sz_scl=strtoul(optarg,&sng_cnv_rcd,NCO_SNG_CNV_BASE10);
	if(*sng_cnv_rcd) nco_sng_cnv_err(optarg,"strtoul",sng_cnv_rcd);
      } /* endif cnk */
      if(!strcmp(opt_crr,"cnk_map") || !strcmp(opt_crr,"chunk_map")){
	/* Chunking map */
	cnk_map_sng=(char *)strdup(optarg);
	cnk_map=nco_cnk_map_get(cnk_map_sng);
      } /* endif cnk */
      if(!strcmp(opt_crr,"cnk_plc") || !strcmp(opt_crr,"chunk_policy")){
	/* Chunking policy */
	cnk_plc_sng=(char *)strdup(optarg);
	cnk_plc=nco_cnk_plc_get(cnk_plc_sng);
      } /* endif cnk */
      if(!strcmp(opt_crr,"cln") || !strcmp(opt_crr,"mmr_cln") || !strcmp(opt_crr,"clean")) flg_cln=True; /* [flg] Clean memory prior to exit */
      if(!strcmp(opt_crr,"drt") || !strcmp(opt_crr,"mmr_drt") || !strcmp(opt_crr,"dirty")) flg_cln=False; /* [flg] Clean memory prior to exit */
      if(!strcmp(opt_crr,"fl_fmt") || !strcmp(opt_crr,"file_format")) rcd=nco_create_mode_prs(optarg,&fl_out_fmt);
      if(!strcmp(opt_crr,"hdf4")) nco_fmt_xtn=nco_fmt_xtn_hdf4; /* [enm] Treat file as HDF4 */
      if(!strcmp(opt_crr,"hdr_pad") || !strcmp(opt_crr,"header_pad")){
        hdr_pad=strtoul(optarg,&sng_cnv_rcd,NCO_SNG_CNV_BASE10);
        if(*sng_cnv_rcd) nco_sng_cnv_err(optarg,"strtoul",sng_cnv_rcd);
      } /* endif "hdr_pad" */
      if(!strcmp(opt_crr,"hdf_upk") || !strcmp(opt_crr,"hdf_unpack")) nco_upk_cnv=nco_upk_HDF; /* [flg] HDF unpack convention: unpacked=scale_factor*(packed-add_offset) */
      if(!strcmp(opt_crr,"lbr") || !strcmp(opt_crr,"library")){
        (void)nco_lbr_vrs_prn();
        nco_exit(EXIT_SUCCESS);
      } /* endif "lbr" */
      if(!strcmp(opt_crr,"ram_all") || !strcmp(opt_crr,"create_ram") || !strcmp(opt_crr,"diskless_all")) RAM_CREATE=True; /* [flg] Open (netCDF3) file(s) in RAM */
      if(!strcmp(opt_crr,"ram_all") || !strcmp(opt_crr,"open_ram") || !strcmp(opt_crr,"diskless_all")) RAM_OPEN=True; /* [flg] Create file in RAM */
      if(!strcmp(opt_crr,"vrs") || !strcmp(opt_crr,"version")){
	(void)nco_vrs_prn(CVS_Id,CVS_Revision);
	nco_exit(EXIT_SUCCESS);
      } /* endif "vrs" */
      if(!strcmp(opt_crr,"wrt_tmp_fl") || !strcmp(opt_crr,"write_tmp_fl")) WRT_TMP_FL=True;
      if(!strcmp(opt_crr,"no_tmp_fl")) WRT_TMP_FL=False;
    } /* opt != 0 */
    /* Process short options */
    switch(opt){
    case 0: /* Long options have already been processed, return */
      break;
    case '3': /* Request netCDF3 output storage format */
      fl_out_fmt=NC_FORMAT_CLASSIC;
      break;
    case '4': /* Catch-all to prescribe output storage format */
      if(!strcmp(opt_crr,"64bit")) fl_out_fmt=NC_FORMAT_64BIT; else fl_out_fmt=NC_FORMAT_NETCDF4; 
      break;
    case '6': /* Request netCDF3 64-bit offset output storage format */
      fl_out_fmt=NC_FORMAT_64BIT;
      break;
    case '7': /* Request netCDF4-classic output storage format */
      fl_out_fmt=NC_FORMAT_NETCDF4_CLASSIC;
      break;
    case 'A': /* Toggle FORCE_APPEND */
      FORCE_APPEND=!FORCE_APPEND;
      break;
    case 'C': /* Extract all coordinates associated with extracted variables? */
      EXTRACT_ASSOCIATED_COORDINATES=False;
      break;
    case 'c':
      EXTRACT_ALL_COORDINATES=True;
      break;
    case 'D': /* Debugging level. Default is 0. */
      nco_dbg_lvl=(unsigned short int)strtoul(optarg,&sng_cnv_rcd,NCO_SNG_CNV_BASE10);
      if(*sng_cnv_rcd) nco_sng_cnv_err(optarg,"strtoul",sng_cnv_rcd);
      nc_set_log_level(nco_dbg_lvl);
      break;
    case 'F': /* Toggle index convention. Default is 0-based arrays (C-style). */
      FORTRAN_IDX_CNV=!FORTRAN_IDX_CNV;
      break;
    case 'f': /* Print function table */
      PRN_FNC_TBL=True;
      break;
    case 'h': /* Toggle appending to history global attribute */
      HISTORY_APPEND=!HISTORY_APPEND;
      break;
    case 'L': /* [enm] Deflate level. Default is 0. */
      dfl_lvl=(int)strtol(optarg,&sng_cnv_rcd,NCO_SNG_CNV_BASE10);
      if(*sng_cnv_rcd) nco_sng_cnv_err(optarg,"strtol",sng_cnv_rcd);
      break;
    case 'l': /* Local path prefix for files retrieved from remote file system */
      fl_pth_lcl=(char *)strdup(optarg);
      break;
    case 'n': /* NINTAP-style abbreviation of files to process */
      /* Currently not used in ncap but should be to allow processing multiple input files by same script */
      err_prn(fnc_nm,std::string(nco_prg_nm_get())+ " does not currently implement -n option\n");
      fl_lst_abb=nco_lst_prs_2D(optarg,",",&abb_arg_nbr);
      if(abb_arg_nbr < 1 || abb_arg_nbr > 3)
	err_prn(fnc_nm, "Incorrect abbreviation for file list\n");
      break;
    case 'O': /* Toggle FORCE_OVERWRITE */
      FORCE_OVERWRITE=!FORCE_OVERWRITE;
      break;
    case 'o': /* Name of output file */
      fl_out=(char *)strdup(optarg);
      break;
    case 'p': /* Common file path */
      fl_pth=(char *)strdup(optarg);
      break;
    case 'R': /* Toggle removal of remotely-retrieved-files. Default is True. */
      RM_RMT_FL_PST_PRC=!RM_RMT_FL_PST_PRC;
      break;
    case 'r': /* Print CVS program information and copyright notice */
      (void)nco_vrs_prn(CVS_Id,CVS_Revision);
      (void)nco_lbr_vrs_prn();
      (void)nco_cpy_prn();
      (void)nco_cnf_prn();
      nco_exit(EXIT_SUCCESS);
      break;
    case 's': /* Copy command script for later processing */
      spt_arg[nbr_spt++]=(char *)strdup(optarg);
      if(nbr_spt == NCAP_SPT_NBR_MAX-1) wrn_prn(fnc_nm,"No more than " +nbr2sng(NCAP_SPT_NBR_MAX) + " allowed. TODO# 24.");
      break;
    case 'S': /* Read command script from file rather than from command line */
      fl_spt_usr=(char *)strdup(optarg);
      break;
    case 't': /* Thread number */
      thr_nbr=(int)strtol(optarg,&sng_cnv_rcd,NCO_SNG_CNV_BASE10);
      if(*sng_cnv_rcd) nco_sng_cnv_err(optarg,"strtol",sng_cnv_rcd);
      break;
    case 'v': /* Variables to extract/exclude */
      PROCESS_ALL_VARS=False;
      xtr_nbr=0;
      break;
    case '?': /* Print proper usage */
      (void)nco_usg_prn();
      nco_exit(EXIT_SUCCESS);
      break;
    case '-': /* Long options are not allowed */
      err_prn(fnc_nm,"Long options are not available in this build. Use single letter options instead.\n");
      break;
    default: /* Print proper usage */
      (void)fprintf(stdout,"%s ERROR in command-line syntax/options. Please reformulate command accordingly.\n",nco_prg_nm_get());
      (void)nco_usg_prn();
      nco_exit(EXIT_FAILURE);
      break;
    } /* end switch */
    if(opt_crr) opt_crr=(char *)nco_free(opt_crr);
  } /* end while loop */
  
  /* Append ";\n" to command-script arguments, then concatenate them */
  for(idx=0;idx<nbr_spt;idx++){
    sng_lng=strlen(spt_arg[idx]);
    if(idx == 0){
      spt_arg_cat=(char *)nco_malloc(sng_lng+3);
      strcpy(spt_arg_cat,spt_arg[idx]);
      strcat(spt_arg_cat,";\n");
      spt_arg_lng=sng_lng+3;
    }else{
      spt_arg_lng+=sng_lng+2;
      spt_arg_cat=(char *)nco_realloc(spt_arg_cat,spt_arg_lng);
      strcat(spt_arg_cat,spt_arg[idx]);
      strcat(spt_arg_cat,";\n");
    } /* end else */
  } /* end if */    
  
  /* Create function/method vector */
 
  // Conversion functions
  cnv_cls cnv_obj(true);
  // Aggregate functions
  agg_cls agg_obj(true);
  // Utility Functions 
  utl_cls utl_obj(true);
  // Maths Functions
  mth_cls mth_obj(true);
  // Maths2 Functions
  mth2_cls mth2_obj(true);
  // Basic Functions
  bsc_cls bsc_obj(true);
  // PDQ functions
  pdq_cls pdq_obj(true);
  // Mask functions
  msk_cls msk_obj(true);
  // Pack functions
  pck_cls pck_obj(true); 
  // Sort functions
  srt_cls srt_obj(true); 
  // Unary functions
  unr_cls unr_obj(true); 
  // Array functions
  arr_cls arr_obj(true); 
  // Bilinear interpolation functions
  bil_cls bil_obj(true); 
  // Co-ordinates functions
  cod_cls cod_obj(true); 
  // misc functions
  misc_cls misc_obj(true); 

  // Populate vector
  (void)pop_fmc_vtr(fmc_vtr,&cnv_obj);
  (void)pop_fmc_vtr(fmc_vtr,&agg_obj);
  (void)pop_fmc_vtr(fmc_vtr,&utl_obj);
  (void)pop_fmc_vtr(fmc_vtr,&mth_obj);
  (void)pop_fmc_vtr(fmc_vtr,&mth2_obj);
  (void)pop_fmc_vtr(fmc_vtr,&bsc_obj);
  (void)pop_fmc_vtr(fmc_vtr,&pdq_obj);
  (void)pop_fmc_vtr(fmc_vtr,&msk_obj);
  (void)pop_fmc_vtr(fmc_vtr,&pck_obj);
  (void)pop_fmc_vtr(fmc_vtr,&srt_obj);
  (void)pop_fmc_vtr(fmc_vtr,&unr_obj);
  (void)pop_fmc_vtr(fmc_vtr,&arr_obj);
  (void)pop_fmc_vtr(fmc_vtr,&bil_obj);
  (void)pop_fmc_vtr(fmc_vtr,&cod_obj);
  (void)pop_fmc_vtr(fmc_vtr,&misc_obj);

#ifdef ENABLE_GSL
#ifdef ENABLE_NCO_GSL
  // nco_gsl functions
  nco_gsl_cls nco_gsl_obj(true); 

  // Populate vector
  (void)pop_fmc_vtr(fmc_vtr,&nco_gsl_obj);
#endif //ENABLE_NCO_GSL
#endif //ENABLE_GSL
   
  // GSL functions
#ifdef ENABLE_GSL
  char *str_ptr;
  
  gsl_cls gsl_obj(true); 
  gsl2_cls gsl2_obj(true); 
  gsl_stt2_cls gsl_stt2_obj(true);
  gsl_spl_cls gsl_spl_obj(true);
  gsl_fit_cls gsl_fit_obj(true);
 
  (void)pop_fmc_vtr(fmc_vtr,&gsl_obj);
  (void)pop_fmc_vtr(fmc_vtr,&gsl2_obj);
  (void)pop_fmc_vtr(fmc_vtr,&gsl_stt2_obj);
  (void)pop_fmc_vtr(fmc_vtr,&gsl_spl_obj);
  (void)pop_fmc_vtr(fmc_vtr,&gsl_fit_obj);

  /* Set GSL error handler */
  gsl_set_error_handler_off(); 
  
  /* Initialize global from environment variable */  
  if((str_ptr=getenv("GSL_PREC_MODE"))) ncap_gsl_mode_prec=(int)strtol(str_ptr,(char **)NULL,NCO_SNG_CNV_BASE10);
  
  if(ncap_gsl_mode_prec<0 || ncap_gsl_mode_prec>2) ncap_gsl_mode_prec=0;

  /* create generator chosen by environment variables GSL_RNG_TYPE, GSL_RNG_SEED */
  gsl_rng_env_setup();
#endif // !ENABLE_GSL
  
  // Sort Vector 
  std::sort(fmc_vtr.begin(),fmc_vtr.end());
  
  if(PRN_FNC_TBL){
    (void)fprintf(stdout,"Methods available in %s:\n",nco_prg_nm_get());
    for(idx=0;idx<(signed int)fmc_vtr.size();idx++)
      std::cout<< fmc_vtr[idx].fnm()<<"()"<<std::endl; 
    nco_exit(EXIT_SUCCESS);
  } /* !PRN_FNC_TBL */
  
  /* Initialize thread information */
  thr_nbr=nco_openmp_ini(thr_nbr);
  
  /* Process positional arguments and fill in filenames */
  fl_lst_in=nco_fl_lst_mk(argv,argc,optind,&fl_nbr,&fl_out,&FL_LST_IN_FROM_STDIN);
  if(fl_out) FL_OUT_NEW=True; else fl_out=(char *)strdup(fl_lst_in[0]);
  
  /* Make uniform list of user-specified dimension limits */
  if(lmt_nbr > 0) lmt=nco_lmt_prs(lmt_nbr,lmt_arg);
  
  /* Parse filename */
  fl_in=nco_fl_nm_prs(fl_in,0,&fl_nbr,fl_lst_in,abb_arg_nbr,fl_lst_abb,fl_pth);
  /* Make sure file is on local system and is readable or die trying */
  fl_in=nco_fl_mk_lcl(fl_in,fl_pth_lcl,&FL_RTR_RMT_LCN);
  /* Open file using appropriate buffer size hints and verbosity */
  if(RAM_OPEN) md_open=NC_NOWRITE|NC_DISKLESS; else md_open=NC_NOWRITE;
  rcd+=nco_fl_open(fl_in,md_open,&bfr_sz_hnt,&in_id);
  (void)nco_inq_format(in_id,&fl_in_fmt);
  
  /* Form list of all dimensions in file */  
  dmn_lst=nco_dmn_lst(in_id,&nbr_dmn_in);
  
  //dmn_in=(dmn_sct **)nco_malloc(nbr_dmn_in*sizeof(dmn_sct *));
  for(idx=0;idx<nbr_dmn_in;idx++) dmn_in_vtr.push_back(nco_dmn_fll(in_id,dmn_lst[idx].id,dmn_lst[idx].nm));
  
  // Sort vector
  dmn_in_vtr.sort();
  dmn_in=&dmn_in_vtr[0];
  
  /* Merge hyperslab limit information into dimension structures */
  if(lmt_nbr > 0) (void)nco_dmn_lmt_mrg(dmn_in,nbr_dmn_in,lmt,lmt_nbr);
  
  /* Make output and input files consanguinous */
  if(fl_out_fmt == NCO_FORMAT_UNDEFINED) fl_out_fmt=fl_in_fmt;
  
  /* Verify output file format supports requested actions */
  (void)nco_fl_fmt_vet(fl_out_fmt,cnk_nbr,dfl_lvl);

  /* Open output file */
  if(FL_OUT_NEW){
    /* Normal case, like rest of NCO, where writes are made to temporary file */
    fl_out_tmp=nco_fl_out_open(fl_out,FORCE_APPEND,FORCE_OVERWRITE,fl_out_fmt,&bfr_sz_hnt,RAM_CREATE,RAM_OPEN,WRT_TMP_FL,&out_id);
  }else{ /* Existing file */
    /* ncap2, like ncrename and ncatted, directly modifies fl_in if fl_out is omitted
       If fl_out resolves to _same name_ as fl_in, method above is employed */
    fl_out_tmp=(char *)strdup(fl_out);
    if(RAM_OPEN) md_open=NC_WRITE|NC_DISKLESS; else md_open=NC_WRITE;
    rcd+=nco_fl_open(fl_out_tmp,md_open,&bfr_sz_hnt,&out_id);
    (void)nco_redef(out_id);
  } /* Existing file */
  
  /* Create structure with all chunking information */
  if(fl_out_fmt == NC_FORMAT_NETCDF4 || fl_out_fmt == NC_FORMAT_NETCDF4_CLASSIC) rcd+=nco_cnk_ini(fl_out,cnk_arg,cnk_nbr,cnk_map,cnk_plc,cnk_min_byt,cnk_sz_byt,cnk_sz_scl,&cnk);

  /* Copy global attributes */
  (void)nco_att_cpy(in_id,out_id,NC_GLOBAL,NC_GLOBAL,(nco_bool)True);

  /* Catenate time-stamped command line to "history" global attribute */
  if(HISTORY_APPEND) (void)nco_hst_att_cat(out_id,cmd_ln);
  if(thr_nbr > 0 && HISTORY_APPEND) (void)nco_thr_att_cat(out_id,thr_nbr);

  /* Take output file out of define mode */
  if(hdr_pad == 0UL){
    (void)nco_enddef(out_id);
  }else{
    (void)nco__enddef(out_id,hdr_pad);
    if(nco_dbg_lvl >= nco_dbg_scl) (void)fprintf(stderr,"%s: INFO Padding header with %lu extra bytes\n",nco_prg_nm_get(),(unsigned long)hdr_pad);
  } /* hdr_pad */
    
  /* Set arguments for script execution NB: all these are used as references */
  prs_cls prs_arg(dmn_in_vtr,dmn_out_vtr,fmc_vtr,var_vtr,int_vtr);
  
  prs_arg.fl_in=fl_in; /* [sng] Input data file */
  prs_arg.in_id=in_id; /* [id] Input data file ID */
  prs_arg.fl_out=fl_out; /* [sng] Output data file */
  prs_arg.out_id=out_id; /* [id] Output data file ID */
  prs_arg.fl_out_fmt=fl_out_fmt; /* [sng] Output data file format */

  if(RAM_OPEN) md_open=NC_NOWRITE|NC_DISKLESS; else md_open=NC_NOWRITE;
  rcd+=nco_fl_open(fl_out_tmp,md_open,&bfr_sz_hnt,&prs_arg.out_id_readonly);
  
  prs_arg.FORTRAN_IDX_CNV=FORTRAN_IDX_CNV;
  prs_arg.ATT_PROPAGATE=ATT_PROPAGATE;      
  prs_arg.ATT_INHERIT=ATT_INHERIT;
  prs_arg.NCAP_MPI_SORT=(thr_nbr > 1 ? true:false);
  
  prs_arg.dfl_lvl=dfl_lvl;  /* [enm] Deflate level */
  prs_arg.cnk_sz=(size_t*)NULL; /* Chunk sizes NULL for now */ 
  
#ifdef NCO_NETCDF4_AND_FILLVALUE
  prs_arg.NCAP4_FILL = (fl_out_fmt==NC_FORMAT_NETCDF4 || fl_out_fmt==NC_FORMAT_NETCDF4_CLASSIC);
#else
  prs_arg.NCAP4_FILL=false;
#endif
  prs_arg.ntl_scn=false;
  (void)ram_vars_add(&prs_arg);
  
  prs_vtr.push_back(prs_arg); 
  
  for(idx=1;idx<thr_nbr;idx++){
    prs_cls prs_tmp(prs_arg);
    
    /* Open files for each thread */
    if(RAM_OPEN) md_open=NC_NOWRITE|NC_DISKLESS; else md_open=NC_NOWRITE;
    rcd+=nco_fl_open(fl_in,md_open,&bfr_sz_hnt,&prs_tmp.in_id);
    
    /* Handle to read output only */
    rcd+=nco_fl_open(fl_out_tmp,md_open,&bfr_sz_hnt,&prs_tmp.out_id_readonly);
    
    /* Only one handle for reading and writing */
    prs_tmp.out_id=out_id;
    
    prs_vtr.push_back(prs_tmp);
  } /* end loop over threads */

  /* If we are appending (-A) then output file already contains dimensions/variables */
  if(FORCE_APPEND){
    int nbr_dmn_out=0;  
    int nbr_xtr=0;
    nm_id_sct *dmn_lst_out=NULL_CEWI;

    NcapVar *Nvar;

    nbr_var_fl=0;
    var_sct *var_tmp;

    /* Form list of all dimensions in file */  
    dmn_lst_out=nco_dmn_lst(out_id,&nbr_dmn_out);
  
    //dmn_in=(dmn_sct **)nco_malloc(nbr_dmn_in*sizeof(dmn_sct *));
    for(idx=0;idx<nbr_dmn_out;idx++) dmn_out_vtr.push_back(nco_dmn_fll(out_id,dmn_lst_out[idx].id,dmn_lst_out[idx].nm));

    // Sort vector
    dmn_out_vtr.sort();

    /* xrf dmn_out with dmn_in_vtr */
    for(idx=0;idx<nbr_dmn_out;idx++){
      dmn_item=dmn_in_vtr.find(dmn_out_vtr[idx]->nm);
      if(!dmn_item) continue; 
      (void)nco_dmn_xrf(dmn_item,dmn_out_vtr[idx]);
      if(dmn_item->sz != dmn_out_vtr[idx]->sz) 
        (void)fprintf(stdout,"%s: WARNING: dimension miss-match. Dimension \"%s\" is size=%ld in input file and size=%ld in output file.\nHINT: Command may work if Append mode (-A) is not used.\n",nco_prg_nm_get(),dmn_item->nm,dmn_item->sz,dmn_out_vtr[idx]->sz);
    } /* end loop over dimensions */

    /* Get number of variables in output file */
    rcd=nco_inq(out_id,(int *)NULL,&nbr_var_fl,(int *)NULL,(int *)NULL);
  
    /* Make list of all new variables in output_file */  
    xtr_lst_a=nco_var_lst_mk(out_id,nbr_var_fl,(char**)NULL,False,False,&nbr_xtr);      
    for(idx=0;idx<nbr_var_fl;idx++){
      var_tmp=prs_arg.ncap_var_init(xtr_lst_a[idx].nm,false);

      Nvar=new NcapVar(var_tmp); 
      Nvar->flg_stt=2;   
      prs_arg.var_vtr.push(Nvar);
      /* Copy output attributes into var_vtr */
      ncap_att_gnrl(xtr_lst_a[idx].nm, xtr_lst_a[idx].nm,2,&prs_arg);        
    } /* end loop over variables */

    /* Free lists */
    dmn_lst_out=nco_nm_id_lst_free(dmn_lst_out,nbr_dmn_out);
    xtr_lst_a=nco_nm_id_lst_free(xtr_lst_a,nbr_xtr);
  } /* !FORCE_APPEND */

  // execute in ANTLR command-line script(s)
  if(nbr_spt >0){
    char *fl_cmd_usr=(char *)strdup("Command-line script");
    /* Print all command-line scripts */
    if(nco_dbg_lvl_get() >= nco_dbg_std){
      for(idx=0;idx<nbr_spt;idx++) 
        (void)fprintf(stderr,"spt_arg[%d] = %s\n",idx,spt_arg[idx]);
    } /* endif debug */
     /* Invoke ANTLR parser */
    rcd=parse_antlr(prs_vtr,fl_cmd_usr,spt_arg_cat);
  }
  
  // execute in ANTLR user specified script 
  if(fl_spt_usr){
    if((yyin=fopen(fl_spt_usr,"r")) == NULL_CEWI)
      err_prn(fnc_nm,"Unable to open script file "+std::string(fl_spt_usr));
    fclose(yyin); 
    /* Invoke ANTLR parser */
    rcd=parse_antlr(prs_vtr,fl_spt_usr,(char *)NULL);
  }


  /* Get number of variables in output file */
  rcd=nco_inq(out_id,(int *)NULL,&nbr_var_fl,(int *)NULL,(int *)NULL);
  
  /* Make list of all new variables in output_file */  
  xtr_lst_a=nco_var_lst_mk(out_id,nbr_var_fl,var_lst_in,False,False,&nbr_lst_a);
  
  if(PROCESS_ALL_VARS){
    /* Get number of variables in input file */
    rcd=nco_inq(in_id,(int *)NULL,&nbr_var_fl,(int *)NULL,(int *)NULL);
    /* Form initial list of all variables in input file */
    xtr_lst=nco_var_lst_mk(in_id,nbr_var_fl,var_lst_in,False,False,&xtr_nbr);
  }else{
    /* Make list of variables of new attributes whose parent variable is only in input file */
    xtr_lst=nco_att_lst_mk(in_id,out_id,var_vtr,&xtr_nbr);
  } /* endif */

  /* Subtract list A */
  if(nbr_lst_a > 0) xtr_lst=nco_var_lst_sub(xtr_lst,&xtr_nbr,xtr_lst_a,nbr_lst_a);
  
  /* Put file in define mode to allow metadata writing */
  (void)nco_redef(out_id);
  
  /* Free current list of all dimensions in input file */
  dmn_lst=nco_nm_id_lst_free(dmn_lst,nbr_dmn_in);
  
  /* Make list of dimensions of variables in xtr_lst */
  if(xtr_nbr > 0) dmn_lst=nco_dmn_lst_ass_var(in_id,xtr_lst,xtr_nbr,&nbr_dmn_ass);
  
  /* Find and add any new dimensions to output */
  for(idx=0;idx<nbr_dmn_ass;idx++){
    jdx=dmn_in_vtr.findis(dmn_lst[idx].nm);
    if(dmn_in_vtr[jdx]->xrf ) continue;

    (void)dmn_out_vtr.push_back(nco_dmn_dpl(dmn_in_vtr[jdx]));
    (void)nco_dmn_dfn(fl_out,out_id,&dmn_out_vtr.back(),1);
    (void)nco_dmn_xrf(dmn_out_vtr.back(),dmn_in_vtr[jdx]);
  }  
  
  /* Free current list of all dimensions in input file */
  dmn_lst=nco_nm_id_lst_free(dmn_lst,nbr_dmn_ass);
  
  /* Dimensions for manually specified extracted variables are now defined in output file
     Add coordinate variables to extraction list
     If EXTRACT_ALL_COORDINATES then write associated dimension to output */
  if(EXTRACT_ASSOCIATED_COORDINATES){
    for(idx=0;idx<dmn_in_vtr.size();idx++){
      dmn_item=dmn_in_vtr[idx]; /*de-reference */
      if(!dmn_item->is_crd_dmn) continue;

      if(EXTRACT_ALL_COORDINATES && !dmn_item->xrf){
        /* Add dimensions to output list dmn_out */
        dmn_new=nco_dmn_dpl(dmn_item);
        (void)nco_dmn_xrf(dmn_new,dmn_item);
        /* Write dimension to output */
        (void)nco_dmn_dfn(fl_out,out_id,&dmn_new,1);
        (void)dmn_out_vtr.push_back(dmn_new);
      } /* end if */
      /* Add coordinate variable to extraction list, dimension has already been output */
      if(dmn_item->xrf){
        for(jdx=0;jdx<xtr_nbr;jdx++)
          if(!strcmp(xtr_lst[jdx].nm,dmn_item->nm)) break;

        if(jdx != xtr_nbr) continue;
        /* If coordinate is not on list then add it to extraction list */
        xtr_lst=(nm_id_sct *)nco_realloc(xtr_lst,(xtr_nbr+1)*sizeof(nm_id_sct));
        xtr_lst[xtr_nbr].nm=(char *)strdup(dmn_item->nm);
        xtr_lst[xtr_nbr].id=dmn_item->cid;

        /* Increment */
        xtr_nbr++;

      } /* endif */
    } /* end loop over idx */	      
  } /* end if */ 

  /* Is this a CCM/CCSM/CF-format history tape? */
  CNV_CCM_CCSM_CF=nco_cnv_ccm_ccsm_cf_inq(in_id);

  /* Add coordinates defined by CF convention */
  if(CNV_CCM_CCSM_CF && (EXTRACT_ALL_COORDINATES || EXTRACT_ASSOCIATED_COORDINATES)) xtr_lst=nco_cnv_cf_crd_add(in_id,xtr_lst,&xtr_nbr);

  /* Subtract list A again (it may contain re-defined coordinates) */
  if(xtr_nbr > 0) xtr_lst=nco_var_lst_sub(xtr_lst,&xtr_nbr,xtr_lst_a,nbr_lst_a);

  /* Sort extraction list for faster I/O */
  if(xtr_nbr > 1) xtr_lst=nco_lst_srt_nm_id(xtr_lst,xtr_nbr,False);

  /* Write "fixed" variables */
  var=(var_sct **)nco_malloc(xtr_nbr*sizeof(var_sct *));
  var_out=(var_sct **)nco_malloc(xtr_nbr*sizeof(var_sct *));
  for(idx=0;idx<xtr_nbr;idx++){
    var[idx]=nco_var_fll(in_id,xtr_lst[idx].id,xtr_lst[idx].nm,&dmn_in_vtr[0],dmn_in_vtr.size());
    var_out[idx]=nco_var_dpl(var[idx]);
    (void)nco_xrf_var(var[idx],var_out[idx]);
    (void)nco_xrf_dmn(var_out[idx]);
  } /* end loop over idx */

  /* NB: ncap is not well-suited for nco_var_lst_dvd() */
  /* Divide variable lists into lists of fixed variables and variables to be processed */
  (void)nco_var_lst_dvd(var,var_out,xtr_nbr,CNV_CCM_CCSM_CF,True,nco_pck_plc_nil,nco_pck_map_nil,(dmn_sct **)NULL,(int)0,&var_fix,&var_fix_out,&nbr_var_fix,&var_prc,&var_prc_out,&nbr_var_prc,NULL);
  
  /* csz: Why not call this with var_fix? */
  /* Define non-processed vars */
  (void)nco_var_dfn(in_id,fl_out,out_id,var_out,xtr_nbr,(dmn_sct **)NULL,(int)0,nco_pck_plc_nil,nco_pck_map_nil,dfl_lvl);
  
  /* Write new attributes possibly overwriting old ones */
  for(idx=0;idx<var_vtr.size();idx++){
   
    if(var_vtr[idx]->xpr_typ == ncap_var){
      int rcd_inq_att;
      var_sct *var_ref;
      
      var_ref=var_vtr[idx]->var;
      rcd=nco_inq_varid_flg(out_id,var_ref->nm,&var_id);
      
      /* Filter-out RAM vars */ 
      if(rcd!=NC_NOERR || !var_ref->has_mss_val) continue;  
      
      /* Is missing value already present? */
      rcd_inq_att=nco_inq_att_flg(out_id,var_id,nco_mss_val_sng_get(),&att_item.type,&att_item.sz);

      /* Do not overwrite if type mismatch, e.g., packed variable are in output with -A switch */
      if(rcd_inq_att == NC_NOERR && var_ref->type != att_item.type) continue;   

      /* Fill-mode and attribute exists */ 
      if(prs_arg.NCAP4_FILL && rcd_inq_att == NC_NOERR){
        (void)nco_rename_att(out_id,var_id,nco_mss_val_sng_get(),att_nm_tmp);     
        (void)nco_put_att(out_id,var_id,att_nm_tmp,var_ref->type,1,var_ref->mss_val.vp);   
        (void)nco_rename_att(out_id,var_id,att_nm_tmp,nco_mss_val_sng_get());  
        continue;
      } /* end if */

      /* Fill-mode and attribute does not exist   */ 
      if(prs_arg.NCAP4_FILL && rcd_inq_att != NC_NOERR){
        (void)nco_put_att(out_id,var_id,att_nm_tmp,var_ref->type,1,var_ref->mss_val.vp);
        (void)nco_rename_att(out_id,var_id,att_nm_tmp,nco_mss_val_sng_get());
        continue;
      } /* end if */

      /* netCDF3 file so just put attribute */
      (void)nco_put_att(out_id,var_id,nco_mss_val_sng_get(),var_ref->type,1,var_ref->mss_val.vp);      
      continue;
    } /* endif */

     /* Write misssing value contained inside variable */
     /* If missing value type is same as disk type  */  
     /*
    if(var_vtr[idx]->xpr_typ == ncap_var ){
      if(!var_vtr[idx]->var->has_mss_val) continue;  
      att_item.att_nm=strdup(nco_mss_val_sng_get());
      att_item.var_nm=strdup(var_vtr[idx]->getVar().c_str());
      att_item.sz=1;
      att_item.type=var_vtr[idx]->var->type;
      att_item.val=var_vtr[idx]->var->mss_val;
      att_item.mode=aed_overwrite;
      //att_item.mode=aed_create;
    }
     */
    if(var_vtr[idx]->xpr_typ == ncap_att){
      /* Skip missing values (for now) */
      if(var_vtr[idx]->getAtt() == nco_mss_val_sng_get()) continue;     
    
      att_item.att_nm=strdup(var_vtr[idx]->getAtt().c_str());
      att_item.var_nm=strdup(var_vtr[idx]->getVar().c_str());
      att_item.sz=var_vtr[idx]->var->sz;
      att_item.type=var_vtr[idx]->var->type;
      att_item.val=var_vtr[idx]->var->val;
      att_item.mode=aed_overwrite;
    } 
   

    if(!strcmp(att_item.var_nm,"global")) 
      var_id=NC_GLOBAL;
    else{
      rcd=nco_inq_varid_flg(out_id,att_item.var_nm,&var_id);
      if(rcd != NC_NOERR)  goto cln_up;
    } /* end else */
    /* Check size */
    if(att_item.sz > NC_MAX_ATTRS ){ 
      (void)fprintf(stdout,"%s: Attribute %s size %ld excceeds maximum %d\n",nco_prg_nm_get(),att_item.att_nm,att_item.sz, NC_MAX_ATTRS );
      goto cln_up;
    } /* end if */
    /* NB: These attributes should probably be written prior to last data mode */
    (void)nco_aed_prc(out_id,var_id,att_item);
    
  cln_up:
    att_item.var_nm=(char*)nco_free(att_item.var_nm);
    att_item.att_nm=(char*)nco_free(att_item.att_nm);
  } /* end for */
  
  /* Set chunksize parameters */
  if(fl_out_fmt == NC_FORMAT_NETCDF4 || fl_out_fmt == NC_FORMAT_NETCDF4_CLASSIC) (void)nco_cnk_sz_set(out_id,(lmt_msa_sct **)NULL_CEWI,(int)0,&cnk_map,&cnk_plc,cnk_sz_scl,cnk.cnk_dmn,cnk_nbr);

  /* Turn-off default filling behavior to enhance efficiency */
  nco_set_fill(out_id,NC_NOFILL,&fll_md_old);
  
  /* Take output file out of define mode */
  (void)nco_enddef(out_id);
  
  /* Copy non-processed vars */
  (void)nco_var_val_cpy(in_id,out_id,var_fix,nbr_var_fix);
  
  /* Close input netCDF file */
  rcd=nco_close(in_id);
  
  /* Close files in all threads except main thread */
  for(idx=1;idx<thr_nbr;idx++){
    rcd=nco_close(prs_vtr[idx].in_id);
    rcd=nco_close(prs_vtr[idx].out_id_readonly);
  } /* end loop over threads */
  
  /* Remove local copy of file */
  if(FL_RTR_RMT_LCN && RM_RMT_FL_PST_PRC) (void)nco_fl_rm(fl_in);

  nco_close(prs_arg.out_id_readonly);
  
  /* Close output file and move it from temporary to permanent location */
  if(FL_OUT_NEW) (void)nco_fl_out_cls(fl_out,fl_out_tmp,out_id); else nco_close(out_id);
  
  /* Clean memory unless dirty memory allowed */
  if(flg_cln){
    /* ncap-specific memory */
    if(fl_spt_usr) fl_spt_usr=(char *)nco_free(fl_spt_usr);
    
    /* Free extraction lists */ 
    xtr_lst=nco_nm_id_lst_free(xtr_lst,xtr_nbr);
    xtr_lst_a=nco_nm_id_lst_free(xtr_lst_a,nbr_lst_a);
    
    /* Free command line algebraic arguments, if any */
    for(idx=0;idx<nbr_spt;idx++) spt_arg[idx]=(char *)nco_free(spt_arg[idx]);
    if(spt_arg_cat) spt_arg_cat=(char *)nco_free(spt_arg_cat);
    
    /* NCO-generic clean-up */
    /* Free individual strings/arrays */
    if(cmd_ln) cmd_ln=(char *)nco_free(cmd_ln);
    if(cnk_map_sng) cnk_map_sng=(char *)nco_free(cnk_map_sng);
    if(cnk_plc_sng) cnk_plc_sng=(char *)nco_free(cnk_plc_sng);
    if(fl_in) fl_in=(char*)nco_free(fl_in);
    if(fl_out) fl_out=(char *)nco_free(fl_out);
    if(fl_out_tmp) fl_out_tmp=(char *)nco_free(fl_out_tmp);
    if(fl_pth) fl_pth=(char *)nco_free(fl_pth);
    if(fl_pth_lcl) fl_pth_lcl=(char *)nco_free(fl_pth_lcl);
    /* Free lists of strings */
    if(fl_lst_in && fl_lst_abb == NULL_CEWI) fl_lst_in=nco_sng_lst_free(fl_lst_in,fl_nbr); 
    if(fl_lst_in && fl_lst_abb) fl_lst_in=nco_sng_lst_free(fl_lst_in,1);
    if(fl_lst_abb) fl_lst_abb=nco_sng_lst_free(fl_lst_abb,abb_arg_nbr);
    /* Free limits */
    for(idx=0;idx<lmt_nbr;idx++) lmt_arg[idx]=(char *)nco_free(lmt_arg[idx]);
    if(lmt_nbr > 0) lmt=nco_lmt_lst_free(lmt,lmt_nbr);
    /* Free chunking information */
    for(idx=0;idx<cnk_nbr;idx++) cnk_arg[idx]=(char *)nco_free(cnk_arg[idx]);
    if(cnk_nbr > 0) cnk.cnk_dmn=(cnk_dmn_sct **)nco_cnk_lst_free(cnk.cnk_dmn,cnk_nbr);
    /* Free dimension vectors */
    if(dmn_in_vtr.size() > 0) { 
      for(idx=0;idx<dmn_in_vtr.size();idx++)
        (void)nco_dmn_free(dmn_in_vtr[idx]);
    }
    if(dmn_out_vtr.size() > 0) { 
      for(idx=0;idx< dmn_out_vtr.size();idx++)
        (void)nco_dmn_free(dmn_out_vtr[idx]);
    }
    /* Free var_vtr */
    if(var_vtr.size() > 0) { 
      for(idx=0; idx < var_vtr.size(); idx++)
        delete var_vtr[idx];
    }  

    /* Clear vectors */
    /*
    fmc_vtr.clear();
    cnv_obj.fmc_vtr.clear();
    agg_obj.fmc_vtr.clear();
    utl_obj.fmc_vtr.clear();
    mth_obj.fmc_vtr.clear();
    mth2_obj.fmc_vtr.clear();
    bsc_obj.fmc_vtr.clear();
    pdq_obj.fmc_vtr.clear();
    msk_obj.fmc_vtr.clear();
    pck_obj.fmc_vtr.clear();     

    */
    /* Free variable lists */
    if(xtr_nbr > 0) var=nco_var_lst_free(var,xtr_nbr);
    if(xtr_nbr > 0) var_out=nco_var_lst_free(var_out,xtr_nbr);
    var_prc=(var_sct **)nco_free(var_prc);
    var_prc_out=(var_sct **)nco_free(var_prc_out);
    var_fix=(var_sct **)nco_free(var_fix);
    var_fix_out=(var_sct **)nco_free(var_fix_out);
  } /* !flg_cln */
  
  nco_exit_gracefully();
  return EXIT_SUCCESS;
} /* end main() */

// Copy vector elements
void 
pop_fmc_vtr
(std::vector<fmc_cls> &fmc_vtr, 
 vtl_cls *vfnc)
{
  // De-reference
  std::vector<fmc_cls> &lcl_vtr=*vfnc->lst_vtr();
  std::copy(lcl_vtr.begin(),lcl_vtr.end(),inserter(fmc_vtr,fmc_vtr.end())  );
}

// Add some useful constants as RAM variables 
void
ram_vars_add
(prs_cls *prs_arg)
{
  var_sct *var1;
  
  var1=ncap_sclr_var_mk(std::string("__BYTE"),nco_int(NC_BYTE));
  prs_arg->ncap_var_write(var1,true);
  
  var1=ncap_sclr_var_mk(std::string("__CHAR"),nco_int(NC_CHAR));
  prs_arg->ncap_var_write(var1,true);
  
  var1=ncap_sclr_var_mk(std::string("__SHORT"),nco_int(NC_SHORT));
  prs_arg->ncap_var_write(var1,true);
  
  var1=ncap_sclr_var_mk(std::string("__INT"),nco_int(NC_INT));
  prs_arg->ncap_var_write(var1,true);
  
  var1=ncap_sclr_var_mk(std::string("__FLOAT"),nco_int(NC_FLOAT));
  prs_arg->ncap_var_write(var1,true);
  
  var1=ncap_sclr_var_mk(std::string("__DOUBLE"),nco_int(NC_DOUBLE));
  prs_arg->ncap_var_write(var1,true);
  
#ifdef ENABLE_NETCDF4
  var1=ncap_sclr_var_mk(std::string("__UBYTE"),nco_int(NC_UBYTE));
  prs_arg->ncap_var_write(var1,true); 
  
  var1=ncap_sclr_var_mk(std::string("__USHORT"),nco_int(NC_USHORT));
  prs_arg->ncap_var_write(var1,true);
  
  var1=ncap_sclr_var_mk(std::string("__UINT"),nco_int(NC_UINT));
  prs_arg->ncap_var_write(var1,true);
  
  var1=ncap_sclr_var_mk(std::string("__INT64"),nco_int(NC_INT64));
  prs_arg->ncap_var_write(var1,true);
  
  var1=ncap_sclr_var_mk(std::string("__UINT64"),nco_int(NC_UINT64));
  prs_arg->ncap_var_write(var1,true);

#endif // !ENABLE_NETCDF4
  
  #ifdef INFINITY
  var1=ncap_sclr_var_mk(std::string("inff"),INFINITY); //float
  prs_arg->ncap_var_write(var1,true);
  #endif

  #ifdef NAN
  var1=ncap_sclr_var_mk(std::string("nanf"),NAN);    //float
  prs_arg->ncap_var_write(var1,true);
  #endif

  #ifdef HUGE_VAL
  var1=ncap_sclr_var_mk(std::string("inf"),HUGE_VAL);    //double
  prs_arg->ncap_var_write(var1,true);
  #endif

  char buff[20];
  double dnan;
#ifndef _MSC_VER
  if( (dnan=nan(buff)) ){
    var1=ncap_sclr_var_mk(std::string("nan"),dnan);    //double
    prs_arg->ncap_var_write(var1,true);
  }
#endif

} // end ram_vars_add()
