# Makefile.am
# 
# Copyright 2001, 2002, 2003, 2004, 2005, 2006, 2007 The University of York
# Author: Paul Emsley
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at
# your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA

## Process this file with automake to produce Makefile.in

# Tell autoreconf where to find macros
ACLOCAL_AMFLAGS=-I macros

# see configure.in for po intl 
#
# coot-utils depends on mini-mol.
# geometry depends on utils
#

if COND_LIDIA_ONLY
   SUBDIRS = compat utils mini-mol geometry lidia-core lbg
else
   SUBDIRS = auxil scheme python pdb-templates macros compat utils protein_db mini-mol geometry  \
        coot-utils surface coot-surface lidia-core \
	coords angles skeleton ccp4mg-utils density-contour \
	db-main analysis ideal ligand high-res build greg-tests python-tests \
	tw libgtkgraph lbg cootaneer cootilus goograph pli pyrogen src data \
	man rama-data doc rcrane pixmaps
endif

# po, no po today.

cootrc: $(top_srcdir)/cootrc.template
	$(MAKE_COOTRC_CMD) $(top_srcdir)/cootrc.template > $@


EXTRA_DIST = 		\
	autogen.sh 	\
	make-scheme-tar \
	cootrc.template	\
	py-compile      \
	standard-residues.pdb \
	BUGS		\
	depcomp 	\
	libtool 	\
	config.sub 	\
	config.guess 	\
	install-sh 	\
	missing 	\
	TODO 		\
	count-loc 	\
	syminfo.lib	\
	colours.def	\
	my.supp		\
	guile.supp	\
	windows/syminfo.lib	\
	fix-cif-loops.awk       \
	ifdef-for-create-aboutdialog.awk \
	mtrix-to-ncs-matrix.awk  \
	RELEASE-NOTES	         \
	copyright-disclaimer.txt \
	intltool-extract.in      \
	intltool-merge.in        \
	intltool-update.in 

# install-data-local:
# 	@$(NORMAL_INSTALL)
# 	if test -d $(srcdir)/pixmaps; then \
# 	  $(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/pixmaps; \
# 	  for pixmap in $(srcdir)/pixmaps/*; do \
# 	    if test -f $$pixmap; then \
# 	      $(INSTALL_DATA) $$pixmap $(DESTDIR)$(pkgdatadir)/pixmaps; \
# 	    fi \
# 	  done \
# 	fi
# 	if test -z "$(COOT_WINDOWS_CFLAGS)" ; then \
# 	  $(INSTALL_DATA) $(srcdir)/syminfo.lib $(DESTDIR)$(pkgdatadir); \
# 	  else \
# 	  $(INSTALL_DATA) $(srcdir)/windows/syminfo.lib $(DESTDIR)$(pkgdatadir); \
# 	fi

# 20140523 Surely this is not needed now?  I have a Makefile in pixmaps.
#
#dist-hook:
#	if test -d $(srcdir)/pixmaps; then \
#	  mkdir $(distdir)/pixmaps; \
#	  for pixmap in $(srcdir)/pixmaps/*; do \
#	    if test -f $$pixmap; then \
#	      cp -p $$pixmap $(distdir)/pixmaps; \
#	    fi \
#	  done \
#	fi

subpkgdatadir = $(pkgdatadir)
subpkgdata_DATA = cootrc standard-residues.pdb colours.def \
	mtrix-to-ncs-matrix.awk syminfo.lib


appdatadir = ${datarootdir}/appdata
dist_appdata_DATA = coot.appdata.xml

applicationdir = ${datarootdir}/applications
dist_application_DATA = coot.desktop


