/* Copyright (C) 1989-2003 artofcode, LLC.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */

//#include "MacHeaders"
#include <Palettes.h>
#include <Aliases.h>
#include <Quickdraw.h>
#include <QDOffscreen.h>
#include <AppleEvents.h>
#include <Fonts.h>
#include <Controls.h>
#include <Script.h>
#include <Timer.h>
#include <Folders.h>
#include <Resources.h>
#include <Sound.h>
#include <ToolUtils.h>
#include <Menus.h>
#include <LowMem.h>
#include <Devices.h>
#include <Scrap.h>
#include <StringCompare.h>
#include <Gestalt.h>
#include <Folders.h>
#include <Files.h>
#include <Fonts.h>
#include <FixMath.h>
#include <Resources.h>

#include "stdio_.h"
#include "math_.h"
#include "string_.h"
#include <stdlib.h>
#include <stdarg.h>
#include <console.h>

#include "gx.h"
#include "gp.h"
#include "gxdevice.h"

#include "gp_mac.h"

#include "stream.h"
#include "gxiodev.h"			/* must come after stream.h */
//#include "gp_macAE.h"
#include "gsdll.h"

//HWND hwndtext;


extern void
convertSpecToPath(FSSpec * s, char * p, int pLen)
{
	OSStatus	err = noErr;
	CInfoPBRec	params;
	Str255		dirName;
	int		totLen = 0, dirLen = 0;

	memcpy(p, s->name + 1, s->name[0]);
	totLen += s->name[0];
	
	params.dirInfo.ioNamePtr = dirName;
	params.dirInfo.ioVRefNum = s->vRefNum;
	params.dirInfo.ioDrParID = s->parID;
	params.dirInfo.ioFDirIndex = -1;
	
	do {
		params.dirInfo.ioDrDirID = params.dirInfo.ioDrParID;
		err = PBGetCatInfoSync(&params);
		
		if ((err != noErr) || (totLen + dirName[0] + 2 > pLen)) {
			p[0] = 0;
			return;
		}
		
		dirName[++dirName[0]] = ':';
		memmove(p + dirName[0], p, totLen);
		memcpy(p, dirName + 1, dirName[0]);
		totLen += dirName[0];
	} while (params.dirInfo.ioDrParID != fsRtParID);
	
	p[totLen] = 0;
	
	return;
}

OSErr
convertPathToSpec(const char *path, const int pathlength, FSSpec * spec)
{
	Str255 filename;
	
	/* path must be shorter than 255 bytes */
	if (pathlength > 254) return bdNamErr;
	
	*filename = pathlength;
	memcpy(filename + 1, path, pathlength);
	
	return FSMakeFSSpec(0, 0, filename, spec);
}

/* ------ File name syntax ------ */

/* Define the character used for separating file names in a list. */
const char gp_file_name_list_separator = ',';

/* Define the default scratch file name prefix. */
const char gp_scratch_file_name_prefix[] = "tempgs_";

/* Define the name of the null output file. */
const char gp_null_file_name[] = "????";

/* Define the name that designates the current directory. */
extern const char gp_current_directory_name[] = ":";

int fake_stdin = 0;


/* Do platform-dependent initialization */

void
setenv(const char * env, char *p) {
//	if ( strcmp(env,"outfile") == 0) {
//	   sprintf((char *)&g_fcout[0],"%s",p);
//	}
}

char *
getenv(const char * env) {

	char 			*p;
	FSSpec			pFile;
	OSErr			err = 0;
	char			fpath[256]="";
	
	if ( strcmp(env,"GS_LIB") == 0) {
	
	    	pFile.name[0] = 0;
	    	err = FindFolder(kOnSystemDisk, kApplicationSupportFolderType, kDontCreateFolder,
			 					&pFile.vRefNum, &pFile.parID);
			
			if (err != noErr) goto failed;

//		FSMakeFSSpec(pFile.vRefNum, pFile.parID,thepfname, &pfile);
		convertSpecToPath(&pFile, fpath, 256);
//		sprintf(fpath,"%s",fpath);
		p = (char*)gs_malloc(1, (size_t) ( 4*strlen(fpath) + 40), "getenv");
		sprintf(p,"%s,%sGhostscript:lib,%sGhostscript:fonts",
						(char *)&fpath[0],(char *)&fpath[0],
						(char *)&fpath[0] );

		return p;
failed:
		
		return NULL;
	} else
	    return NULL;

}

/* ====== Substitute for stdio ====== */

/* Forward references */
private void mac_std_init(void);
private stream_proc_process(mac_stdin_read_process);
private stream_proc_process(mac_stdout_write_process);
private stream_proc_process(mac_stderr_write_process);
private stream_proc_available(mac_std_available);

/* Use a pseudo IODevice to get mac_stdio_init called at the right time. */
/* This is bad architecture; we'll fix it later. */
private iodev_proc_init(mac_stdio_init);
const gx_io_device gs_iodev_macstdio =
{
    "macstdio", "Special",
    {mac_stdio_init, iodev_no_open_device,
     iodev_no_open_file, iodev_no_fopen, iodev_no_fclose,
     iodev_no_delete_file, iodev_no_rename_file,
     iodev_no_file_status, iodev_no_enumerate_files
    }
};

/* Do one-time initialization */
private int
mac_stdio_init(gx_io_device * iodev, gs_memory_t * mem)
{
    mac_std_init();		/* redefine stdin/out/err to our window routines */
    return 0;
}

/* Define alternate 'open' routines for our stdin/out/err streams. */

extern const gx_io_device gs_iodev_stdin;
private int
mac_stdin_open(gx_io_device * iodev, const char *access, stream ** ps,
	       gs_memory_t * mem)
{
    int code = gs_iodev_stdin.procs.open_device(iodev, access, ps, mem);
    stream *s = *ps;

    if (code != 1)
	return code;
    s->procs.process = mac_stdin_read_process;
    s->procs.available = mac_std_available;
    s->file = NULL;
    return 0;
}

extern const gx_io_device gs_iodev_stdout;
private int
mac_stdout_open(gx_io_device * iodev, const char *access, stream ** ps,
		gs_memory_t * mem)
{
    int code = gs_iodev_stdout.procs.open_device(iodev, access, ps, mem);
    stream *s = *ps;

    if (code != 1)
	return code;
    s->procs.process = mac_stdout_write_process;
    s->procs.available = mac_std_available;
    s->file = NULL;
    return 0;
}

extern const gx_io_device gs_iodev_stderr;
private int
mac_stderr_open(gx_io_device * iodev, const char *access, stream ** ps,
		gs_memory_t * mem)
{
    int code = gs_iodev_stderr.procs.open_device(iodev, access, ps, mem);
    stream *s = *ps;

    if (code != 1)
	return code;
    s->procs.process = mac_stderr_write_process;
    s->procs.available = mac_std_available;
    s->file = NULL;
    return 0;
}

/* Patch stdin/out/err to use our windows. */
private void
mac_std_init(void)
{
    /* If stdxxx is the console, replace the 'open' routines, */
    /* which haven't gotten called yet. */

//    if (gp_file_is_console(gs_stdin))
	gs_findiodevice((const byte *)"%stdin", 6)->procs.open_device =
	    mac_stdin_open;

//    if (gp_file_is_console(gs_stdout))
	gs_findiodevice((const byte *)"%stdout", 7)->procs.open_device =
	    mac_stdout_open;

//    if (gp_file_is_console(gs_stderr))
	gs_findiodevice((const byte *)"%stderr", 7)->procs.open_device =
	    mac_stderr_open;
}


private int
mac_stdin_read_process(stream_state *st, stream_cursor_read *ignore_pr,
  stream_cursor_write *pw, bool last)
{
    uint count = pw->limit - pw->ptr;
    /* callback to get more input */
    if (pgsdll_callback == NULL) return EOFC;
    count = (*pgsdll_callback) (GSDLL_STDIN, (char*)pw->ptr + 1, count);
	pw->ptr += count;	
	return 1;
}


private int
mac_stdout_write_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *ignore_pw, bool last)
{	uint count = pr->limit - pr->ptr;
 
    if (pgsdll_callback == NULL) return EOFC;
    (*pgsdll_callback) (GSDLL_STDOUT, (char *)(pr->ptr + 1), count);
	pr->ptr = pr->limit;
	return 0;
}

private int
mac_stderr_write_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *ignore_pw, bool last)
{	uint count = pr->limit - pr->ptr;

    if (pgsdll_callback == NULL) return EOFC;
    (*pgsdll_callback) (GSDLL_STDOUT, (char *)(pr->ptr + 1), count);
	pr->ptr = pr->limit;
	return 0;
}

private int
mac_std_available(register stream * s, long *pl)
{
    *pl = -1;		// EOF, since we can't do it
    return 0;		// OK
}

/* ====== Substitute for stdio ====== */

/* These are used instead of the stdio version. */
/* The declarations must be identical to that in <stdio.h>. */
int
fprintf(FILE *file, const char *fmt, ...)
{
	int		count;
	va_list	args;
	char	buf[1024];
	
	va_start(args,fmt);
	
	if (file != stdout  &&  file != stderr) {
		count = vfprintf(file, fmt, args);
	}
	else {
		count = vsprintf(buf, fmt, args);
		return fwrite(buf, strlen(buf), 1, file);
	}
	
	va_end(args);
	return count;
}

int
fputs(const char *string, FILE *file)
{
	if (file != stdout  &&  file != stderr) {
		return fwrite(string, strlen(string), 1, file);
	}
	else {
		return fwrite(string, strlen(string), 1, file);
	}
}


/* ------ Printer accessing ------ */

/* These should NEVER be called. */

/* Open a connection to a printer.  A null file name means use the */
/* standard printer connected to the machine, if any. */
/* "|command" opens an output pipe. */
/* Return NULL if the connection could not be opened. */

FILE *
gp_open_printer (char *fname, int binary_mode)
{
	if (strlen(fname) == 1  &&  fname[0] == '-')
		return stdout;
	else if (strlen(fname) == 0)
		return gp_open_scratch_file(gp_scratch_file_name_prefix, fname, binary_mode ? "wb" : "w");
	else
		return gp_fopen(fname, binary_mode ? "wb" : "b");
}

/* Close the connection to the printer. */

void
gp_close_printer (FILE *pfile, const char *fname)
{
	fclose(pfile);
}



/* Define whether case is insignificant in file names. */
/* OBSOLETE
const int gp_file_names_ignore_case = 1;
*/

/* Define the string to be concatenated with the file mode */
/* for opening files without end-of-line conversion. */
const char gp_fmode_binary_suffix[] = "b";

/* Define the file modes for binary reading or writing. */
const char gp_fmode_rb[] = "rb";
const char gp_fmode_wb[] = "wb";


/* Set a file into binary or text mode. */
int
gp_setmode_binary(FILE *pfile, bool binary)
{	return 0;	/* Noop under VMS */
}


/* Create and open a scratch file with a given name prefix. */
/* Write the actual file name at fname. */

FILE *
gp_open_scratch_file (const char *prefix, char *fname, const char *mode)
{
    char thefname[256];
    Str255 thepfname;
	OSErr myErr;
	short foundVRefNum;
	long foundDirID;
	FSSpec fSpec;
	FILE *f;

	strcpy (fname, (char *) prefix);
	{
		char newName[50];

		tmpnam (newName);
		strcat (fname, newName);
	}

   if ( strrchr(fname,':') == NULL ) {
       memcpy((char*)&thepfname[1],(char *)&fname[0],strlen(fname));
	   thepfname[0]=strlen(fname);
		myErr = FindFolder(kOnSystemDisk,kTemporaryFolderType,kCreateFolder,
			&foundVRefNum, &foundDirID);
		if ( myErr != noErr ) {
			fprintf(stderr,"Can't find temp folder.\n");
			return (NULL);
		}
		FSMakeFSSpec(foundVRefNum, foundDirID,thepfname, &fSpec);
		convertSpecToPath(&fSpec, thefname, sizeof(thefname) - 1);
		sprintf(fname,"%s",thefname);
   } else {
       sprintf((char*)&thefname[0],"%s\0",fname);
       memcpy((char*)&thepfname[1],(char *)&thefname[0],strlen(thefname));
	   thepfname[0]=strlen(thefname);
   }

    f = gp_fopen (thefname, mode);
    if (f == NULL)
	eprintf1("**** Could not open temporary file %s\n", fname);
    return f;
}

/*
{
    char thefname[256];
	strcpy (fname, (char *) prefix);
	{
		char newName[50];

		tmpnam (newName);
		strcat (fname, newName);
	}

   if ( strrchr(fname,':') == NULL ) 
//      sprintf((char *)&thefname[0],"%s%s\0",g_homeDir,fname);
      sprintf((char *)&thefname[0],"%s%s\0","",fname);
   else
       sprintf((char*)&thefname[0],"%s\0",fname);

	return gp_fopen (thefname, mode);
}
*/

#if !NEW_COMBINE_PATH
/* Answer whether a path_string can meaningfully have a prefix applied */
int
gp_pathstring_not_bare(const char *fname, unsigned len) {
    /* Macintosh paths are not 'bare' i.e., cannot have a prefix	*/
    /* applied with predictable results if the string contains '::'	*/
    /* If a pathstring starts with ':' we also call it not_bare since	*/
    /* prefixing a 'somedir:' string would end up with '::' which will	*/
    /* move up a directory level.					*/
    /* While MacHD:somedir:xyz is a "root" or "absolute" reference we	*/
    /* assume that this syntax is actually somedir:subdir:xyz since the	*/
    /* HardDrive name will vary from site to site ("root" or "absolute"	*/
    /* references aren't really practical on Macintosh pre OS/X)	*/
    /* As far as we can tell, this whole area is confused on Mac since	*/
    /* root level references and current_directory references look the	*/
    /* same.								*/

    if (len != 0) {
	if (*fname == ':') {	/* leading ':' */
	    return 1;		/* cannot be prefixed - not_bare, but	*/
				/* *IS* relative to the current dir.	*/
	} else {
	    char *p;
	    bool lastWasColon;

	    for (len, p = (char *)fname, lastWasColon = 0; len > 0; len--, p++) {
		if (*p == ':') {
		    if (lastWasColon != 0) 
			return 1;
		    else 
			lastWasColon = 1;
		} else
		    lastWasColon = 0;
	    }
	    return 0;	/* pathstring *ASSUMED* bare */
	    /* fixme: if the start of the pathstring up to the first ':'*/
	    /* matches a drive name, then this is really an absolute	*/
	    /* pathstring, and thus should be not_bare (return 1).	*/
	}
    } else 
	return 0;	/* empty path ?? */
}

/* Answer whether the file_name references the directory	*/
/* containing the specified path (parent). 			*/
bool
gp_file_name_references_parent(const char *fname, unsigned len)
{
    int i = 0, last_sep_pos = -1;

    /* A file name references its parent directory if it starts */
    /* with ..: or ::  or if one of these strings follows : */
    while (i < len) {
	if (fname[i] == ':') {
	    if (last_sep_pos == i - 1)
	        return true;	/* also returns true is starts with ':' */
	    last_sep_pos = i++;
	    continue;
	}
	if (fname[i++] != '.')
	    continue;
        if (i > last_sep_pos + 2 || (i < len && fname[i] != '.'))
	    continue;
	i++;
	/* have separator followed by .. */
	if (i < len && (fname[i] == ':'))
	    return true;
    }
    return false;
}

/* Answer the string to be used for combining a directory/device prefix */
/* with a base file name. The prefix directory/device is examined to	*/
/* determine if a separator is needed and may return an empty string	*/
const char *
gp_file_name_concat_string (const char *prefix, uint plen)
{
	if ( plen > 0 && prefix[plen - 1] == ':' )
		return "";
	return ":";
}

#endif /* !NEW_COMBINE_PATH */


/* read a resource and copy the data into a buffer */
/* we don't have access to an allocator, nor any context for local  */
/* storage, so we implement the following idiom: we return the size */
/* of the requested resource and copy the data into buf iff it's    */
/* non-NULL. Thus, the caller can pass NULL for buf the first time, */
/* allocate the appropriate sized buffer, and then call us a second */
/* time to actually transfer the data.                              */
int
gp_read_macresource(byte *buf, const char *fname, const uint type, const ushort id)
{
    Handle resource = NULL;
    SInt32 size = 0;
    FSSpec spec;
    SInt16 fileref;
    OSErr result;
    
    /* open file */
    result = convertPathToSpec(fname, strlen(fname), &spec);
    if (result != noErr) goto fin;
    fileref = FSpOpenResFile(&spec, fsRdPerm);
    if (fileref == -1) goto fin;
    
    if_debug1('s', "[s] loading resource from fileref %d\n", fileref);

    /* load resource */
    resource = Get1Resource((ResType)type, (SInt16)id);
    if (resource == NULL) goto fin;
          
    /* allocate res */
    /* GetResourceSize() is probably good enough */
    //size = GetResourceSizeOnDisk(resource);
    size = GetMaxResourceSize(resource);
    
    if_debug1('s', "[s] resource size on disk is %d bytes\n", size);
    
    /* if we don't have a buffer to fill, just return */
    if (buf == NULL) goto fin;

    /* otherwise, copy resource into res from handle */
    HLock(resource);
    memcpy(buf, *resource, size);
    HUnlock(resource);
    
fin:
    /* free resource, if necessary */
    ReleaseResource(resource);
    CloseResFile(fileref);
    
    return (size);
}

/* return a list of font names and corresponding paths from 
 * the native system locations
 */
int gp_native_fontmap(char *names[], char *paths[], int *count)
{
    return 0;
}

/* ------ File enumeration ------ */

/****** THIS IS NOT SUPPORTED ON MACINTOSH SYSTEMS. ******/

struct file_enum_s {
	char *pattern;
	int first_time;
	gs_memory_t *memory;
};

/* Initialize an enumeration.  NEEDS WORK ON HANDLING * ? \. */

file_enum *
gp_enumerate_files_init (const char *pat, uint patlen, gs_memory_t *memory)

{	file_enum *pfen = 
		(file_enum *)gs_alloc_bytes(memory, sizeof(file_enum), "gp_enumerate_files");
	char *pattern;
	if ( pfen == 0 ) return 0;
	pattern = 
		(char *)gs_alloc_bytes(memory, patlen + 1, "gp_enumerate_files(pattern)");
	if ( pattern == 0 ) return 0;
	memcpy(pattern, pat, patlen);
	pattern[patlen] = 0;
	pfen->pattern = pattern;
	pfen->memory = memory;
	pfen->first_time = 1;
	return pfen;
}

/* Enumerate the next file. */

uint
gp_enumerate_files_next (file_enum *pfen, char *ptr, uint maxlen)

{	if ( pfen->first_time )
	   {	pfen->first_time = 0;
	   }
	return -1;
}

/* Clean up the file enumeration. */

void
gp_enumerate_files_close (file_enum *pfen)

{	
	gs_free_object(pfen->memory, pfen->pattern, "gp_enumerate_files_close(pattern)");
	gs_free_object(pfen->memory, (char *)pfen, "gp_enumerate_files_close");
}

FILE * 
gp_fopen (const char * fname, const char * mode) {

   char thefname[256];
   FILE *fid;

//sprintf((char*)&thefname[0],"\n%s\n",fname);
//(*pgsdll_callback) (GSDLL_STDOUT, thefname, strlen(fname));
   if ( strrchr(fname,':') == NULL ) 
//      sprintf((char *)&thefname[0],"%s%s\0",g_homeDir,fname);
      sprintf((char *)&thefname[0],"%s%s\0","",fname);
   else
       sprintf((char*)&thefname[0],"%s\0",fname);
       
   fid = fopen(thefname,mode);
   
   return fid;
   
}

FILE * 
popen (const char * fname, const char * mode ) {
	return gp_fopen (fname,  mode);
}

int
pclose (FILE * pipe ) {
	return fclose (pipe);
}

/* -------------- Helpers for gp_file_name_combine_generic ------------- */

#ifdef __CARBON__

/* compare an HFSUnitStr255 with a C string */
static int compare_UniStr(HFSUniStr255 u, const char *c, uint len)
{
	int i,searchlen,unichar;
	searchlen = min(len,u.length);
	for (i = 0; i < searchlen; i++) {
	  unichar = u.unicode[i];
	  /* punt on wide characters. we should really convert */
	  if (unichar & !0xFF) return -1;
	  /* otherwise return the the index of the first non-matching character */
	  if (unichar != c[i]) break;
	}
	/* return the offset iff we matched the whole volume name */
	return (i == u.length) ? i : 0;
}

uint gp_file_name_root(const char *fname, uint len)
{
	OSErr err = noErr;
   	HFSUniStr255 volumeName;
   	FSRef rootDirectory;
   	int index, match;
   	
    if (len > 0 && fname[0] == ':')
		return 0; /* A relative path, no root. */

	/* iterate over mounted volumes and compare our path */
	index = 1;
	while (err == noErr) {
		err = FSGetVolumeInfo (kFSInvalidVolumeRefNum, index,
			NULL, kFSVolInfoNone, NULL, /* not interested in these fields */
			&volumeName, &rootDirectory);
		if (err == nsvErr) return 0; /* no more volumes */
		if (err == noErr) {
			match = compare_UniStr(volumeName, fname, len);
			if (match > 0) {
    			/* include the separator if it's present  */
				if (fname[match] == ':') return match + 1;
				return match;
			}
		}
		index++;
	}
	
	/* nothing matched */
    return 0;
}

#else /* Classic MacOS */

/* FSGetVolumeInfo requires carbonlib or macos >= 9
   we essentially leave this unimplemented on Classic */
uint gp_file_name_root(const char *fname, uint len)
{
	return 0;
}
   
#endif /* __CARBON__ */


uint gs_file_name_check_separator(const char *fname, int len, const char *item)
{   if (len > 0) {
	if (fname[0] == ':') {
	    if (fname == item + 1 && item[0] == ':')
		return 1; /* It is a separator after parent. */
	    if (len > 1 && fname[1] == ':')
		return 0; /* It is parent, not a separator. */
	    return 1;
	}
    } else if (len < 0) {
	if (fname[-1] == ':')
	    return 1;
    }
    return 0;
}

bool gp_file_name_is_parent(const char *fname, uint len)
{   return len == 1 && fname[0] == ':';
}

bool gp_file_name_is_current(const char *fname, uint len)
{   return (len == 0) || (len == 1 && fname[0] == ':');
}

const char *gp_file_name_separator(void)
{   return ":";
}

const char *gp_file_name_directory_separator(void)
{   return ":";
}

const char *gp_file_name_parent(void)
{   return "::";
}

const char *gp_file_name_current(void)
{   return ":";
}

bool gp_file_name_is_partent_allowed(void)
{   return true;
}

bool gp_file_name_is_empty_item_meanful(void)
{   return true;
}

gp_file_name_combine_result
gp_file_name_combine(const char *prefix, uint plen, const char *fname, uint flen, 
		    bool no_sibling, char *buffer, uint *blen)
{
    return gp_file_name_combine_generic(prefix, plen, 
	    fname, flen, no_sibling, buffer, blen);
}

// FIXME: there must be a system util for this!
static char *MacStr2c(char *pstring)
{
	char *cstring;
	int len = (pstring[0] < 256) ? pstring[0] : 255;

	if (len == 0) return NULL;
	
	cstring = malloc(len + 1);
	if (cstring != NULL) {
		memcpy(cstring, &(pstring[1]), len);
		cstring[len] = '\0';
	}
	
	return(cstring);
}

/* ------ Font enumeration ------ */
                                                                                
 /* This is used to query the native os for a list of font names and
  * corresponding paths. The general idea is to save the hassle of
  * building a custom fontmap file
  */

typedef struct {
    int size, style, id;
} refentry;

typedef struct {
    int entries;
    refentry *refs;
} reftable;

static reftable *reftable_new(int entries)
{
    reftable *table = malloc(sizeof(reftable));
    if (table != NULL) {
        table->entries = entries;
        table->refs = malloc(entries * sizeof(refentry));
        if (table->refs == NULL) { free(table); table = NULL; }
    }
    return table;
}

static void reftable_free(reftable *table)
{
    if (table != NULL) {
        if (table->refs) free(table->refs);
        free(table);
    }
}

static reftable *reftable_grow(reftable *table, int entries)
{
    if (table == NULL) {
        table = reftable_new(entries);
    } else {
        table->entries += entries;
        table->refs = realloc(table->refs, table->entries * sizeof(refentry));
    }
    return table;
}

static int get_int16(unsigned char *p) {
    return (p[0]&0xFF)<<8 | (p[1]&0xFF);
}

static int get_int32(unsigned char *p) {
    return (p[0]&0xFF)<<24 | (p[1]&0xFF)<<16 | (p[2]&0xFF)<<8 | (p[3]&0xFF);
}

/* parse and summarize FOND resource information */
static reftable * parse_fond(FSSpec *spec)
{
    OSErr result = noErr;
    FSRef specref;
    Handle fond = NULL;
    unsigned char *res;
    reftable *table = NULL;
    SInt16 ref;
    char *filename = MacStr2c(spec->name);
    int i,j, count, n, start;
    
    start = 0;
    
    result = FSpMakeFSRef(spec,&specref);
    if (result == noErr)
      result = FSOpenResourceFile(&specref, 0, NULL, fsRdPerm, &ref);
    if (result != noErr) {
      if (result = mapReadErr) {
        /* this may mean there's no resource map, i.e. it's
           it's a .ttf or .otf file, but also occurs if the
           map structure on disk isn't 'internally consistent'
           which is unfortunately rather common, so we ignore
           this an hope for the best. */
        dlprintf("map read error opening resource file...ignoring\n");
      } else {
      	dlprintf2("unable to open resource file '%s' (error %d)\n", filename, result);
      	return NULL;
      }
    }
    if (ref == -1) goto fin;
    
    UseResFile(ref);
    count = Count1Resources('FOND');
    for (i = 0; i < count; i++) {
        fond = Get1IndResource('FOND', i+1);
        if (fond == NULL) {
            result = ResError();
            goto fin;
        }
        
        /* The FOND resource structure corresponds to the FamRec and AsscEntry
           data structures documented in the FontManager reference. However,
           access to these types is deprecated in Carbon. We therefore access the
           data by direct offset in the hope that the resource format will not change
           even in api access to the in-memory versions goes away. */
        HLock(fond);
        res = *fond + 52; /* offset to association table */
        n = get_int16(res) + 1;	res += 2;
        dlprintf1("found %d fonts in resource file\n", n);
		table = reftable_grow(table, n);
        for (j = start; j < start + n; j++ ) {
            table->refs[j].size = get_int16(res); res += 2;
            table->refs[j].style = get_int16(res); res += 2;
            table->refs[j].id = get_int16(res); res += 2;
        }
        start += n;
        HUnlock(fond);
    }
fin:
    CloseResFile(ref);
    return table;
}

/* FIXME: should check for uppercase as well */
static int is_ttf_file(const char *path)
{
    int len = strlen(path);
    return !memcmp(path+len-4,".ttf",4);
}
static int is_otf_file(const char *path)
{
    int len = strlen(path);
    return !memcmp(path+len-4,".otf",4);
}

static void strip_char(char *string, int len, const int c)
{
    char *bit;
    len += 1;
    while(bit = strchr(string,' ')) {
        memmove(bit, bit + 1, string + len - bit - 1);
    }
}

/* get the macos name for the font instance and mangle it into a PS
   fontname */
static char *makePSFontName(FMFontFamily Family, FMFontStyle Style)
{
	Str255 Name;
	OSStatus result;
	int length;
	char *stylename, *fontname;
	char *psname;
	
	result = FMGetFontFamilyName(Family, Name);
	if (result != noErr) return NULL;
	fontname = MacStr2c(Name);
	if (fontname == NULL) return NULL;
	strip_char(fontname, strlen(fontname), ' ');
	
	switch (Style) {
		case 0: stylename=""; break;;
		case 1: stylename="Bold"; break;;
		case 2: stylename="Italic"; break;;
		case 3: stylename="BoldItalic"; break;;
		default: stylename="Unknown"; break;;
	}
	
	length = strlen(fontname) + strlen(stylename) + 2;
	psname = malloc(length);
	if (Style != 0)
		snprintf(psname, length, "%s-%s", fontname, stylename);
	else
		snprintf(psname, length, "%s", fontname);
		
	free(fontname);
	
	return psname;	
}
                                             
typedef struct {
    int count;
    FMFontIterator Iterator;
    char *name;
    char *path;
} fontenum_t;
                                                                                
void *gp_enumerate_fonts_init(gs_memory_t *mem)
{
    fontenum_t *state = gs_alloc_bytes(mem, sizeof(fontenum_t),
	"macos font enumerator state");
	FMFontIterator *Iterator = &state->Iterator;
	OSStatus result;
    
    if (state != NULL) {
		state->count = 0;
		state->name = NULL;
		state->path = NULL;
		result = FMCreateFontIterator(NULL, NULL,
			kFMUseGlobalScopeOption, Iterator);
		if (result != noErr) return NULL;
    }

    return (void *)state;
}
                                   
int gp_enumerate_fonts_next(void *enum_state, char **name, char **path)
{
    fontenum_t *state = (fontenum_t *)enum_state;
	FMFontIterator *Iterator = &state->Iterator;
	FMFont Font;
	FourCharCode Format;
	FMFontFamily FontFamily;
	FMFontStyle Style;
	FSSpec FontContainer;
	char type[5];
	char fontpath[256];
	char *fontname;
	reftable *table;
	OSStatus result;
    	
	result = FMGetNextFont(Iterator, &Font);
    if (result != noErr) return 0;

	result = FMGetFontFormat(Font, &Format);
	type[0] = ((char*)&Format)[0];
	type[1] = ((char*)&Format)[1];
	type[2] = ((char*)&Format)[2];
	type[3] = ((char*)&Format)[3];
	type[4] = '\0';

 	FMGetFontFamilyInstanceFromFont(Font, &FontFamily, &Style);
    if (state->name) free (state->name);
    
    fontname = makePSFontName(FontFamily, Style);
    if (fontname == NULL) {
		state->name = strdup("GSPlaceHolder");
	} else {
		state->name = fontname;
	}
    	
	result = FMGetFontContainer(Font, &FontContainer);
	if (state->path) {
		free(state->path);
		state->path = NULL;
	}
	convertSpecToPath(&FontContainer, fontpath, 256);
    table = parse_fond(&FontContainer);
    if (table != NULL) {
    	int i;
    	for (i = 0; i < table->entries; i++) {
            if (table->refs[i].size == 0) { /* ignore non-scalable fonts */
                if (table->refs[i].style == Style) {
                    int len = strlen(fontpath) + strlen("%macresource%#sfnt+") + 6;
                	state->path = malloc(len);
                    snprintf(state->path, len, "%%macresource%%%s#sfnt+%d", 
                        fontpath, table->refs[i].id);
                    break;
                }
            }
        }
        reftable_free(table);
        if (state->path == NULL) dlprintf1("couldn't find reource matching font '%s'\n",
        	state->name);
    } else {
        /* regular font file */
        state->path = strdup(fontpath);
    }
    
    *name = state->name;
    *path = state->path;

	dlprintf2("nativefontenum: returning '%s'\n in '%s'\n", *name, *path);
	state->count += 1;
	//return (state->count < 24) ? 1 : 0;
	return 1;
}
                                                                                
void gp_enumerate_fonts_free(void *enum_state)
{
    fontenum_t *state = (fontenum_t *)enum_state;
	FMFontIterator *Iterator = &state->Iterator;
	
	FMDisposeFontIterator(Iterator);
	
    /* free any gs_malloc() stuff here */
    if (state->name) free(state->name);
    if (state->path) free(state->path);
    /* the garbage collector will take care of the struct itself */
    
}
