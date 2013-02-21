/* $Header: /data/zender/nco_20150216/nco/src/nco/nco_grp_trv.c,v 1.61 2013-02-21 06:47:55 pvicente Exp $ */

/* Purpose: netCDF4 traversal storage */

/* Copyright (C) 2011--2013 Charlie Zender
   License: GNU General Public License (GPL) Version 3
   See http://www.gnu.org/copyleft/gpl.html for full license text */

/* Testing:
   ncks -D 1 ~/nco/data/in_grp.nc */

#include "nco_grp_trv.h" /* Group traversal */
#include "nco_lmt.h" /* Hyperslab limits */

void                          
trv_tbl_init
(trv_tbl_sct **tbl) /* I/O [sct] Traversal table fxm pvn move all this to a alloc by need case */
{
  unsigned int idx;
  trv_tbl_sct *tb=(trv_tbl_sct *)nco_malloc(sizeof(trv_tbl_sct));

  /* Object (group/variable) list */

  tb->sz=100;
  tb->nbr=0;
  tb->lst=(trv_sct *)nco_malloc(tb->sz*sizeof(trv_sct));

  for(idx=0;idx<tb->sz;idx++){
    tb->lst[idx].nm_fll=NULL;
    tb->lst[idx].nm_fll_lng=0L;
    tb->lst[idx].grp_nm_fll=NULL;
    tb->lst[idx].typ=nco_obj_typ_err;
    tb->lst[idx].nm[0]='\0';
    tb->lst[idx].nm_lng=0L;

    tb->lst[idx].flg_cf=False; /* [flg] Object matches CF-metadata extraction criteria */
    tb->lst[idx].flg_crd=False; /* [flg] Object matches coordinate extraction criteria */
    tb->lst[idx].flg_dfl=False; /* [flg] Object meets default subsetting criteria */
    tb->lst[idx].flg_gcv=False; /* [flg] Group contains matched variable */
    tb->lst[idx].flg_mch=False; /* [flg] Object matches user-specified strings */
    tb->lst[idx].flg_ncs=False; /* [flg] Group is ancestor of specified group or variable */
    tb->lst[idx].flg_nsx=False; /* [flg] Object matches intersection criteria */
    tb->lst[idx].flg_rcr=False; /* [flg] Extract group recursively */
    tb->lst[idx].flg_unn=False; /* [flg] Object matches union criteria */
    tb->lst[idx].flg_vfp=False; /* [flg] Variable matches full path specification */
    tb->lst[idx].flg_vsg=False; /* [flg] Variable selected because group matches */
    tb->lst[idx].flg_xcl=False; /* [flg] Object matches exclusion criteria */
    tb->lst[idx].flg_xtr=False; /* [flg] Extract object */
    tb->lst[idx].is_crd_var=False; /* [flg] (For variables only) Is this a coordinate variable? (unique dimension exists in scope) */
    tb->lst[idx].is_rec_var=False;

    tb->lst[idx].grp_dpt=nco_obj_typ_err; /* [nbr] Depth of group (root = 0) */
    tb->lst[idx].grp_id_in=nco_obj_typ_err; /* [id] Group ID in input file */
    tb->lst[idx].grp_id_out=nco_obj_typ_err; /* [id] Group ID in output file */

    tb->lst[idx].nbr_att=nco_obj_typ_err;
    tb->lst[idx].nbr_dmn=nco_obj_typ_err;
    tb->lst[idx].nbr_grp=nco_obj_typ_err;
    tb->lst[idx].nbr_rec=nco_obj_typ_err;
    tb->lst[idx].nbr_var=nco_obj_typ_err;

    /* Variable dimensions */
    for(int dmn_idx_var=0;dmn_idx_var<NC_MAX_DIMS;dmn_idx_var++){
      tb->lst[idx].var_dmn.dmn_nm_fll[dmn_idx_var]=NULL;
      tb->lst[idx].var_dmn.dmn_nm[dmn_idx_var]=NULL;
      tb->lst[idx].var_dmn.grp_nm_fll[dmn_idx_var]=NULL;
    }
    tb->lst[idx].var_dmn.nbr_dmn=nco_obj_typ_err;

  } /* end loop over objects */

  /* Dimension list */

  tb->sz_dmn=100;
  tb->nbr_dmn=0;
  tb->lst_dmn=(dmn_fll_sct *)nco_malloc(tb->sz_dmn*sizeof(dmn_fll_sct));

  for(idx=0;idx<tb->sz_dmn;idx++){
    tb->lst_dmn[idx].grp_nm_fll=NULL; /* [sng] Full group name where dimension was defined (there is one and only one group)*/
    tb->lst_dmn[idx].is_rec_dmn=-1; /* [flg] Is a record dimension? */
    tb->lst_dmn[idx].nm[0]='\0';  /* [sng] Name of dimension (if coordinate variable, also name of variable) */
    tb->lst_dmn[idx].nm_fll=NULL; /* [sng] Dimension fully qualified name (path) */
    tb->lst_dmn[idx].sz=0; /* [nbr] Size of dimension */
    tb->lst_dmn[idx].lmt_dmn_nbr=0;  /* [nbr] Number of limit structures */
    tb->lst_dmn[idx].lmt_crr=0; /* [nbr] Index of current limit structure being initialized */

    /* Limits */
    tb->lst_dmn[idx].lmt_dmn=NULL;
    tb->lst_dmn[idx].lmt_crr=0;
    tb->lst_dmn[idx].WRP=False;
    tb->lst_dmn[idx].BASIC_DMN=True;
    tb->lst_dmn[idx].MSA_USR_RDR=False;  
    tb->lst_dmn[idx].dmn_cnt=nco_obj_typ_err;
  }

  *tbl=tb;
} /* trv_tbl_init() */

 
void 
trv_tbl_free
(trv_tbl_sct *tbl) /* I [sct] Traversal table */
{
  unsigned int idx;

  /* Object (group/variable) list */

  for(idx=0;idx<tbl->sz;idx++){
    tbl->lst[idx].nm_fll=(char *)nco_free(tbl->lst[idx].nm_fll);
    tbl->lst[idx].grp_nm_fll=(char *)nco_free(tbl->lst[idx].grp_nm_fll);

    /* Variable dimensions */
    for(int dmn_idx_var=0;dmn_idx_var<tbl->lst[idx].var_dmn.nbr_dmn;dmn_idx_var++){
      tbl->lst[idx].var_dmn.dmn_nm_fll[dmn_idx_var]=(char *)nco_free(tbl->lst[idx].var_dmn.dmn_nm_fll[dmn_idx_var]);
      tbl->lst[idx].var_dmn.dmn_nm[dmn_idx_var]=(char *)nco_free(tbl->lst[idx].var_dmn.dmn_nm[dmn_idx_var]);
      tbl->lst[idx].var_dmn.grp_nm_fll[dmn_idx_var]=(char *)nco_free(tbl->lst[idx].var_dmn.grp_nm_fll[dmn_idx_var]);
    }

  } /* end loop */
  tbl->lst=(trv_sct *)nco_free(tbl->lst);

  /* Dimension list */

  for(unsigned int dmn_idx=0;dmn_idx<tbl->sz_dmn;dmn_idx++){
    tbl->lst_dmn[dmn_idx].nm_fll=(char *)nco_free(tbl->lst_dmn[dmn_idx].nm_fll);
    tbl->lst_dmn[dmn_idx].grp_nm_fll=(char *)nco_free(tbl->lst_dmn[dmn_idx].grp_nm_fll);

    for(int lmt_idx=0;lmt_idx<tbl->lst_dmn[dmn_idx].lmt_dmn_nbr;lmt_idx++){
      if(dbg_lvl_get() >= nco_dbg_dev){
        dmn_fll_sct dmn_trv=tbl->lst_dmn[dmn_idx];
        (void)fprintf(stdout,"INFO limit [%d]%s done:\n",lmt_idx,dmn_trv.lmt_dmn[lmt_idx]->nm);
      }
      tbl->lst_dmn[dmn_idx].lmt_dmn[lmt_idx]=nco_lmt_free(tbl->lst_dmn[dmn_idx].lmt_dmn[lmt_idx]);
    }

  } /* End Dimension list loop */

  tbl->lst_dmn=(dmn_fll_sct *)nco_free(tbl->lst_dmn);

  tbl=(trv_tbl_sct *)nco_free(tbl);
} /* end trv_tbl_free() */

void 
trv_tbl_add
(trv_sct const obj, /* I [sct] Object to store */
 trv_tbl_sct * const tbl) /* I/O [sct] Traversal table */
{
  unsigned int idx;

  if(tbl->nbr == tbl->sz){
    tbl->sz*=2;
    tbl->lst=(trv_sct *)nco_realloc(tbl->lst,tbl->sz*sizeof(trv_sct));

    for(idx=tbl->nbr;idx<tbl->sz;idx++){
      tbl->lst[idx].nm_fll=NULL;
      tbl->lst[idx].grp_nm_fll=NULL;
    } /* idx */
  } /* tbl->sz */

  idx=tbl->nbr++;

  tbl->lst[idx].nm_fll=(char *)strdup(obj.nm_fll);
  tbl->lst[idx].nm_fll_lng=obj.nm_fll_lng;
  tbl->lst[idx].grp_nm_fll=(char *)strdup(obj.grp_nm_fll);
  strcpy(tbl->lst[idx].nm,obj.nm);
  tbl->lst[idx].nm_lng=obj.nm_lng;
  tbl->lst[idx].typ=obj.typ;

  tbl->lst[idx].flg_cf=obj.flg_cf; /* [flg] Object matches CF-metadata extraction criteria */
  tbl->lst[idx].flg_crd=obj.flg_crd; /* [flg] Object matches coordinate extraction criteria */
  tbl->lst[idx].flg_dfl=obj.flg_dfl; /* [flg] Object meets default subsetting criteria */
  tbl->lst[idx].flg_gcv=obj.flg_gcv; /* [flg] Group contains matched variable */
  tbl->lst[idx].flg_mch=obj.flg_mch; /* [flg] Object matches user-specified strings */
  tbl->lst[idx].flg_ncs=obj.flg_ncs; /* [flg] Group is ancestor of specified group or variable */
  tbl->lst[idx].flg_nsx=obj.flg_nsx; /* [flg] Object matches intersection criteria */
  tbl->lst[idx].flg_rcr=obj.flg_rcr; /* [flg] Extract group recursively */
  tbl->lst[idx].flg_unn=obj.flg_unn; /* [flg] Object matches union criteria */
  tbl->lst[idx].flg_vfp=obj.flg_vfp; /* [flg] Variable matches full path specification */
  tbl->lst[idx].flg_vsg=obj.flg_vsg; /* [flg] Variable selected because group matches */
  tbl->lst[idx].flg_xcl=obj.flg_xcl; /* [flg] Object matches exclusion criteria */
  tbl->lst[idx].flg_xtr=obj.flg_xtr; /* [flg] Extract object */
  tbl->lst[idx].is_crd_var=False; /* [flg] (For variables only) Is this a coordinate variable? (unique dimension exists in scope) */

  tbl->lst[idx].grp_dpt=obj.grp_dpt; /* [nbr] Depth of group (root = 0) */
  tbl->lst[idx].grp_id_in=obj.grp_id_in; /* [id] Group ID in input file */
  tbl->lst[idx].grp_id_out=obj.grp_id_out; /* [id] Group ID in output file */

  tbl->lst[idx].nbr_att=obj.nbr_att;
  tbl->lst[idx].nbr_dmn=obj.nbr_dmn;
  tbl->lst[idx].nbr_grp=obj.nbr_grp;
  tbl->lst[idx].nbr_rec=obj.nbr_rec;
  tbl->lst[idx].nbr_var=obj.nbr_var;
} /* end trv_tbl_add() */


void 
trv_tbl_add_dmn                       /* [fnc] Add a dimension object to table  */
(dmn_fll_sct const obj,               /* I [sct] Object to store */
 trv_tbl_sct * const tbl)             /* I/O [sct] Traversal table */
{
  unsigned int idx;

  if(tbl->nbr_dmn == tbl->sz_dmn){
    tbl->sz_dmn*=2;
    tbl->lst_dmn=(dmn_fll_sct *)nco_realloc(tbl->lst_dmn,tbl->sz_dmn*sizeof(dmn_fll_sct));

    for(idx=tbl->nbr_dmn;idx<tbl->sz_dmn;idx++){

      tbl->lst_dmn[idx].grp_nm_fll=NULL; /* [sng] Full group name where dimension was defined (there is one and only one group)*/
      tbl->lst_dmn[idx].is_rec_dmn=nco_obj_typ_err; /* [flg] Is a record dimension? */
      tbl->lst_dmn[idx].nm[0]='\0';  /* [sng] Name of dimension (if coordinate variable, also name of variable) */
      tbl->lst_dmn[idx].nm_fll=NULL; /* [sng] Dimension fully qualified name (path) */
      tbl->lst_dmn[idx].sz=nco_obj_typ_err; /* [nbr] Size of dimension */
      tbl->lst_dmn[idx].lmt_dmn_nbr=0;  /* [nbr] Number of limit structures */
      tbl->lst_dmn[idx].lmt_crr=0; /* [nbr] Index of current limit structure being initialized */

      /* Limits: TO DO move all this to a alloc by need case */
      tbl->lst_dmn[idx].lmt_dmn=NULL;
      tbl->lst_dmn[idx].lmt_crr=0;
      tbl->lst_dmn[idx].WRP=False;
      tbl->lst_dmn[idx].BASIC_DMN=True;
      tbl->lst_dmn[idx].MSA_USR_RDR=False;  
      tbl->lst_dmn[idx].dmn_cnt=nco_obj_typ_err;

    } /* idx */
  } /* tbl->sz_dmn */

  idx=tbl->nbr_dmn++;

  tbl->lst_dmn[idx].nm_fll=(char *)strdup(obj.nm_fll);
  tbl->lst_dmn[idx].grp_nm_fll=(char *)strdup(obj.grp_nm_fll);
  strcpy(tbl->lst_dmn[idx].nm,obj.nm);
  tbl->lst_dmn[idx].is_rec_dmn=obj.is_rec_dmn;
  tbl->lst_dmn[idx].sz=obj.sz;

  /* Limits are initialized in build limits function */

} /* end trv_tbl_add_dmn() */





void                       
trv_tbl_inq                          /* [fnc] Find and return global totals of dimensions, variables, attributes */
(int * const att_glb_all,            /* O [nbr] Number of global attributes in file */
 int * const att_grp_all,            /* O [nbr] Number of group attributes in file */
 int * const att_var_all,            /* O [nbr] Number of variable attributes in file */
 int * const dmn_nbr_all,            /* O [nbr] Number of dimensions in file */
 int * const dmn_rec_all,            /* O [nbr] Number of record dimensions in file */
 int * const grp_dpt_all,            /* O [nbr] Maximum group depth (root = 0) */
 int * const grp_nbr_all,            /* O [nbr] Number of groups in file */
 int * const var_ntm_all,            /* O [nbr] Number of non-atomic variables in file */
 int * const var_tmc_all,            /* O [nbr] Number of atomic-type variables in file */
 const trv_tbl_sct * const trv_tbl)  /* I [sct] Traversal table */
{
  /* [fnc] Find and return global file summaries like # of dimensions, variables, attributes */

  int att_glb_lcl; /* [nbr] Number of global attributes in file */
  int att_grp_lcl; /* [nbr] Number of group attributes in file */
  int att_var_lcl; /* [nbr] Number of variable attributes in file */
  int dmn_rec_lcl; /* [nbr] Number of record dimensions in file */
  int grp_dpt_lcl; /* [nbr] Maximum group depth (root = 0) */
  int grp_nbr_lcl; /* [nbr] Number of groups in file */
  int var_ntm_lcl; /* [nbr] Number of non-atomic variables in file */
  int var_tmc_lcl; /* [nbr] Number of atomic-type variables in file */
 
  /* Initialize */
  att_glb_lcl=0;
  att_grp_lcl=0;
  att_var_lcl=0;
  dmn_rec_lcl=0;
  grp_dpt_lcl=0;
  grp_nbr_lcl=0;
  var_ntm_lcl=0;
  var_tmc_lcl=0;

  for(unsigned uidx=0;uidx<trv_tbl->nbr;uidx++){
    trv_sct trv=trv_tbl->lst[uidx]; 
    if(trv.typ == nco_obj_typ_var) att_var_lcl+=trv.nbr_att;
    if(trv.typ == nco_obj_typ_nonatomic_var) var_ntm_lcl++;
    if(trv.typ == nco_obj_typ_grp){ 
      grp_nbr_lcl+=trv.nbr_grp;
      var_tmc_lcl+=trv.nbr_var;
      if(grp_dpt_lcl < trv.grp_dpt) grp_dpt_lcl=trv.grp_dpt;
      if(!strcmp(trv.nm_fll,"/")) att_glb_lcl=trv.nbr_att; else att_grp_lcl+=trv.nbr_att; 
    } /* end nco_obj_typ_grp */
  } /* end uidx */

  for(unsigned uidx=0;uidx<trv_tbl->nbr_dmn;uidx++)
    if(trv_tbl->lst_dmn[uidx].is_rec_dmn) dmn_rec_lcl++;

  if(att_glb_all) *att_glb_all=att_glb_lcl;
  if(att_grp_all) *att_grp_all=att_grp_lcl;
  if(att_var_all) *att_var_all=att_var_lcl;
  if(dmn_nbr_all) *dmn_nbr_all=trv_tbl->nbr_dmn;
  if(dmn_rec_all) *dmn_rec_all=dmn_rec_lcl;
  if(grp_dpt_all) *grp_dpt_all=grp_dpt_lcl;
  if(grp_nbr_all) *grp_nbr_all=grp_nbr_lcl;
  if(var_ntm_all) *var_ntm_all=var_ntm_lcl;
  if(var_tmc_all) *var_tmc_all=var_tmc_lcl;

  return;
} /* end trv_tbl_inq() */

void 
trv_tbl_prn_flg_mch                  /* [fnc] Print table items that have .flg_mch  */
(const trv_tbl_sct * const trv_tbl,  /* I [sct] Traversal table */
 const nco_obj_typ obj_typ)          /* I [enm] Object type (group or variable) */
{
  /* Print all matching objects of given type from traversal table */
  trv_sct trv_obj;
  for(unsigned int tbl_idx=0;tbl_idx<trv_tbl->nbr;tbl_idx++){
    trv_obj=trv_tbl->lst[tbl_idx];
    if((trv_obj.typ == obj_typ) && trv_obj.flg_mch) (void)fprintf(stdout,"nm_fll=%s\n",trv_obj.nm_fll);
  } /* end loop over trv_tbl */
}/* end trv_tbl_prn_flg_mch() */

void                          
trv_tbl_prn                          /* [fnc] Print table with -z */
(const trv_tbl_sct * const trv_tbl)  /* I [sct] Traversal table */  
{
  for(unsigned uidx=0;uidx<trv_tbl->nbr;uidx++){
    trv_sct trv=trv_tbl->lst[uidx];
    if(trv.typ == nco_obj_typ_grp) (void)fprintf(stdout,"grp: "); else (void)fprintf(stdout,"var: ");
    (void)fprintf(stdout,"%s\n",trv_tbl->lst[uidx].nm_fll); 
  } /* end uidx */
} /* end trv_tbl_prn() */

nco_bool                              /* O [nbr] Item found or not */
trv_tbl_fnd_var_nm_fll                /* [fnc] Check if "var_nm_fll" is in table */
(const char * const var_nm_fll,       /* I [sng] Variable name to find */
 const trv_tbl_sct * const trv_tbl)   /* I [sct] Traversal table */
{
  for(unsigned uidx=0;uidx<trv_tbl->nbr;uidx++)
    if(trv_tbl->lst[uidx].typ == nco_obj_typ_var && !strcmp(var_nm_fll,trv_tbl->lst[uidx].nm_fll))
      return True;

  return False;
} /* end trv_tbl_fnd_var_nm_fll() */

nco_bool                              /* O [flg] Item found */
trv_tbl_fnd_var_nm                    /* [fnc] Find a variable that matches parameter "var_nm" */
(const char * const var_nm,           /* I [sng] Variable (relative) name to find */
 const trv_tbl_sct * const trv_tbl)   /* I [sct] Traversal table */
{
  for(unsigned uidx=0;uidx<trv_tbl->nbr;uidx++){
    trv_sct trv=trv_tbl->lst[uidx];
    if(trv.typ == nco_obj_typ_var && !strcmp(trv.nm,var_nm)) return True;
  } /* end loop over uidx */

  return False;
} /* end trv_tbl_fnd_var_nm() */ 

void
trv_tbl_mrk_xtr                       /* [fnc] Mark extraction flag in table for "var_nm_fll" */
(const char * const var_nm_fll,       /* I [sng] Variable name to find */
 const trv_tbl_sct * const trv_tbl)   /* I [sct] Traversal table */
{
  for(unsigned uidx=0;uidx<trv_tbl->nbr;uidx++)
    if(!strcmp(var_nm_fll,trv_tbl->lst[uidx].nm_fll)) trv_tbl->lst[uidx].flg_xtr=True;

  return;
} /* end trv_tbl_mrk_xtr() */

void 
trv_tbl_prn_xtr                      /* [fnc] Print extraction flag of traversal table */
(const trv_tbl_sct * const trv_tbl,  /* I [sct] Traversal table */
 const char * const fnc_nm)          /* I [sng] Function name of the calling function */
{
  int idx=0;
  int nbr_flg=0;

  for(unsigned uidx=0;uidx<trv_tbl->nbr;uidx++)
    if(trv_tbl->lst[uidx].flg_xtr){
      nbr_flg++;
    } /* endif */

  (void)fprintf(stdout,"%s: INFO trv_tbl_prn_xtr() reports %d objects with extraction flag (flg_xtr) set\n",
    prg_nm_get(),nbr_flg); 
  for(unsigned uidx=0;uidx<trv_tbl->nbr;uidx++)
    if(trv_tbl->lst[uidx].flg_xtr){
      (void)fprintf(stdout,"[%d] %s\n",idx++,trv_tbl->lst[uidx].nm_fll); 
    } /* endif */

} /* end trv_tbl_prn_xtr() */

int /* O [enm] Comparison result [<,=,>] 0 iff val_1 [<,==,>] val_2 */
nco_cmp_trv_tbl_nm_fll  /* [fnc] Compare two trv_sct's by full name member */
(const void *val_1, /* I [sct] trv_sct to compare */
 const void *val_2) /* I [sct] trv_sct to compare */
{
  /* Purpose: Compare two trv_sct's by name structure member
     Function is suitable for argument to ANSI C qsort() routine in stdlib.h
     Code based on responses to my comp.lang.c thread 20040101 */
  return strcmp((*(trv_sct const *)val_1).nm_fll,(*(trv_sct const *)val_2).nm_fll);
} /* end nco_cmp_trv_tbl_nm() */

void 
trv_tbl_srt /* [fnc] Sort traversal table */
(trv_tbl_sct * const trv_tbl) /* I/O [sct] Traversal table */
{
  /* Purpose: Alphabetize list by object full name
     This produces easy-to-search variable name screen output with ncks */
  qsort(trv_tbl->lst,(size_t)trv_tbl->nbr,sizeof(trv_sct),nco_cmp_trv_tbl_nm_fll);
} /* end trv_tbl_srt() */

void                          
trv_tbl_mch                           /* [fnc] Match 2 tables (find common objects) */
(trv_tbl_sct * const trv_tbl_1,       /* I/O [sct] Traversal table 1 */  
 trv_tbl_sct * const trv_tbl_2)       /* I/O [sct] Traversal table 2 */  
{
  /* Purpose: Find common objects; the algorithm used for this search is the
  *  cosequential match algorithm and is described in
  *  Folk, Michael; Zoellick, Bill. (1992). File Structures. Addison-Wesley.
  *
  * Compare 2 ordered lists of names:
  *  if Name(1) is less than Name(2), read the next name from List 1; this is done by incrementing a current index
  *  if Name(1) is greater than Name(2), read the next name from List 2
  *  if the names are the same, read the next names from the two lists 
  * 
  */

  typedef struct{		
    char *var_nm_fll; /* [sng] Full path of variable */
    nco_bool flg_in_fl[2];  /* [flg] Is this name if each file?; files are [0] and [1] */
  } nco_cmn_t;

  nco_cmn_t *cmn_lst=NULL;       /* [sct] A list of common names */ 
  int nbr_tbl_1;                 /* [nbr] Number of items in list 1 */
  int nbr_tbl_2;                 /* [nbr] Number of items in list 2 */
  nco_bool flg_more_names_exist; /* [flg] Are there more names to process? */
  int nco_cmp;                   /* [nbr] Return value of strcmp() */ 
  int idx_tbl_1;                 /* [idx] Current position in List 1 */ 
  int idx_tbl_2;                 /* [idx] Current position in List 2 */ 
  int idx_lst;                   /* [idx] Current position in common List */ 
  nco_bool flg_in_fl[2];         /* [flg] Is this name if each file?; files are [0] and [1] */

  /* Tables *must* be sorted */
  (void)trv_tbl_srt(trv_tbl_1);
  (void)trv_tbl_srt(trv_tbl_2);

  (void)fprintf(stdout,"Sorted table 1\n");
  (void)trv_tbl_prn(trv_tbl_1);
  (void)fprintf(stdout,"Sorted table 2\n");
  (void)trv_tbl_prn(trv_tbl_2);
  (void)fprintf(stdout,"Common objects\n");

  /* Get number of objects in each table */
  nbr_tbl_1=trv_tbl_1->nbr;
  nbr_tbl_2=trv_tbl_2->nbr;

  /* If both lists have names, then there are names to process */
  flg_more_names_exist = (nbr_tbl_1>0 && nbr_tbl_2>0) ? 1 : 0;

  /* Put counters ar start */
  idx_tbl_1=0;
  idx_tbl_2=0;
  idx_lst=0;

  /* Store a list of common objects */
  cmn_lst=(nco_cmn_t *)nco_malloc((nbr_tbl_1+nbr_tbl_2)*sizeof(nco_cmn_t));

  /* Iterate the 2 lists */
  while (flg_more_names_exist)
  {
    trv_sct trv_1=trv_tbl_1->lst[idx_tbl_1];
    trv_sct trv_2=trv_tbl_2->lst[idx_tbl_2];

    /* Criteria is string compare */
    nco_cmp = strcmp(trv_1.nm_fll,trv_2.nm_fll);

    /* Names are the same: store flag True for both items and read the next names from the two lists */
    if (nco_cmp == 0)
    {
      flg_in_fl[0]=True; 
      flg_in_fl[1]=True;
      cmn_lst[idx_lst].flg_in_fl[0]=flg_in_fl[0]; 
      cmn_lst[idx_lst].flg_in_fl[1]=flg_in_fl[1];
      cmn_lst[idx_lst].var_nm_fll=strdup(trv_1.nm_fll);
      idx_lst++;

      (void)fprintf(stdout,"tbl_1[%d]:%s\n",idx_tbl_1,trv_1.nm_fll);

      idx_tbl_1++;
      idx_tbl_2++;
    }
    /* Name(1) is less than Name(2), read the next name from List 1  */
    else if (nco_cmp < 0)
    {
      flg_in_fl[0]=True; 
      flg_in_fl[1]=False;
      cmn_lst[idx_lst].flg_in_fl[0]=flg_in_fl[0]; 
      cmn_lst[idx_lst].flg_in_fl[1]=flg_in_fl[1];
      cmn_lst[idx_lst].var_nm_fll=strdup(trv_1.nm_fll);
      idx_lst++;

      (void)fprintf(stdout,"tbl_1[%d]:%s\n",idx_tbl_1,trv_1.nm_fll);

      idx_tbl_1++;
    }
    /* Name(1) is greater than Name(2), read the next name from List 2 */
    else
    {
      flg_in_fl[0]=False; 
      flg_in_fl[1]=True;
      cmn_lst[idx_lst].flg_in_fl[0]=flg_in_fl[0]; 
      cmn_lst[idx_lst].flg_in_fl[1]=flg_in_fl[1];
      cmn_lst[idx_lst].var_nm_fll=strdup(trv_2.nm_fll);
      idx_lst++;

      (void)fprintf(stdout,"tbl_2[%d]:%s\n",idx_tbl_2,trv_2.nm_fll);

      idx_tbl_2++;
    }

    flg_more_names_exist = (idx_tbl_1<nbr_tbl_1 && idx_tbl_2<nbr_tbl_2) ? 1 : 0;
  } /* end while */


  /* List1 did not end */
  if (idx_tbl_1<nbr_tbl_1)
  {
    while (idx_tbl_1<nbr_tbl_1)
    {
      trv_sct trv_1=trv_tbl_1->lst[idx_tbl_1];

      flg_in_fl[0]=True; 
      flg_in_fl[1]=False;
      cmn_lst[idx_lst].flg_in_fl[0]=flg_in_fl[0]; 
      cmn_lst[idx_lst].flg_in_fl[1]=flg_in_fl[1];
      cmn_lst[idx_lst].var_nm_fll=strdup(trv_1.nm_fll);
      idx_lst++;

      (void)fprintf(stdout,"tbl_1[%d]:%s\n",idx_tbl_1,trv_1.nm_fll);

      idx_tbl_1++;
    }
  }

  /* List2 did not end */
  if (idx_tbl_2<nbr_tbl_2)
  {
    while (idx_tbl_2<nbr_tbl_2)
    {
      trv_sct trv_2=trv_tbl_2->lst[idx_tbl_2];

      flg_in_fl[0]=False; 
      flg_in_fl[1]=True;
      cmn_lst[idx_lst].flg_in_fl[0]=flg_in_fl[0]; 
      cmn_lst[idx_lst].flg_in_fl[1]=flg_in_fl[1];
      cmn_lst[idx_lst].var_nm_fll=strdup(trv_2.nm_fll);
      idx_lst++;

      (void)fprintf(stdout,"tbl_2[%d]:%s\n",idx_tbl_2,trv_2.nm_fll);

      idx_tbl_2++;
    }
  }

  /* Print the list */
  (void)fprintf(stdout,"\n");
  (void)fprintf(stdout,"file1     file2\n");
  (void)fprintf(stdout,"---------------------------------------\n");
  for(int idx=0;idx<idx_lst;idx++){
    char c1, c2;

    c1 = (cmn_lst[idx].flg_in_fl[0]) ? 'x' : ' ';
    c2 = (cmn_lst[idx].flg_in_fl[1]) ? 'x' : ' ';
    (void)fprintf(stdout,"%5c %6c    %-15s\n", c1, c2, cmn_lst[idx].var_nm_fll);

    cmn_lst[idx].var_nm_fll=(char *)nco_free(cmn_lst[idx].var_nm_fll);
  } /* end loop over idx */
  (void)fprintf(stdout,"\n");

  cmn_lst=(nco_cmn_t *)nco_free(cmn_lst);

} /* end trv_tbl_mch() */

