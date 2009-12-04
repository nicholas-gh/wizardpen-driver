# Copyright (c) 2005 Nickolay Kondrashov
#
# This file is part of wizardpen-calibrate.
#
# Wizardpen-calibrate is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Wizardpen-calibrate is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with wizardpen-calibrate; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

PREFIX=/usr/local

build: wizardpen-calibrate

install: build
	install -m755 wizardpen-calibrate ${PREFIX}/bin/wizardpen-calibrate

wizardpen-calibrate: wizardpen-calibrate.c
	gcc -o wizardpen-calibrate wizardpen-calibrate.c

clean:
	rm -f wizardpen-calibrate
