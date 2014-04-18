##
## This file is part of the libsigrok project.
##
## Copyright (C) 2014 Martin Ling <martin-sigrok@earth.li>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

lib_LTLIBRARIES = libsigrokxx.la

libsigrokxx_la_SOURCES = classes.cpp

libsigrokxx_la_LIBADD = -lsigrok $(GLIBMM_LIBS)

libsigrokxx_la_LDFLAGS = $(SR_LIB_LDFLAGS)

library_includedir = $(includedir)/libsigrok
library_include_HEADERS = libsigrok.hpp enums.hpp \
	QuantityFlag_methods.hpp \
	ConfigKey_methods.hpp

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libsigrokxx.pc

classes.lo: enums.hpp

enums.hpp: enums.py libsigrok.xml
	python $<

libsigrok.xml: ../../libsigrok.h
	gccxml $(CFLAGS) -DGLIB_DISABLE_DEPRECATION_WARNINGS $< -fxml=$@
