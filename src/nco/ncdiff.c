/* $Header: /data/zender/nco_20150216/nco/src/nco/ncdiff.c,v 1.17 1999-12-06 18:10:01 zender Exp $ */

/* ncdiff -- netCDF differencer */

/* (c) Copyright 1995--1999 University Corporation for Atmospheric Research 
   The file LICENSE contains the full copyright notice 
   Contact NSF/UCAR/NCAR/CGD/CMS for copyright assistance */

/* Purpose: Compute differences of specified hyperslabs of specfied variables
   from two input netCDF files and output them to a single file. */

/* Usage:
   ncdiff -O in.nc in.nc foo.nc
   ncdiff -O -v mss_val in.nc in.nc foo.nc
   ncdiff -p /data/zender/tmp h0001.nc foo.nc
   ncdiff -p /data/zender/tmp -l /data/zender/tmp/rmt h0001.nc h0002.nc foo.nc
   ncdiff -p /ZENDER/tmp -l /data/zender/tmp/rmt h0001.nc h0002.nc foo.nc
   ncdiff -p /ZENDER/tmp -l /usr/tmp/zender h0001.nc h0002.nc foo.nc

   Test type conversion:
   ncks -O -C -v float_var in.nc foo1.nc
   ncrename -v float_var,double_var foo1.nc
   ncks -O -C -v double_var in.nc foo2.nc
   ncdiff -O -C -v double_var foo1.nc foo2.nc foo3.nc
   ncdiff -O -C -v double_var foo2.nc foo1.nc foo4.nc
   ncks -H -m foo1.nc
   ncks -H -m foo2.nc
   ncks -H -m foo3.nc
   ncks -H -m foo4.nc

   Test var_conform_dim:
   ncks -O -v scalar_var in.nc foo.nc ; ncrename -v scalar_var,four_dim_rec_var foo.nc ; ncdiff -O -v four_dim_rec_var in.nc foo.nc foo2.nc
 */ 

/* Standard header files */
#include <math.h>               /* sin cos cos sin 3.14159 */
#include <netcdf.h>             /* netCDF def'ns */
#include <stdio.h>              /* stderr, FILE, NULL, etc. */
#include <stdlib.h>             /* atof, atoi, malloc, getopt */ 
#include <string.h>             /* strcmp. . . */
#include <sys/stat.h>           /* stat() */
#include <time.h>               /* machine time */
#include <unistd.h>             /* all sorts of POSIX stuff */ 
/* #include <assert.h> */            /* assert() debugging macro */ 
/* #include <errno.h> */             /* errno */
/* #include <malloc.h>    */         /* malloc() stuff */

/* #define MAIN_PROGRAM_FILE MUST precede #include nc.h */
#define MAIN_PROGRAM_FILE
#include "nc.h"                 /* global definitions */

int 
main(int argc,char **argv)
{
  bool EXCLUDE_INPUT_LIST=False; /* Option c */ 
  bool FILE_1_RETRIEVED_FROM_REMOTE_LOCATION;
  bool FILE_2_RETRIEVED_FROM_REMOTE_LOCATION;
  bool FORCE_APPEND=False; /* Option A */ 
  bool FORCE_OVERWRITE=False; /* Option O */ 
  bool FORTRAN_STYLE=False; /* Option F */
  bool HISTORY_APPEND=True; /* Option h */
  bool MUST_CONFORM=True; /* Must var_conform_dim() find truly conforming variables? */
  bool DO_CONFORM; /* Did var_conform_dim() find truly conforming variables? */ 
  bool NCAR_CSM_FORMAT;
  bool PROCESS_ALL_COORDINATES=False; /* Option c */
  bool PROCESS_ASSOCIATED_COORDINATES=True; /* Option C */
  bool REMOVE_REMOTE_FILES_AFTER_PROCESSING=True; /* Option R */ 
  
  char **var_lst_in=NULL_CEWI;
  char **fl_lst_abb=NULL; /* Option a */ 
  char **fl_lst_in;
  char *fl_in=NULL; /* fl_in is realloc'd when not NULL */ 
  char *fl_in_1;
  char *fl_in_2;
  char *fl_pth_lcl=NULL; /* Option l */ 
  char *lmt_arg[MAX_NC_DIMS];
  char *opt_sng;
  char *fl_out;
  char *fl_out_tmp;
  char *fl_pth=NULL; /* Option p */ 
  char *time_bfr_srt;
  char *cmd_ln;
  char CVS_Id[]="$Id: ncdiff.c,v 1.17 1999-12-06 18:10:01 zender Exp $"; 
  char CVS_Revision[]="$Revision: 1.17 $";
  
  dim_sct **dim;
  dim_sct **dim_out;
  
  extern char *optarg;
  extern int ncopts;
  extern int optind;
  
  int has_mss_val=False;
  int idx;
  int idx_dim;
  int idx_fl;
  int in_id;  
  int in_id_1;  
  int in_id_2;  
  int out_id;  
  int nbr_abb_arg=0;
  int nbr_dim_fl;
  int nbr_lmt=0; /* Option d. NB: nbr_lmt gets incremented */
  int nbr_var_fl;
  int nbr_var_fix; /* nbr_var_fix gets incremented */ 
  int nbr_var_prc; /* nbr_var_prc gets incremented */ 
  int nbr_xtr=0; /* nbr_xtr won't otherwise be set for -c with no -v */ 
  int nbr_dim_xtr;
  int nbr_fl=0;
  int opt;
  
  lmt_sct *lmt;
  
  long *lp;

  nm_id_sct *dim_lst;
  nm_id_sct *xtr_lst=NULL; /* xtr_lst can get realloc()'d from NULL with -c option */ 
  
  ptr_unn mss_val;

  time_t clock;
  
  var_sct **var;
  var_sct **var_fix;
  var_sct **var_fix_out;
  var_sct **var_out;
  var_sct **var_prc;
  var_sct **var_prc_out;
  
  /* Start the clock and save the command line */  
  cmd_ln=cmd_ln_sng(argc,argv);
  clock=time((time_t *)NULL);
  time_bfr_srt=ctime(&clock);
  
  /* Get the program name and set the enum for the program (e.g., prg=ncra) */
  prg_nm=prg_prs(argv[0],&prg);

  /* Parse command line arguments */
  opt_sng="ACcD:d:Fhl:Op:rRv:x";
  while((opt = getopt(argc,argv,opt_sng)) != EOF){
    switch(opt){
    case 'A': /* Toggle FORCE_APPEND */
      FORCE_APPEND=!FORCE_APPEND;
      break;
    case 'C': /* Add to extraction list any coordinates associated with variables to be extracted? */ 
      PROCESS_ASSOCIATED_COORDINATES=False;
      break;
    case 'c':
      PROCESS_ALL_COORDINATES=True;
      break;
    case 'D': /* The debugging level. Default is 0. */
      dbg_lvl=atoi(optarg);
      break;
    case 'd': /* Copy argument for later processing */ 
      lmt_arg[nbr_lmt]=(char *)strdup(optarg);
      nbr_lmt++;
      break;
    case 'F': /* Toggle the style of printing out arrays. Default is C-style. */
      FORTRAN_STYLE=!FORTRAN_STYLE;
      break;
    case 'h': /* Toggle appending to history global attribute */
      HISTORY_APPEND=!HISTORY_APPEND;
      break;
    case 'l': /* Get local path prefix for storing files retrieved from remote file system */
      fl_pth_lcl=optarg;
      break;
    case 'O': /* Toggle FORCE_OVERWRITE */
      FORCE_OVERWRITE=!FORCE_OVERWRITE;
      break;
    case 'p': /* Get the path prefix */
      fl_pth=optarg;
      break;
    case 'R': /* Toggle the removal of remotely-retrieved-files after processing. Default is True */
      REMOVE_REMOTE_FILES_AFTER_PROCESSING=!REMOVE_REMOTE_FILES_AFTER_PROCESSING;
      break;
    case 'r': /* Print the CVS program info and copyright notice */
      (void)copyright_prn(CVS_Id,CVS_Revision);
      (void)nc_lib_vrs_prn();
      exit(EXIT_SUCCESS);
      break;
    case 'v': /* Assemble the list of variables to extract/exclude */ 
      var_lst_in=lst_prs(optarg,",",&nbr_xtr);
      break;
    case 'x': /* Exclude rather than extract the variables specified with -v */
      EXCLUDE_INPUT_LIST=True;
      break;
    default: /* Print proper usage */
      (void)usg_prn();
      exit(EXIT_FAILURE);
      break;
    } /* end switch */
  } /* end while loop */
  
  /* Process positional arguments and fill in filenames */
  fl_lst_in=fl_lst_mk(argv,argc,optind,&nbr_fl,&fl_out);
  
  /* Make uniform list of user-specified dimension limits */ 
  lmt=lmt_prs(nbr_lmt,lmt_arg);
  
  /* Make netCDF errors fatal and print the diagnostic */   
  ncopts=NC_VERBOSE | NC_FATAL; 
  
  /* Parse filename */ 
  idx_fl=0;
  fl_in=fl_nm_prs(fl_in,idx_fl,&nbr_fl,fl_lst_in,nbr_abb_arg,fl_lst_abb,fl_pth);
  if(dbg_lvl > 0) (void)fprintf(stderr,"\nInput file %d is %s; ",idx_fl,fl_in);
  /* Make sure the file is on the local system and is readable or die trying */ 
  fl_in=fl_mk_lcl(fl_in,fl_pth_lcl,&FILE_1_RETRIEVED_FROM_REMOTE_LOCATION);
  if(dbg_lvl > 0) (void)fprintf(stderr,"local file %s:\n",fl_in);
  in_id=ncopen(fl_in,NC_NOWRITE);
  
  /* Get the number of variables and dimensions in the file */
  (void)ncinquire(in_id,&nbr_dim_fl,&nbr_var_fl,(int *)NULL,(int *)NULL);
  
  /* Form the initial extraction list from the user input */ 
  xtr_lst=var_lst_mk(in_id,nbr_var_fl,var_lst_in,PROCESS_ALL_COORDINATES,&nbr_xtr);

  /* Change the included variables to excluded variables */ 
  if(EXCLUDE_INPUT_LIST) xtr_lst=var_lst_xcl(in_id,nbr_var_fl,xtr_lst,&nbr_xtr);

  /* Add all the coordinate variables to the extraction list */ 
  if(PROCESS_ALL_COORDINATES) xtr_lst=var_lst_add_crd(in_id,nbr_var_fl,nbr_dim_fl,xtr_lst,&nbr_xtr);

  /* Make sure all coordinates associated with each of the variables to be extracted is also on the list */ 
  if(PROCESS_ASSOCIATED_COORDINATES) xtr_lst=var_lst_ass_crd_add(in_id,xtr_lst,&nbr_xtr);

  /* Finally, heapsort the extraction list by variable ID for fastest I/O */ 
  if(nbr_xtr > 1) xtr_lst=lst_heapsort(xtr_lst,nbr_xtr,False);
    
  /* We now have the final list of variables to extract. Phew. */
  
  /* Find the coordinate/dimension values associated with the limits */ 
  for(idx=0;idx<nbr_lmt;idx++) (void)lmt_evl(in_id,lmt+idx,0L,FORTRAN_STYLE);
  
  /* Find all the dimensions associated with all variables to be extracted */ 
  dim_lst=dim_lst_ass_var(in_id,xtr_lst,nbr_xtr,&nbr_dim_xtr);

  /* Fill in the dimension structure for all the extracted dimensions */ 
  dim=(dim_sct **)malloc(nbr_dim_xtr*sizeof(dim_sct *));
  for(idx=0;idx<nbr_dim_xtr;idx++) dim[idx]=dim_fll(in_id,dim_lst[idx].id,dim_lst[idx].nm);
  
  /* Merge the hyperslab limit information into the dimension structures */ 
  if(nbr_lmt > 0) (void)dim_lmt_merge(dim,nbr_dim_xtr,lmt,nbr_lmt);

  /* Duplicate the input dimension structures for output dimension structures */ 
  dim_out=(dim_sct **)malloc(nbr_dim_xtr*sizeof(dim_sct *));
  for(idx=0;idx<nbr_dim_xtr;idx++){
    dim_out[idx]=dim_dup(dim[idx]);
    (void)dim_xrf(dim[idx],dim_out[idx]); 
  } /* end loop over idx */

  if(dbg_lvl > 0){
    for(idx=0;idx<nbr_xtr;idx++) (void)fprintf(stderr,"xtr_lst[%d].nm = %s, .id= %d\n",idx,xtr_lst[idx].nm,xtr_lst[idx].id);
  } /* end if */
  
  /* Is this an NCAR CSM-format history tape? */
  NCAR_CSM_FORMAT=ncar_csm_inq(in_id);

  /* Fill in variable structure list for all extracted variables */ 
  var=(var_sct **)malloc(nbr_xtr*sizeof(var_sct *));
  var_out=(var_sct **)malloc(nbr_xtr*sizeof(var_sct *));
  for(idx=0;idx<nbr_xtr;idx++){
    var[idx]=var_fll(in_id,xtr_lst[idx].id,xtr_lst[idx].nm,dim,nbr_dim_xtr);
    var_out[idx]=var_dup(var[idx]);
    (void)var_xrf(var[idx],var_out[idx]);
    (void)var_dim_xrf(var_out[idx]);
  } /* end loop over idx */

  /* Divide the variable lists into lists of fixed variables and variables to be processed */ 
  (void)var_lst_divide(var,var_out,nbr_xtr,NCAR_CSM_FORMAT,(dim_sct **)NULL,0,&var_fix,&var_fix_out,&nbr_var_fix,&var_prc,&var_prc_out,&nbr_var_prc);

  /* Open the output file */ 
  fl_out_tmp=fl_out_open(fl_out,FORCE_APPEND,FORCE_OVERWRITE,&out_id);

  /* Copy all the global attributes */ 
  (void)att_cpy(in_id,out_id,NC_GLOBAL,NC_GLOBAL);
  
  /* Catenate the time-stamped command line to the "history" global attribute */ 
  if(HISTORY_APPEND) (void)hst_att_cat(out_id,cmd_ln);

  /* Define the dimensions in the output file */ 
  (void)dim_def(fl_out,out_id,dim_out,nbr_dim_xtr);

  /* Define the variables in the output file, and copy their attributes */ 
  (void)var_def(in_id,fl_out,out_id,var_out,nbr_xtr,(dim_sct **)NULL,0);

  /* Turn off the default filling behavior to enhance efficiency */ 
#if ( ! defined SUN4 ) && ( ! defined SUN4SOL2 ) && ( ! defined SUNMP )
  (void)ncsetfill(out_id,NC_NOFILL);
#endif
  
  /* Take the output file out of define mode */ 
  (void)ncendef(out_id);
  
  /* Zero the start vectors for all the output variables */ 
  (void)var_srt_zero(var_out,nbr_xtr);

  /* Copy the variable data for the non-processed variables */ 
  (void)var_val_cpy(in_id,out_id,var_fix,nbr_var_fix);

  /* The code for ncdiff() has been similar to ncea() (and ncra()) wherever possible.
     The only differences have occurred where performance would suffer otherwise.
     From now on, however, the binary-file and binary-operation nature of ncdiff() 
     is too different from the ncea() paradigm to justify following the ncea() style.
     Instead, a symmetric nomenclature of file_1, file_2 is adopted, and the 
     differences are performed variable by variable so that peak memory usage goes as
     Order(2*maximum variable size) rather than Order(3*maximum record size) or
     Order(3*file size) */ 

  in_id_1=in_id;
  fl_in_1=(char *)strdup(fl_in);
  idx_fl=1;

  /* Parse filename */ 
  fl_in=fl_nm_prs(fl_in,idx_fl,&nbr_fl,fl_lst_in,nbr_abb_arg,fl_lst_abb,fl_pth);
  if(dbg_lvl > 0) (void)fprintf(stderr,"\nInput file %d is %s; ",idx_fl,fl_in);
  /* Make sure the file is on the local system and is readable or die trying */ 
  fl_in=fl_mk_lcl(fl_in,fl_pth_lcl,&FILE_2_RETRIEVED_FROM_REMOTE_LOCATION);
  if(dbg_lvl > 0) (void)fprintf(stderr,"local file %s:\n",fl_in);
  in_id_2=ncopen(fl_in,NC_NOWRITE);
  fl_in_2=fl_in;
  
  /* Perform error checking if there are any variables to be processed in this file */ 
  if(False) (void)fl_cmp_err_chk();

  /* Loop over each differenced variable */ 
  for(idx=0;idx<nbr_var_prc;idx++){
    if(dbg_lvl > 0) (void)fprintf(stderr,"%s, ",var_prc[idx]->nm);
    if(dbg_lvl > 0) (void)fflush(stderr);

    /* Initialize mss_val flag */ 
    has_mss_val=False;

    (void)var_refresh(in_id_1,var_prc[idx]);
    has_mss_val=has_mss_val || var_prc[idx]->has_mss_val; 
    (void)var_get(in_id_1,var_prc[idx]);
    
    /* Save output variable ID from being overwritten in refresh call */ 
    var_prc[idx]->id=var_prc_out[idx]->id;
    (void)var_refresh(in_id_2,var_prc_out[idx]);

    /* Assume var1 and var2 conform if they are same rank */ 
    if(var_prc_out[idx]->nbr_dim == var_prc[idx]->nbr_dim){
      nc_type var_type;
      int rcd;

      /* Routine var_refresh() does not set type for var_prc_out, do so manually */ 
#ifdef NETCDF2_ONLY
      int var_id;
      var_id=ncvarid(in_id_2,var_prc_out[idx]->nm);
      (void)ncvarinq(in_id_2,var_id,(char *)NULL,&var_type,(int *)NULL,(int *)NULL,(int *)NULL);
#else /* not NETCDF2_ONLY */
      rcd=nc_inq_vartype(in_id_2,ncvarid(in_id_2,var_prc_out[idx]->nm),&var_type);
#endif /* not NETCDF2_ONLY */
      var_prc_out[idx]->type=var_type;

      /* Test whether all dimensions match in sequence */ 
      for(idx_dim=0;idx_dim<var_prc[idx]->nbr_dim;idx_dim++){
	if(
	   !strstr(var_prc_out[idx]->dim[idx_dim]->nm,var_prc[idx]->dim[idx_dim]->nm) || /* Dimension names do not match */
	   (var_prc_out[idx]->dim[idx_dim]->cnt != var_prc[idx]->dim[idx_dim]->cnt) || /* Dimension sizes do not match */ 
	   False){
	  (void)fprintf(stdout,"%s: ERROR Variables do not conform:\nFile %s variable %s dimension %d is %s with size %li and count %li\nFile %s variable %s dimension %d is %s with size %li and count %li\n",prg_nm,fl_in_1,var_prc[idx]->nm,idx_dim,var_prc[idx]->dim[idx_dim]->nm,var_prc[idx]->dim[idx_dim]->sz,var_prc[idx]->dim[idx_dim]->cnt,fl_in_2,var_prc_out[idx]->nm,idx_dim,var_prc_out[idx]->dim[idx_dim]->nm,var_prc_out[idx]->dim[idx_dim]->sz,var_prc_out[idx]->dim[idx_dim]->cnt);
	  exit(EXIT_FAILURE);
	} /* endif */ 
      } /* end loop over idx_dim */ 

      /* Temporarily set srt vector to account for hyperslabs while reading */ 
      lp=var_prc_out[idx]->srt;
      var_prc_out[idx]->srt=var_prc[idx]->srt;
      (void)var_get(in_id_2,var_prc_out[idx]);
      /* Reset srt vector to zeros for eventual output */ 
      var_prc_out[idx]->srt=lp;
    }else{
      int idx_var_dim;

      var_sct *var_tmp=NULL;
      
      /* var1 and var2 have differing numbers of dimensions so make var2 conform to var1 */ 
      var_prc_out[idx]=var_free(var_prc_out[idx]);
      var_prc_out[idx]=var_fll(in_id_2,ncvarid(in_id_2,var_prc[idx]->nm),var_prc[idx]->nm,dim,nbr_dim_xtr);
      (void)var_get(in_id_2,var_prc_out[idx]);
      
      /* Now that we have the second variable, reset its dimension IDs to match those of the 
	 corresponding dimensions in the first variable, because the var_conform_dim() routine
	 makes the (usually true) assumption that if the dimension IDs are equal, then the
	 dimensions conform, i.e., var_conform_dim() assumes that both the "conformee" and the
	 "conformand" are from the same file. */ 
      
      /* This 8 line block should not be necessary when dimension names are used rather than dim IDs */
      if(False){
	for(idx_var_dim=0;idx_var_dim<var_prc_out[idx]->nbr_dim;idx_var_dim++){
	  for(idx_dim=0;idx_dim<var_prc[idx]->nbr_dim;idx_dim++){
	    if(var_prc_out[idx]->dim[idx_var_dim]->id == var_prc[idx]->dim[idx_dim]->id) break;
	  } /* end loop over idx_var_dim */ 
	  var_prc_out[idx]->dim_id[idx_var_dim]=var_prc[idx]->dim[idx_dim]->id;
	} /* end loop over idx_dim */ 
      } /* endif False */ 
      
      /* Pass a dummy pointer so we do not lose track of original */ 
      var_tmp=var_conform_dim(var_prc[idx],var_prc_out[idx],var_tmp,MUST_CONFORM,&DO_CONFORM);
      var_prc_out[idx]=var_free(var_prc_out[idx]);
      var_prc_out[idx]=var_tmp;
    } /* end else */
    
    /* var2 now conforms in size to var1, and is in memory */ 

    /* Make sure var2 conforms to type of var1 */ 
    if(var_prc_out[idx]->type != var_prc[idx]->type){
      (void)fprintf(stderr,"%s: WARNING Input variables do not conform in type:\nFile 1 = %s variable %s has type %s\nFile 2 = %s variable %s has type %s\nFile 3 = %s variable %s will have type %s\n",prg_nm,fl_in_1,var_prc[idx]->nm,nc_type_nm(var_prc[idx]->type),fl_in_2,var_prc_out[idx]->nm,nc_type_nm(var_prc_out[idx]->type),fl_out,var_prc[idx]->nm,nc_type_nm(var_prc[idx]->type));
    }  /* endif different type */ 
    var_prc_out[idx]=var_conform_type(var_prc[idx]->type,var_prc_out[idx]);

    /* The mss_val in fl_1, if any, overrides the mss_val in fl_2 */ 
    if(has_mss_val) mss_val=var_prc[idx]->mss_val; else mss_val=var_prc_out[idx]->mss_val;
    has_mss_val=has_mss_val || var_prc_out[idx]->has_mss_val; 
    
    (void)var_subtract(var_prc[idx]->type,var_prc[idx]->sz,has_mss_val,mss_val,var_prc_out[idx]->val,var_prc[idx]->val);
    
    (void)free(var_prc_out[idx]->val.vp);
    
    /* Copy the differences to the output file and free the differencing buffer */ 
    if(var_prc[idx]->nbr_dim == 0){
      (void)ncvarput1(out_id,var_prc[idx]->id,var_prc_out[idx]->srt,var_prc[idx]->val.vp);
    }else{ /* end if variable is a scalar */ 
      (void)ncvarput(out_id,var_prc[idx]->id,var_prc_out[idx]->srt,var_prc_out[idx]->cnt,var_prc[idx]->val.vp);
    } /* end else */ 
    (void)free(var_prc[idx]->val.vp);
    
  } /* end loop over idx */
  if(dbg_lvl > 0) (void)fprintf(stderr,"\n");
  
  /* Close input netCDF files */ 
  ncclose(in_id_1);
  ncclose(in_id_2);

  /* Close output file and move it from temporary to permanent location */ 
  (void)fl_out_cls(fl_out,fl_out_tmp,out_id);
  
  /* Remove local copy of file */ 
  if(FILE_1_RETRIEVED_FROM_REMOTE_LOCATION && REMOVE_REMOTE_FILES_AFTER_PROCESSING) (void)fl_rm(fl_in_1);
  if(FILE_2_RETRIEVED_FROM_REMOTE_LOCATION && REMOVE_REMOTE_FILES_AFTER_PROCESSING) (void)fl_rm(fl_in_2);
  
  Exit_gracefully();
  return EXIT_SUCCESS;
} /* end main() */
