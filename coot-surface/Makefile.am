# Makefile.am
# 
# Copyright 2002 The University of York
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

AM_CPPFLAGS = -I$(top_srcdir) \
	$(MMDB_CXXFLAGS) $(CLIPPER_CXXFLAGS) $(GTKGL_CFLAGS) $(GLUT_CFLAGS)

lib_LTLIBRARIES = libcoot-surface.la

libcoot_surface_la_SOURCES = \
	 coot-surface.cc coot-surface.hh rgbreps.cc rgbreps.h

# We should provide the OpenGL library in the list of dependencies for
# libcoot-surface.  Usually it resolved to -lGL, but on Windows - we
# need this to be blank (otherwise it doesn't compile).
#
libcoot_surface_la_LIBADD = \
	$(top_builddir)/surface/libcoot-electrostatics.la \
	$(CLIPPER_LIBS) $(MMDB_LIBS) $(GLUT_LIBS) $(COOT_OPENGL_LIBS)

libcoot_surface_la_LDFLAGS = $(SHARED_LDFLAGS)
