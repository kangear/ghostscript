#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pxl_top.mak
# Top-level platform-independent makefile for PCL XL

# This file must be preceded by pxl.mak.

#DEVICE_DEVS is defined in the platform-specific file.
FEATURE_DEVS=colimlib.dev dps2lib.dev path1lib.dev patlib.dev psl2cs.dev rld.dev roplib.dev ttflib.dev

default: $(TARGET_XE)$(XE)
	echo Done.

clean: config-clean clean-not-config-clean

clean-not-config-clean: pl.clean-not-config-clean pxl.clean-not-config-clean
	$(RMN_) $(TARGET_XE)$(XE)

config-clean: pl.config-clean pxl.config-clean
	$(RMN_) *.tr $(GD)devs.tr$(CONFIG) $(GD)ld$(CONFIG).tr
	$(RMN_) $(PXLGEN)pconf$(CONFIG).h $(PXLGEN)pconfig.h

#### Main program

# Note: we always compile the main program with -DDEBUG.
$(PXLOBJ)pxmain.$(OBJ): $(PXLSRC)pxmain.c $(AK)\
 $(malloc__h) $(math__h) $(memory__h) $(stdio__h) $(string__h)\
 $(gdebug_h) $(gp_h)\
 $(gsargs_h) $(gscdefs_h) $(gscoord_h) $(gsdevice_h) $(gserrors_h) $(gsgc_h)\
 $(gslib_h) $(gsmatrix_h) $(gsmemory_h) $(gspaint_h) $(gsparam_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h)\
 $(gxalloc_h) $(gxstate_h)\
 $(plmain_h) $(plparse_h) $(pjparse_h)\
 $(pxattr_h) $(pxerrors_h) $(pxparse_h) $(pxptable_h) $(pxstate_h) $(pxvalue_h)\
 $(PXLGEN)pconf$(CONFIG).h
	$(CP_) $(PXLGEN)pconf$(CONFIG).h $(PXLGEN)pconfig.h
	$(PXLCCC) $(PXLSRC)pxmain.c $(PXLO_)pxmain.$(OBJ)
