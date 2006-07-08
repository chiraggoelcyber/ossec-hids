/*   $OSSEC, create_db.c, v0.3, 2005/10/05, Daniel B. Cid$   */

/* Copyright (C) 2005 Daniel B. Cid <dcid@ossec.net>
 * All right reserved.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */

/* v0.3 (2005/10/05): Adding st_mode, owner uid and group owner.
 * v0.2 (2005/08/22): Removing st_ctime, bug 1104
 * v0.1 (2005/07/15)
 */
 
#include <stdio.h>       
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>

#include "os_crypto/md5/md5_op.h"

#include "headers/debug_op.h"

#include "syscheck.h"

/** Prototypes **/
int read_dir(char *dir_name, int opts);

int read_file(char *file_name, int opts)
{
    struct stat statbuf;
    
    /* Win32 does not have lstat */
    #ifdef WIN32
    if(stat(file_name, &statbuf) < 0)
    #else
    if(lstat(file_name, &statbuf) < 0)
    #endif
    {
        merror("%s: Error accessing '%s'",ARGV0,file_name);
        return(-1);
    }
    
    if(S_ISDIR(statbuf.st_mode))
    {
        #ifdef DEBUG
        verbose("%s: Reading dir: %s\n",ARGV0, file_name);
        #endif

        return(read_dir(file_name, opts));
    }
    
    /* No S_ISLNK on windows */
    #ifdef WIN32
    else if(S_ISREG(statbuf.st_mode))
    #else
    else if(S_ISREG(statbuf.st_mode) || S_ISLNK(statbuf.st_mode))
    #endif    
    {
        os_md5 f_sum;

        /* generating md5 of the file */
        if(OS_MD5_File(file_name, f_sum) < 0)
        {
            /* Not add files we can't md5sum */
            /* fprintf(syscheck.fp,"-1 %s\n",file_name); */
        }

        else
        {
            fprintf(syscheck.fp,"%c%c%c%c%c%d:%d:%d:%d:%s %s\n",
                    opts & CHECK_SIZE?'+':'-',
                    opts & CHECK_PERM?'+':'-',
                    opts & CHECK_OWNER?'+':'-',
                    opts & CHECK_GROUP?'+':'-',
                    opts & CHECK_SUM?'+':'-',
                    opts & CHECK_SIZE?(int)statbuf.st_size:0,
                    opts & CHECK_PERM?(int)statbuf.st_mode:0,
                    opts & CHECK_OWNER?(int)statbuf.st_uid:0,
                    opts & CHECK_GROUP?(int)statbuf.st_gid:0,
                    opts & CHECK_SUM?f_sum:"xxx",
                    file_name);
        }
        
        #ifdef DEBUG 
        verbose("%s: file '%s %s'\n",ARGV0, file_name,f_sum);
        #endif
    }
    else
    {
        #ifdef DEBUG
        verbose("%s: *** IRREG file: '%s'\n",ARGV0,file_name);
        #endif
    }

    return(0);
}

/* read_dir v0.1
 *
 */
int read_dir(char *dir_name, int opts)
{
    int dir_size;
   
    char f_name[PATH_MAX +2]; 
    DIR *dp;
    
	struct dirent *entry;
	
    if((dir_name == NULL)||((dir_size = strlen(dir_name)) > PATH_MAX))
    {
        merror("%s: Invalid directory given",ARGV0);
        return(-1);
    }
    
    /* Opening the directory given */
    dp = opendir(dir_name);
	if(!dp)
    {
        merror("%s: Error opening directory: '%s': %s ",
                                              ARGV0,
                                              dir_name,
                                              strerror(errno));
        return(-1);
    }

    while((entry = readdir(dp)) != NULL)
    {
        char *s_name;
        
        /* Just ignore . and ..  */
        if((strcmp(entry->d_name,".") == 0) ||
           (strcmp(entry->d_name,"..") == 0))  
            continue;
            
        strcpy(f_name,dir_name);
       
        s_name = f_name;
        
        s_name += dir_size;

        /* checking if the file name is already null terminated */
        if(*(s_name-1) != '/')
            *s_name++ = '/';
            
        *s_name = '\0';
        
        strncpy(s_name,entry->d_name,PATH_MAX-dir_size);
        read_file(f_name, opts);
    }

    closedir(dp);
    
    return(0);
}

/* create_db v0.1
 *
 */
int create_db()
{
    int i = 0;
    char **dir_name;
    
    dir_name = syscheck.dir;

    syscheck.fp = fopen(syscheck.db,"w+"); /* Read and write */
    if(!syscheck.fp)
    {
        ErrorExit("%s: Unable to create syscheck database "
                  "at '%s'. Exiting..",ARGV0,syscheck.db);
        return(0);    
    }

    /* Creating an local fp only */
    if(syscheck.notify == QUEUE)
    {
        unlink(syscheck.db);
    }
    
    /* dir_name can't be null */
    if(dir_name == NULL || *dir_name == NULL)
    {
        merror("%s: No directories to check.",ARGV0);
        return(-1);
    }
    
    do
    {
        read_dir(dir_name[i], syscheck.opts[i]);
        i++;
    }while(dir_name[i] != NULL);

    return(0);

}

/* EOF */
