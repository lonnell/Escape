#
# $Id: debug.s 806 2010-09-20 13:05:03Z nasmussen $
# Copyright (C) 2008 - 2009 Nils Asmussen
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

.section .text

.include "arch/mmix/syscalls.s"

.extern errno

.global debugChar
debugChar:
	POP		0,0

.global debug
debug:
	POP		0,0

#SYSC_VOID_1ARGS debugChar,$SYSCALL_DEBUGCHAR
#SYSC_VOID_0ARGS debug,$SYSCALL_DEBUG
