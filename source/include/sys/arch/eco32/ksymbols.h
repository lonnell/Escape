/**
 * $Id: ksymbols.h 847 2010-10-04 01:25:15Z nasmussen $
 * Copyright (C) 2008 - 2009 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#if DEBUGGING
#	if TESTING
#		include "../../../../build/eco32-debug/kernelt_symbols.txt"
#	else
#		include "../../../../build/eco32-debug/kernel_symbols.txt"
#	endif
#else
#	if TESTING
#		include "../../../../build/eco32-release/kernelt_symbols.txt"
#	else
#		include "../../../../build/eco32-release/kernel_symbols.txt"
#	endif
#endif
