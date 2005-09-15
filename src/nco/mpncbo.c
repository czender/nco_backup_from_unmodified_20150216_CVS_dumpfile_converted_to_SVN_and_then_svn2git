/* $Header: /data/zender/nco_20150216/nco/src/nco/mpncbo.c,v 1.27 2005-09-15 22:21:35 zender Exp $ */

/* mpncbo -- netCDF binary operator */

/* Purpose: Compute sum, difference, product, or ratio of specified hyperslabs of specfied variables
   from two input netCDF files and output them to a single file. */

/* Copyright (C) 1995--2005 Charlie Zender
   
   This software may be modified and/or re-distributed under the terms of the GNU General Public License (GPL) Version 2
   The full license text is at http://www.gnu.ai.mit.edu/copyleft/gpl.html 
   and in the file nco/doc/LICENSE in the NCO source distribution.
   
   As a special exception to the terms of the GPL, you are permitted 
   to link the NCO source code with the HDF, netCDF, OPeNDAP, and UDUnits
   libraries and to distribute the resulting executables under the terms 
   of the GPL, but in addition obeying the extra stipulations of the 
   HDF, netCDF, OPeNDAP, and UDUnits licenses.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
   See the GNU General Public License for more details.
   
   The original author of this software, Charlie Zender, wants to improve it
   with the help of your suggestions, improvements, bug-reports, and patches.
   Please contact the NCO project at http://nco.sf.net or write to
   Charlie Zender
   Department of Earth System Science
   University of California at Irvine
   Irvine, CA 92697-3100 */

/* Usage:
   ncbo -O in.nc in.nc foo.nc
   ncbo -O -v mss_val in.nc in.nc foo.nc
   ncbo -p /data/zender/tmp h0001.nc foo.nc
   ncbo -p /data/zender/tmp -l /data/zender/tmp/rmt h0001.nc h0002.nc foo.nc
   ncbo -p /ZENDER/tmp -l /data/zender/tmp/rmt h0001.nc h0002.nc foo.nc
   ncbo -p /ZENDER/tmp -l /usr/tmp/zender h0001.nc h0002.nc foo.nc
   
   Test type conversion:
   ncks -O -C -v float_var in.nc foo1.nc
   ncrename -v float_var,double_var foo1.nc
   ncks -O -C -v double_var in.nc foo2.nc
   ncbo -O -C -v double_var foo1.nc foo2.nc foo3.nc
   ncbo -O -C -v double_var foo2.nc foo1.nc foo4.nc
   ncks -H -m foo1.nc
   ncks -H -m foo2.nc
   ncks -H -m foo3.nc
   ncks -H -m foo4.nc
   
   Test nco_var_cnf_dmn:
   ncks -O -v scalar_var in.nc foo.nc ; ncrename -v scalar_var,four_dmn_rec_var foo.nc ; ncbo -O -v four_dmn_rec_var in.nc foo.nc foo2.nc */
#ifdef HAVE_CONFIG_H
#include <config.h> /* Autotools tokens */
#endif /* !HAVE_CONFIG_H */

/* Standard C headers */
#include <math.h> /* sin cos cos sin 3.14159 */
#include <stdio.h> /* stderr, FILE, NULL, etc. */
#include <stdlib.h> /* atof, atoi, malloc, getopt */
#include <string.h> /* strcmp. . . */
#include <sys/stat.h> /* stat() */
#include <time.h> /* machine time */
#include <unistd.h> /* all sorts of POSIX stuff */
#ifndef HAVE_GETOPT_LONG
#include "nco_getopt.h"
#else /* !NEED_GETOPT_LONG */ 
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif /* !HAVE_GETOPT_H */ 
#endif /* HAVE_GETOPT_LONG */

/* 3rd party vendors */
#include <netcdf.h> /* netCDF definitions and C library */
#ifdef ENABLE_MPI
#include <mpi.h> /* MPI definitions */
#endif /* !ENABLE_MPI */

/* Personal headers */
/* #define MAIN_PROGRAM_FILE MUST precede #include libnco.h */
#define MAIN_PROGRAM_FILE
#include "libnco.h" /* netCDF Operator (NCO) library */

int 
main(int argc,char **argv)
{
  bool EXCLUDE_INPUT_LIST=False; /* Option c */
  bool EXTRACT_ALL_COORDINATES=False; /* Option c */
  bool EXTRACT_ASSOCIATED_COORDINATES=True; /* Option C */
  bool FILE_1_RETRIEVED_FROM_REMOTE_LOCATION;
  bool FILE_2_RETRIEVED_FROM_REMOTE_LOCATION;
  bool FL_LST_IN_FROM_STDIN=False; /* [flg] fl_lst_in comes from stdin */
  bool FMT_64BIT=False; /* Option Z */
  bool FORCE_APPEND=False; /* Option A */
  bool FORCE_OVERWRITE=False; /* Option O */
  bool FORTRAN_IDX_CNV=False; /* Option F */
  bool HISTORY_APPEND=True; /* Option h */
  bool CNV_CCM_CCSM_CF;
  bool REMOVE_REMOTE_FILES_AFTER_PROCESSING=True; /* Option R */

  char **fl_lst_abb=NULL; /* Option a */
  char **fl_lst_in;
  char **var_lst_in=NULL_CEWI;
  char *cmd_ln;
  char *fl_in_1=NULL; /* fl_in_1 is nco_realloc'd when not NULL */;
  char *fl_in_2=NULL; /* fl_in_2 is nco_realloc'd when not NULL */;
  char *fl_out=NULL; /* Option o */
  char *fl_out_tmp=NULL; /* MPI CEWI */
  char *fl_pth=NULL; /* Option p */
  char *fl_pth_lcl=NULL; /* Option l */
  char *lmt_arg[NC_MAX_DIMS];
  char *nco_op_typ_sng=NULL; /* [sng] Operation type */
  char *optarg_lcl=NULL; /* [sng] Local copy of system optarg */
  char *time_bfr_srt;
  
  const char * const CVS_Id="$Id: mpncbo.c,v 1.27 2005-09-15 22:21:35 zender Exp $"; 
  const char * const CVS_Revision="$Revision: 1.27 $";
  const char * const opt_sht_lst="4ACcD:d:Fhl:Oo:p:rRt:v:xy:Z-:";

  dmn_sct **dim_1;
  dmn_sct **dim_2;
  dmn_sct **dmn_out;
  
  extern char *optarg;
  extern int optind;
  
  /* Using naked stdin/stdout/stderr in parallel region generates warning
     Copy appropriate filehandle to variable scoped shared in parallel clause */
  FILE * const fp_stderr=stderr; /* [fl] stderr filehandle CEWI */
  FILE * const fp_stdout=stdout; /* [fl] stdout filehandle CEWI */

  int abb_arg_nbr=0;
  int fl_idx;
  int fl_nbr=0;
  int fll_md_old; /* [enm] Old fill mode */
  int idx;
  int in_id_1;  
  int in_id_2;  
  int lmt_nbr=0; /* Option d. NB: lmt_nbr gets incremented */
  int nbr_dmn_fl_1;
  int nbr_dmn_fl_2;
  int nbr_dmn_xtr_1;
  int nbr_dmn_xtr_2;
  int nbr_var_fix_1; /* nbr_var_fix_1 gets incremented */
  int nbr_var_fix_2; /* nbr_var_fix_2 gets incremented */
  int nbr_var_fl_1;
  int nbr_var_fl_2;
  int nbr_var_prc_1; /* nbr_var_prc_1 gets incremented */
  int nbr_var_prc_2; /* nbr_var_prc_2 gets incremented */
  int nbr_xtr_1=0; /* nbr_xtr_1 won't otherwise be set for -c with no -v */
  int nbr_xtr_2=0; /* nbr_xtr_2 won't otherwise be set for -c with no -v */
  int nco_op_typ=nco_op_nil; /* [enm] Operation type */
  int opt;
  int out_id;  
  int rcd=NC_NOERR; /* [rcd] Return code */
  int thr_nbr=0; /* [nbr] Thread number Option t */
  int var_lst_in_nbr=0;

  lmt_sct **lmt;

  nm_id_sct *dmn_lst_1;
  nm_id_sct *dmn_lst_2;
  nm_id_sct *xtr_lst_1=NULL; /* xtr_lst_1 may be alloc()'d from NULL with -c option */
  nm_id_sct *xtr_lst_2=NULL; /* xtr_lst_2 may be alloc()'d from NULL with -c option */
  
  time_t time_crr_time_t;
  
  var_sct **var_1;
  var_sct **var_2;
  var_sct **var_fix_1;
  var_sct **var_fix_2;
  var_sct **var_fix_out;
  var_sct **var_out;
  var_sct **var_prc_1;
  var_sct **var_prc_2;
  var_sct **var_prc_out;
  
#ifdef ENABLE_MPI
  /* Declare all MPI-specific variables here */
  MPI_Status mpi_stt; /* [enm] Status check to decode msg_typ */

  bool TOKEN_FREE=True; /* [flg] Allow MPI workers write-access to output file */

  const double sleep_tm=0.04; /* [s] Token request interval */

  const int info_bfr_lng=3; /* [nbr] Number of elements in info_bfr */
  const int mpi_rnk_root=0; /* [enm] Rank of broadcast root */
  const int wrk_id_bfr_lng=1; /* [nbr] Number of elements in wrk_id_bfr */

  int fl_nm_lng; /* [nbr] Output file name length CEWI */
  int info_bfr[3]; /* [bfr] Buffer containing var, idx, tkn_rsp */
  int msg_typ; /* [enm] MPI message type */
  int proc_id; /* [id] Process ID */
  int proc_nbr=0; /* [nbr] Number of MPI processes */
  int tkn_rsp; /* [enm] Mangager response [0,1] = [Wait,Allow] */
  int var_wrt_nbr=0; /* [nbr] Variables written to output file until now */
  int wrk_id; /* [id] Sender node ID */
  int wrk_id_bfr[1]; /* [bfr] Buffer for wrk_id */
#endif /* !ENABLE_MPI */
  
  static struct option opt_lng[]=
    { /* Structure ordered by short option key if possible */
      {"append",no_argument,0,'A'},
      {"coords",no_argument,0,'c'},
      {"crd",no_argument,0,'c'},
      {"no-coords",no_argument,0,'C'},
      {"no-crd",no_argument,0,'C'},
      {"debug",required_argument,0,'D'},
      {"dbg_lvl",required_argument,0,'D'},
      {"dimension",required_argument,0,'d'},
      {"dmn",required_argument,0,'d'},
      {"fortran",no_argument,0,'F'},
      {"ftn",no_argument,0,'F'},
      {"history",no_argument,0,'h'},
      {"hst",no_argument,0,'h'},
      {"local",required_argument,0,'l'},
      {"lcl",required_argument,0,'l'},
      {"overwrite",no_argument,0,'O'},
      {"ovr",no_argument,0,'O'},
      {"path",required_argument,0,'p'},
      {"retain",no_argument,0,'R'},
      {"rtn",no_argument,0,'R'},
      {"revision",no_argument,0,'r'},
      {"variable",required_argument,0,'v'},
      {"version",no_argument,0,'r'},
      {"vrs",no_argument,0,'r'},
      {"thr_nbr",required_argument,0,'t'},
      {"exclude",no_argument,0,'x'},
      {"xcl",no_argument,0,'x'},
      {"operation",required_argument,0,'y'},
      {"op_typ",required_argument,0,'y'},
      {"64-bit-offset",no_argument,0,'Z'},
      {"help",no_argument,0,'?'},
      {0,0,0,0}
    }; /* end opt_lng */
  int opt_idx=0; /* Index of current long option into opt_lng array */

#ifdef ENABLE_MPI 
  /* MPI Initialization */
  MPI_Init(&argc,&argv);
  MPI_Comm_size(MPI_COMM_WORLD,&proc_nbr);
  MPI_Comm_rank(MPI_COMM_WORLD,&proc_id);
#endif /* !ENABLE_MPI */

  /* Start clock and save command line */ 
  cmd_ln=nco_cmd_ln_sng(argc,argv);
  time_crr_time_t=time((time_t *)NULL);
  time_bfr_srt=ctime(&time_crr_time_t); time_bfr_srt=time_bfr_srt; /* Avoid compiler warning until variable is used for something */
 
  /* Get program name and set program enum (e.g., prg=ncra) */
  prg_nm=prg_prs(argv[0],&prg);
  
  /* Parse command line arguments */
  while((opt = getopt_long(argc,argv,opt_sht_lst,opt_lng,&opt_idx)) != EOF){
    switch(opt){
    case 'A': /* Toggle FORCE_APPEND */
      FORCE_APPEND=!FORCE_APPEND;
      break;
    case 'C': /* Extract all coordinates associated with extracted variables? */
      EXTRACT_ASSOCIATED_COORDINATES=False;
      break;
    case 'c':
      EXTRACT_ALL_COORDINATES=True;
      break;
    case 'D': /* The debugging level. Default is 0. */
      dbg_lvl=(unsigned short)strtol(optarg,(char **)NULL,10);
      break;
    case 'd': /* Copy argument for later processing */
      lmt_arg[lmt_nbr]=(char *)strdup(optarg);
      lmt_nbr++;
      break;
    case 'F': /* Toggle index convention. Default is 0-based arrays (C-style). */
      FORTRAN_IDX_CNV=!FORTRAN_IDX_CNV;
      break;
    case 'h': /* Toggle appending to history global attribute */
      HISTORY_APPEND=!HISTORY_APPEND;
      break;
    case 'l': /* Local path prefix for files retrieved from remote file system */
      fl_pth_lcl=(char *)strdup(optarg);
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
      REMOVE_REMOTE_FILES_AFTER_PROCESSING=!REMOVE_REMOTE_FILES_AFTER_PROCESSING;
      break;
    case 'r': /* Print CVS program information and copyright notice */
      (void)copyright_prn(CVS_Id,CVS_Revision);
      (void)nco_lbr_vrs_prn();
      nco_exit(EXIT_SUCCESS);
      break;
    case 't': /* Thread number */
      thr_nbr=(int)strtol(optarg,(char **)NULL,10);
      break;
    case 'v': /* Variables to extract/exclude */
      /* Replace commas with hashes when within braces (convert back later) */
      optarg_lcl=(char *)strdup(optarg);
      (void)nco_lst_comma2hash(optarg_lcl);
      var_lst_in=lst_prs_2D(optarg_lcl,",",&var_lst_in_nbr);
      optarg_lcl=(char *)nco_free(optarg_lcl);
      nbr_xtr_1=nbr_xtr_2=var_lst_in_nbr;
      break;
    case 'x': /* Exclude rather than extract variables specified with -v */
      EXCLUDE_INPUT_LIST=True;
      break;
    case 'y': /* User-specified operation type overrides invocation default */
      nco_op_typ_sng=(char *)strdup(optarg);
      nco_op_typ=nco_op_typ_get(nco_op_typ_sng);
      break;
    case 'Z': /* [flg] Create output file with 64-bit offsets */
      FMT_64BIT=True;
      break;
    case '?': /* Print proper usage */
      (void)nco_usg_prn();
      nco_exit(EXIT_SUCCESS);
      break;
    case '-': /* Long options are not allowed */
      (void)fprintf(stderr,"%s: ERROR Long options are not available in this build. Use single letter options instead.\n",prg_nm_get());
      nco_exit(EXIT_FAILURE);
      break;
    default: /* Print proper usage */
      (void)nco_usg_prn();
      nco_exit(EXIT_FAILURE);
      break;
    } /* end switch */
  } /* end while loop */
  
  /* Process positional arguments and fill in filenames */
  fl_lst_in=nco_fl_lst_mk(argv,argc,optind,&fl_nbr,&fl_out,&FL_LST_IN_FROM_STDIN);
  
  /* Make uniform list of user-specified dimension limits */
  lmt=nco_lmt_prs(lmt_nbr,lmt_arg);
  
  /* Parse filenames */
  fl_idx=0; /* Input file _1 */
  fl_in_1=nco_fl_nm_prs(fl_in_1,fl_idx,&fl_nbr,fl_lst_in,abb_arg_nbr,fl_lst_abb,fl_pth);
  if(dbg_lvl > 0) (void)fprintf(stderr,"\nInput file %d is %s; ",fl_idx,fl_in_1);
  /* Make sure file is on local system and is readable or die trying */
  fl_in_1=nco_fl_mk_lcl(fl_in_1,fl_pth_lcl,&FILE_1_RETRIEVED_FROM_REMOTE_LOCATION);
  if(dbg_lvl > 0) (void)fprintf(stderr,"local file %s:\n",fl_in_1);
  rcd=nco_open(fl_in_1,NC_NOWRITE,&in_id_1);

  fl_idx=1; /* Input file _2 */
  fl_in_2=nco_fl_nm_prs(fl_in_2,fl_idx,&fl_nbr,fl_lst_in,abb_arg_nbr,fl_lst_abb,fl_pth);
  if(dbg_lvl > 0) (void)fprintf(stderr,"\nInput file %d is %s; ",fl_idx,fl_in_2);
  /* Make sure file is on local system and is readable or die trying */
  fl_in_2=nco_fl_mk_lcl(fl_in_2,fl_pth_lcl,&FILE_2_RETRIEVED_FROM_REMOTE_LOCATION);
  if(dbg_lvl > 0) (void)fprintf(stderr,"local file %s:\n",fl_in_2);
  rcd=nco_open(fl_in_2,NC_NOWRITE,&in_id_2);
  
  /* Get number of variables and dimensions in file */
  (void)nco_inq(in_id_1,&nbr_dmn_fl_1,&nbr_var_fl_1,(int *)NULL,(int *)NULL);
  (void)nco_inq(in_id_2,&nbr_dmn_fl_2,&nbr_var_fl_2,(int *)NULL,(int *)NULL);
  
  /* Form initial extraction list which may include extended regular expressions */
  xtr_lst_1=nco_var_lst_mk(in_id_1,nbr_var_fl_1,var_lst_in,EXTRACT_ALL_COORDINATES,&nbr_xtr_1);
  xtr_lst_2=nco_var_lst_mk(in_id_2,nbr_var_fl_2,var_lst_in,EXTRACT_ALL_COORDINATES,&nbr_xtr_2);
  
  /* Change included variables to excluded variables */
  if(EXCLUDE_INPUT_LIST) xtr_lst_1=nco_var_lst_xcl(in_id_1,nbr_var_fl_1,xtr_lst_1,&nbr_xtr_1);
  if(EXCLUDE_INPUT_LIST) xtr_lst_2=nco_var_lst_xcl(in_id_2,nbr_var_fl_2,xtr_lst_2,&nbr_xtr_2);
  
  /* Add all coordinate variables to extraction list */
  if(EXTRACT_ALL_COORDINATES) xtr_lst_1=nco_var_lst_add_crd(in_id_1,nbr_dmn_fl_1,xtr_lst_1,&nbr_xtr_1);
  if(EXTRACT_ALL_COORDINATES) xtr_lst_2=nco_var_lst_add_crd(in_id_2,nbr_dmn_fl_2,xtr_lst_2,&nbr_xtr_2);
  
  /* Make sure coordinates associated extracted variables are also on extraction list */
  if(EXTRACT_ASSOCIATED_COORDINATES) xtr_lst_1=nco_var_lst_ass_crd_add(in_id_1,xtr_lst_1,&nbr_xtr_1);
  if(EXTRACT_ASSOCIATED_COORDINATES) xtr_lst_2=nco_var_lst_ass_crd_add(in_id_2,xtr_lst_2,&nbr_xtr_2);
  
  /* With fully symmetric 1<->2 ordering, may occasionally find nbr_xtr_2 > nbr_xtr_1 
     This occurs, e.g., when fl_in_1 contains reduced variables and full coordinates
     are only in fl_in_2 and so will not appear xtr_lst_1 */
  
  /* Sort extraction list by variable ID for fastest I/O */
  if(nbr_xtr_1 > 1) xtr_lst_1=nco_lst_srt_nm_id(xtr_lst_1,nbr_xtr_1,False);
  if(nbr_xtr_2 > 1) xtr_lst_2=nco_lst_srt_nm_id(xtr_lst_2,nbr_xtr_2,False);
  
  /* We now have final list of variables to extract. Phew. */
  
  /* Find coordinate/dimension values associated with user-specified limits */
  for(idx=0;idx<lmt_nbr;idx++) (void)nco_lmt_evl(in_id_1,lmt[idx],0L,FORTRAN_IDX_CNV);

  /* Find dimensions associated with variables to be extracted */
  dmn_lst_1=nco_dmn_lst_ass_var(in_id_1,xtr_lst_1,nbr_xtr_1,&nbr_dmn_xtr_1);
  dmn_lst_2=nco_dmn_lst_ass_var(in_id_2,xtr_lst_2,nbr_xtr_2,&nbr_dmn_xtr_2);
  
  /* Fill in dimension structure for all extracted dimensions */
  dim_1=(dmn_sct **)nco_malloc(nbr_dmn_xtr_1*sizeof(dmn_sct *));
  dim_2=(dmn_sct **)nco_malloc(nbr_dmn_xtr_2*sizeof(dmn_sct *));
  for(idx=0;idx<nbr_dmn_xtr_1;idx++) dim_1[idx]=nco_dmn_fll(in_id_1,dmn_lst_1[idx].id,dmn_lst_1[idx].nm);
  for(idx=0;idx<nbr_dmn_xtr_2;idx++) dim_2[idx]=nco_dmn_fll(in_id_2,dmn_lst_2[idx].id,dmn_lst_2[idx].nm);
  /* Dimension lists no longer needed */
  dmn_lst_1=nco_nm_id_lst_free(dmn_lst_1,nbr_dmn_xtr_1);
  dmn_lst_2=nco_nm_id_lst_free(dmn_lst_2,nbr_dmn_xtr_2);
  
  /* Merge hyperslab limit information into dimension structures */
  if(lmt_nbr > 0) (void)nco_dmn_lmt_mrg(dim_1,nbr_dmn_xtr_1,lmt,lmt_nbr);
  if(lmt_nbr > 0) (void)nco_dmn_lmt_mrg(dim_2,nbr_dmn_xtr_2,lmt,lmt_nbr);
  
  /* Duplicate input dimension structures for output dimension structures */
  dmn_out=(dmn_sct **)nco_malloc(nbr_dmn_xtr_1*sizeof(dmn_sct *));
  for(idx=0;idx<nbr_dmn_xtr_1;idx++){
    dmn_out[idx]=nco_dmn_dpl(dim_1[idx]);
    (void)nco_dmn_xrf(dim_1[idx],dmn_out[idx]); 
  } /* end loop over idx */
  
  if(dbg_lvl > 3){
    for(idx=0;idx<nbr_xtr_1;idx++) (void)fprintf(stderr,"xtr_lst_1[%d].nm = %s, .id= %d\n",idx,xtr_lst_1[idx].nm,xtr_lst_1[idx].id);
  } /* end if */
  
  /* Is this an CCM/CCSM/CF-format history tape? */
  CNV_CCM_CCSM_CF=nco_cnv_ccm_ccsm_cf_inq(in_id_1);
  
  /* Fill in variable structure list for all extracted variables */
  var_1=(var_sct **)nco_malloc(nbr_xtr_1*sizeof(var_sct *));
  var_2=(var_sct **)nco_malloc(nbr_xtr_2*sizeof(var_sct *));
  var_out=(var_sct **)nco_malloc(nbr_xtr_1*sizeof(var_sct *));
  for(idx=0;idx<nbr_xtr_1;idx++){
    var_1[idx]=nco_var_fll(in_id_1,xtr_lst_1[idx].id,xtr_lst_1[idx].nm,dim_1,nbr_dmn_xtr_1);
    var_out[idx]=nco_var_dpl(var_1[idx]);
    (void)nco_xrf_var(var_1[idx],var_out[idx]);
    (void)nco_xrf_dmn(var_out[idx]);
  } /* end loop over idx */
  for(idx=0;idx<nbr_xtr_2;idx++){
    var_2[idx]=nco_var_fll(in_id_2,xtr_lst_2[idx].id,xtr_lst_2[idx].nm,dim_2,nbr_dmn_xtr_2);
  } /* end loop over idx */

  /* Extraction lists no longer needed */
  xtr_lst_1=nco_nm_id_lst_free(xtr_lst_1,nbr_xtr_1);
  xtr_lst_2=nco_nm_id_lst_free(xtr_lst_2,nbr_xtr_2);
  
  /* Divide variable lists into lists of fixed variables and variables to be processed
     Create lists from file_1 last so those values remain in *_out arrays */
  (void)nco_var_lst_dvd(var_2,var_out,nbr_xtr_2,CNV_CCM_CCSM_CF,nco_pck_plc_nil,nco_pck_map_nil,(dmn_sct **)NULL,0,&var_fix_2,&var_fix_out,&nbr_var_fix_2,&var_prc_2,&var_prc_out,&nbr_var_prc_2);
  /* Avoid double-free() condition */
  var_fix_out=(var_sct **)nco_free(var_fix_out);
  var_prc_out=(var_sct **)nco_free(var_prc_out);
  (void)nco_var_lst_dvd(var_1,var_out,nbr_xtr_1,CNV_CCM_CCSM_CF,nco_pck_plc_nil,nco_pck_map_nil,(dmn_sct **)NULL,0,&var_fix_1,&var_fix_out,&nbr_var_fix_1,&var_prc_1,&var_prc_out,&nbr_var_prc_1);

  /* Die gracefully on unsupported features... */
  if(nbr_var_fix_1 < nbr_var_fix_2){
    (void)fprintf(fp_stdout,"%s: ERROR First file has fewer fixed variables than second file (%d < %d). This feature is NCO TODO 581.\n",prg_nm,nbr_var_fix_1,nbr_var_fix_2);
      nco_exit(EXIT_FAILURE);
  } /* endif */

  /* Merge two variable lists into same order */
  rcd=nco_var_lst_mrg(&var_prc_1,&var_prc_2,&nbr_var_prc_1,&nbr_var_prc_2); 

  /* Zero start vectors for all output variables */
  (void)nco_var_srt_zero(var_out,nbr_xtr_1);

#ifdef ENABLE_MPI 
  if(proc_id == 0){ /* MPI manager code */
#endif /* !ENABLE_MPI */
    /* Open output file */
    fl_out_tmp=nco_fl_out_open(fl_out,FORCE_APPEND,FORCE_OVERWRITE,FMT_64BIT,&out_id);
    
    /* Copy global attributes */
    (void)nco_att_cpy(in_id_1,out_id,NC_GLOBAL,NC_GLOBAL,True);
    
    /* Catenate time-stamped command line to "history" global attribute */
    if(HISTORY_APPEND) (void)nco_hst_att_cat(out_id,cmd_ln);
    
    /* Initialize thread information */
    thr_nbr=nco_openmp_ini(thr_nbr);
    if(thr_nbr > 0 && HISTORY_APPEND) (void)nco_thr_att_cat(out_id,thr_nbr);
    
#ifdef ENABLE_MPI 
    /* Initialize MPI task information */
    if(proc_nbr > 0 && HISTORY_APPEND) (void)nco_mpi_att_cat(out_id,proc_nbr);
#endif /* !ENABLE_MPI */
    
    /* Define dimensions in output file */
    (void)nco_dmn_dfn(fl_out,out_id,dmn_out,nbr_dmn_xtr_1);
    
    /* fxm: TODO 550 put max_dim_sz/list(var_1,var_2) into var_def(var_out) */
    /* Define variables in output file, copy their attributes */
    (void)nco_var_dfn(in_id_1,fl_out,out_id,var_out,nbr_xtr_1,(dmn_sct **)NULL,(int)0,nco_pck_plc_nil,nco_pck_map_nil);
    
    /* Turn off default filling behavior to enhance efficiency */
    rcd=nco_set_fill(out_id,NC_NOFILL,&fll_md_old);
    
    /* Take output file out of define mode */
    (void)nco_enddef(out_id);

#ifdef ENABLE_MPI
  } /* proc_id != 0 */
  
  /* Manager obtains output filename and broadcasts to workers */
  if(proc_id == 0) fl_nm_lng=(int)strlen(fl_out_tmp); 
  MPI_Bcast(&fl_nm_lng,1,MPI_INT,mpi_rnk_root,MPI_COMM_WORLD);
  if(proc_id != 0) fl_out_tmp=(char *)malloc((fl_nm_lng+1)*sizeof(char));
  MPI_Bcast(fl_out_tmp,fl_nm_lng+1,MPI_CHAR,mpi_rnk_root,MPI_COMM_WORLD); 
  
  if(proc_id == 0){ /* MPI manager code */
    TOKEN_FREE=False;
#endif /* !ENABLE_MPI */
    /* Copy variable data for non-processed variables */
    (void)nco_var_val_cpy(in_id_1,out_id,var_fix_1,nbr_var_fix_1);
#ifdef ENABLE_MPI
    /* Close output file so workers can open it */
    nco_close(out_id);
    TOKEN_FREE=True;
  } /* proc_id != 0 */
#endif /* !ENABLE_MPI */
  
  /* ncbo() code has been similar to ncea() (and ncra()) wherever possible
     Major differences occur where performance would otherwise suffer
     From now on, however, binary-file and binary-operation nature of ncbo()
     is too different from ncea() paradigm to justify following ncea() style.
     Instead, we adopt symmetric nomenclature (e.g., file_1, file_2), and 
     perform differences variable-by-variable so peak memory usage goes as
     Order(2*maximum variable size) rather than Order(3*maximum record size) or
     Order(3*file size) */
     
  /* Perform various error-checks on input file */
  if(False) (void)nco_fl_cmp_err_chk();
  
  /* Default operation depends on invocation name */
  if(nco_op_typ_sng == NULL) nco_op_typ=nco_op_typ_get(nco_op_typ_sng);
  
#ifdef ENABLE_MPI
  if(proc_id == 0){ /* MPI manager code */
    /* Compensate for incrementing on each worker's first message */
    var_wrt_nbr=-proc_nbr+1;
    idx=0;
    /* While variables remain to be processed or written... */
    while(var_wrt_nbr < nbr_var_prc_1){
      /* Receive message from any worker */
      MPI_Recv(wrk_id_bfr,wrk_id_bfr_lng,MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&mpi_stt);
      /* Obtain MPI message type */
      msg_typ=mpi_stt.MPI_TAG;
      /* Get sender's proc_id */ 
      wrk_id=wrk_id_bfr[0];
      
      /* Allocate next variable, if any, to worker */
      if(msg_typ == WORK_REQUEST){
	var_wrt_nbr++; /* [nbr] Number of variables written */
	/* Worker closed output file before sending WORK_REQUEST */
	TOKEN_FREE=True;
	
	if(idx < nbr_var_prc_1){
	  /* Tell requesting worker to allocate space for next variable */
	  info_bfr[0]=idx; /* [idx] Variable to be processed */
	  info_bfr[1]=out_id; /* Output file ID */
	  info_bfr[2]=var_prc_out[idx]->id; /* [id] Variable ID in output file */
	  /* Point to next variable on list */
	  idx++; 
	}else{
	  /* Variable index = -1 indicates NO_MORE_WORK */
	  info_bfr[0]=NO_MORE_WORK; /* [idx] -1 */
	  info_bfr[1]=out_id; /* Output file ID */
	} /* endif idx */
	MPI_Send(info_bfr,info_bfr_lng,MPI_INT,wrk_id,WORK_ALLOC,MPI_COMM_WORLD);
	/* msg_typ != WORK_REQUEST */
      }else if(msg_typ == TOKEN_REQUEST){ 
	/* Allocate token if free, else ask worker to try later */
	if(TOKEN_FREE){
	  TOKEN_FREE=False;
	  info_bfr[0]=1; /* Allow */
	}else{
	  info_bfr[0]=0; /* Wait */
	} /* !TOKEN_FREE */
	MPI_Send(info_bfr,info_bfr_lng,MPI_INT,wrk_id,TOKEN_RESULT,MPI_COMM_WORLD);
      } /* msg_typ != TOKEN_REQUEST */
    } /* end while var_wrt_nbr < nbr_var_prc_1 */
  }else{ /* proc_id != 0, end Manager code begin Worker code */
    wrk_id_bfr[0]=proc_id;
    while(1){ /* While work remains... */
      /* Send WORK_REQUEST */
      wrk_id_bfr[0]=proc_id;
      MPI_Send(wrk_id_bfr,wrk_id_bfr_lng,MPI_INT,mgr_id,WORK_REQUEST,MPI_COMM_WORLD);
      /* Receive WORK_ALLOC */
      MPI_Recv(info_bfr,info_bfr_lng,MPI_INT,0,WORK_ALLOC,MPI_COMM_WORLD,&mpi_stt);
      idx=info_bfr[0];
      out_id=info_bfr[1];
      if(idx == NO_MORE_WORK) break;
      else{ 
	var_prc_out[idx]->id=info_bfr[2];
      /* Process this variable same as UP code */
#else /* !ENABLE_MPI */
#ifdef _OPENMP
     /* OpenMP notes:
     shared(): msk and wgt are not altered within loop
     private(): wgt_avg does not need initialization */
#pragma omp parallel for default(none) private(idx) shared(dbg_lvl,dim_1,fl_in_1,fl_in_2,fl_out,fp_stderr,in_id_1,in_id_2,nbr_dmn_xtr_1,nbr_var_prc_1,nbr_var_prc_2,nco_op_typ,out_id,prg_nm,var_prc_1,var_prc_2,var_prc_out)
#endif /* !_OPENMP */
      /* UP and SMP codes main loop over variables */ 
      for(idx=0;idx<nbr_var_prc_1;idx++){
#endif /* ENABLE_MPI */
	/* Common code for UP, SMP, and MPI */ /* fxm: requires C99 as is? */ /* fxm: for the decl to be at the beginning of scope, another ifdef MPI below the decl should have the prev stmt? */
	int has_mss_val=False;
	ptr_unn mss_val;
	if(dbg_lvl > 0) (void)fprintf(fp_stderr,"%s, ",var_prc_1[idx]->nm);
	if(dbg_lvl > 0) (void)fflush(fp_stderr);
	
	(void)nco_var_mtd_refresh(in_id_1,var_prc_1[idx]); /* Routine contains OpenMP critical regions */
	has_mss_val=var_prc_1[idx]->has_mss_val; 
	(void)nco_var_get(in_id_1,var_prc_1[idx]); /* Routine contains OpenMP critical regions */
    
	/* Find and set variable dmn_nbr, ID, mss_val, type in second file */
	(void)nco_var_mtd_refresh(in_id_2,var_prc_2[idx]);
    
	/* Read hyperslab from second file */
	(void)nco_var_get(in_id_2,var_prc_2[idx]);

	/* Determine whether var1 and var2 conform */
	if(var_prc_1[idx]->nbr_dim == var_prc_2[idx]->nbr_dim){
	  int dmn_idx;
	  /* Do all dimensions match in sequence? */
	  for(dmn_idx=0;dmn_idx<var_prc_1[idx]->nbr_dim;dmn_idx++){
	    if(
	       strcmp(var_prc_1[idx]->dim[dmn_idx]->nm,var_prc_2[idx]->dim[dmn_idx]->nm) || /* Dimension names do not match */
	       (var_prc_1[idx]->dim[dmn_idx]->cnt != var_prc_2[idx]->dim[dmn_idx]->cnt) || /* Dimension sizes do not match */
	       False){
	      (void)fprintf(fp_stdout,"%s: ERROR Variables do not conform:\nFile %s variable %s dimension %d is %s with size %li and count %li\nFile %s variable %s dimension %d is %s with size %li and count %li\n",prg_nm,fl_in_1,var_prc_1[idx]->nm,dmn_idx,var_prc_1[idx]->dim[dmn_idx]->nm,var_prc_1[idx]->dim[dmn_idx]->sz,var_prc_1[idx]->dim[dmn_idx]->cnt,fl_in_2,var_prc_2[idx]->nm,dmn_idx,var_prc_2[idx]->dim[dmn_idx]->nm,var_prc_2[idx]->dim[dmn_idx]->sz,var_prc_2[idx]->dim[dmn_idx]->cnt);
	      if(var_prc_1[idx]->dim[dmn_idx]->cnt == 1 || var_prc_2[idx]->dim[dmn_idx]->cnt == 1) (void)fprintf(fp_stdout,"%s: HINT If a dimension is present in both files, it must be the same size. %s will not attempt to broadcast a degenerate (i.e., size 1) dimension (e.g., a single timestep) to a non-degenerate size. If one of the dimensions is degenerate, try removing it completely (e.g., by averaging over it with ncwa) before invoking %s\n",prg_nm,prg_nm,prg_nm);
	      nco_exit(EXIT_FAILURE);
	    } /* endif */
	  } /* end loop over dmn_idx */
	}else{ /* var_prc_out[idx]->nbr_dim != var_prc_1[idx]->nbr_dim) */
	  /* Number of dimensions do not match, attempt to broadcast variables 
	     fxm: broadcasting here leads to memory leak later since var_[1,2] does not know */
	  
	  /* Die gracefully on unsupported features... */
	  if(var_prc_1[idx]->nbr_dim < var_prc_2[idx]->nbr_dim){
	    (void)fprintf(fp_stdout,"%s: ERROR Variable %s has lesser rank in first file than in second file (%d < %d). This feature is NCO TODO 552.\n",prg_nm,var_prc_1[idx]->nm,var_prc_1[idx]->nbr_dim,var_prc_2[idx]->nbr_dim);
	    nco_exit(EXIT_FAILURE);
	  } /* endif */
	  
	  (void)ncap_var_cnf_dmn(&var_prc_1[idx],&var_prc_2[idx]);
	} /* end else */
	
	/* var2 now conforms in size to var1, and is in memory */
	
	/* fxm: TODO 268 allow var1 or var2 to typecast */
	/* Make sure var2 conforms to type of var1 */
	if(var_prc_1[idx]->type != var_prc_2[idx]->type){
	  (void)fprintf(fp_stderr,"%s: WARNING Input variables do not conform in type:\nFile 1 = %s variable %s has type %s\nFile 2 = %s variable %s has type %s\nFile 3 = %s variable %s will have type %s\n",prg_nm,fl_in_1,var_prc_1[idx]->nm,nco_typ_sng(var_prc_1[idx]->type),fl_in_2,var_prc_2[idx]->nm,nco_typ_sng(var_prc_2[idx]->type),fl_out,var_prc_1[idx]->nm,nco_typ_sng(var_prc_1[idx]->type));
	}  /* endif different type */
	var_prc_2[idx]=nco_var_cnf_typ(var_prc_1[idx]->type,var_prc_2[idx]);
	
	/* Change missing_value of var_prc_2, if any, to missing_value of var_prc_1, if any */
	has_mss_val=nco_mss_val_cnf(var_prc_1[idx],var_prc_2[idx]);
	
	/* mss_val in fl_1, if any, overrides mss_val in fl_2 */
	if(has_mss_val) mss_val=var_prc_1[idx]->mss_val;
    
	/* Perform specified binary operation */
	switch(nco_op_typ){
	case nco_op_add: /* [enm] Add file_1 to file_2 */
	  (void)nco_var_add(var_prc_1[idx]->type,var_prc_1[idx]->sz,has_mss_val,mss_val,var_prc_2[idx]->val,var_prc_1[idx]->val); break;
	case nco_op_mlt: /* [enm] Multiply file_1 by file_2 */
	  (void)nco_var_mlt(var_prc_1[idx]->type,var_prc_1[idx]->sz,has_mss_val,mss_val,var_prc_2[idx]->val,var_prc_1[idx]->val); break;
	case nco_op_dvd: /* [enm] Divide file_1 by file_2 */
	  (void)nco_var_dvd(var_prc_1[idx]->type,var_prc_1[idx]->sz,has_mss_val,mss_val,var_prc_2[idx]->val,var_prc_1[idx]->val); break;
	case nco_op_sbt: /* [enm] Subtract file_2 from file_1 */
	  (void)nco_var_sbt(var_prc_1[idx]->type,var_prc_1[idx]->sz,has_mss_val,mss_val,var_prc_2[idx]->val,var_prc_1[idx]->val); break;
	default: /* Other defined nco_op_typ values are valid for ncra(), ncrcat(), ncwa(), not ncbo() */
	  (void)fprintf(fp_stdout,"%s: ERROR Illegal nco_op_typ in binary operation\n",prg_nm);
	  nco_exit(EXIT_FAILURE);
	  break;
	} /* end case */
    
	var_prc_2[idx]->val.vp=nco_free(var_prc_2[idx]->val.vp);
	
#ifdef ENABLE_MPI
	/* Obtain token and prepare to write */
	while(1){ /* Send TOKEN_REQUEST repeatedly until token obtained */
	  wrk_id_bfr[0]=proc_id;
	  MPI_Send(wrk_id_bfr,wrk_id_bfr_lng,MPI_INT,mgr_id,TOKEN_REQUEST,MPI_COMM_WORLD);
	  /* Receive TOKEN_RESULT (1,0)=(ALLOW,WAIT) */
	  MPI_Recv(info_bfr,info_bfr_lng,MPI_INT,mgr_id,TOKEN_RESULT,MPI_COMM_WORLD,&mpi_stt);
	  tkn_rsp=info_bfr[0];
	  /* Wait then re-send request */
	  if(tkn_rsp == TOKEN_WAIT) sleep(sleep_tm); else break;
	} /* end while loop waiting for write token */
	
	/* Worker has token---prepare to write */
	if(tkn_rsp == TOKEN_ALLOC){
	  rcd=nco_open(fl_out_tmp,NC_WRITE,&out_id);
#else /* !ENABLE_MPI */
#ifdef _OPENMP
#pragma omp critical
#endif /* !_OPENMP */ 
#endif /* !ENABLE_MPI */
	  /* Common code for UP, SMP, and MPI */
	  { /* begin OpenMP critical */ 
	    /* Copy result to output file and free workspace buffer */
	    if(var_prc_1[idx]->nbr_dim == 0){
	      (void)nco_put_var1(out_id,var_prc_out[idx]->id,var_prc_out[idx]->srt,var_prc_1[idx]->val.vp,var_prc_out[idx]->type);
	    }else{ /* end if variable is scalar */
	      (void)nco_put_vara(out_id,var_prc_out[idx]->id,var_prc_out[idx]->srt,var_prc_out[idx]->cnt,var_prc_1[idx]->val.vp,var_prc_out[idx]->type);
	    } /* end else */
	  } /* end OpenMP critical */
	  var_prc_1[idx]->val.vp=nco_free(var_prc_1[idx]->val.vp);

#ifdef ENABLE_MPI
	  /* Close output file and increment written counter */
	  nco_close(out_id);
	  var_wrt_nbr++;
	} /* endif TOKEN_ALLOC */
      } /* end else !NO_MORE_WORK */
      } /* end while loop requesting work/token */
    } /* endif Worker */
#else /* !ENABLE_MPI */
  }  /* end (OpenMP parallel for) loop over idx */
#endif /* !ENABLE_MPI */
  if(dbg_lvl > 0) (void)fprintf(stderr,"\n");

  /* Close input netCDF files */
  nco_close(in_id_1);
  nco_close(in_id_2);
  
#ifdef ENABLE_MPI 
  /* Manager moves output file (closed by workers) from temporary to permanent location */
  if(proc_id == 0) (void)nco_fl_mv(fl_out_tmp,fl_out);
#else /* !ENABLE_MPI */
  /* Close output file and move it from temporary to permanent location */
  (void)nco_fl_out_cls(fl_out,fl_out_tmp,out_id); 
#endif /* end !ENABLE_MPI */

  /* Remove local copy of file */
  if(FILE_1_RETRIEVED_FROM_REMOTE_LOCATION && REMOVE_REMOTE_FILES_AFTER_PROCESSING) (void)nco_fl_rm(fl_in_1);
  if(FILE_2_RETRIEVED_FROM_REMOTE_LOCATION && REMOVE_REMOTE_FILES_AFTER_PROCESSING) (void)nco_fl_rm(fl_in_2);

  /* ncbo-unique memory */
  if(fl_in_1 != NULL) fl_in_1=(char *)nco_free(fl_in_1);
  if(fl_in_2 != NULL) fl_in_2=(char *)nco_free(fl_in_2);

  /* NCO-generic clean-up */
  /* Free individual strings */
  if(cmd_ln != NULL) cmd_ln=(char *)nco_free(cmd_ln);
  if(fl_out != NULL) fl_out=(char *)nco_free(fl_out);
  if(fl_out_tmp != NULL) fl_out_tmp=(char *)nco_free(fl_out_tmp);
  if(fl_pth != NULL) fl_pth=(char *)nco_free(fl_pth);
  if(fl_pth_lcl != NULL) fl_pth_lcl=(char *)nco_free(fl_pth_lcl);
  /* Free lists of strings */
  if(fl_lst_in != NULL && fl_lst_abb == NULL) fl_lst_in=nco_sng_lst_free(fl_lst_in,fl_nbr); 
  if(fl_lst_in != NULL && fl_lst_abb != NULL) fl_lst_in=nco_sng_lst_free(fl_lst_in,1);
  if(fl_lst_abb != NULL) fl_lst_abb=nco_sng_lst_free(fl_lst_abb,abb_arg_nbr);
  if(var_lst_in_nbr > 0) var_lst_in=nco_sng_lst_free(var_lst_in,var_lst_in_nbr);
  /* Free limits */
  for(idx=0;idx<lmt_nbr;idx++) lmt_arg[idx]=(char *)nco_free(lmt_arg[idx]);
  if(lmt_nbr > 0) lmt=nco_lmt_lst_free(lmt,lmt_nbr);
  /* Free dimension lists */
  if(nbr_dmn_xtr_1 > 0) dim_1=nco_dmn_lst_free(dim_1,nbr_dmn_xtr_1);
  if(nbr_dmn_xtr_2 > 0) dim_2=nco_dmn_lst_free(dim_2,nbr_dmn_xtr_2);
  if(nbr_dmn_xtr_1 > 0) dmn_out=nco_dmn_lst_free(dmn_out,nbr_dmn_xtr_1);
  /* Free variable lists 
     Using nco_var_lst_free() to free main var_1 and var_2 lists would fail
     if ncap_var_prc_dmn() had to broadcast any variables because pointer
     var_1 and var_2 still contain dangling pointer to old variable.
     Hence, use nco_var_lst_free() to free prc and fix lists and 
     use nco_free() to free main var_1 and var_2 lists.
     Dangling pointers in var_1 and var_2 are unsafe: fxm TODO 578 */
  if(nbr_var_prc_1 > 0) var_prc_1=nco_var_lst_free(var_prc_1,nbr_var_prc_1);
  if(nbr_var_fix_1 > 0) var_fix_1=nco_var_lst_free(var_fix_1,nbr_var_fix_1);
  if(nbr_var_prc_2 > 0) var_prc_2=nco_var_lst_free(var_prc_2,nbr_var_prc_2);
  if(nbr_var_fix_2 > 0) var_fix_2=nco_var_lst_free(var_fix_2,nbr_var_fix_2);
  var_1=(var_sct **)nco_free(var_1);
  var_2=(var_sct **)nco_free(var_2);
  if(nbr_xtr_1 > 0) var_out=nco_var_lst_free(var_out,nbr_xtr_1);
  var_prc_out=(var_sct **)nco_free(var_prc_out);
  var_fix_out=(var_sct **)nco_free(var_fix_out);

#ifdef ENABLE_MPI 
  MPI_Finalize();
#endif /* !ENABLE_MPI */

  if(rcd != NC_NOERR) nco_err_exit(rcd,"main");
  nco_exit_gracefully();
  return EXIT_SUCCESS;
} /* end main() */
