#
# $Id$
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

# the IRQ for syscalls
.set SYSCALL_IRQ,						0x30

# macros for the different syscall-types (void / ret-value, arg-count, error-handling)

.macro SYSC_VOID_0ARGS name syscno
.global \name
.type \name, @function
\name:
	mov		$\syscno,%eax					# set syscall-number
	int		$SYSCALL_IRQ
	ret
.endm

.macro SYSC_VOID_1ARGS name syscno
.global \name
.type \name, @function
\name:
	mov		$\syscno,%eax					# set syscall-number
	mov		4(%esp),%ecx					# set arg1
	int		$SYSCALL_IRQ
	ret
.endm

.macro SYSC_RET_0ARGS name syscno
.global \name
.type \name, @function
\name:
	mov		$\syscno,%eax					# set syscall-number
	int		$SYSCALL_IRQ
	ret													# return-value is in eax
.endm

.macro SYSC_RET_0ARGS_ERR name syscno
.global \name
.type \name, @function
\name:
	mov		$\syscno,%eax					# set syscall-number
	int		$SYSCALL_IRQ
	test	%ecx,%ecx
	jz		1f										# no-error?
	STORE_ERRNO
	mov		%ecx,%eax							# return error-code
1:
	ret
.endm

.macro SYSC_RET_1ARGS_ERR name syscno
.global \name
.type \name, @function
\name:
	mov		$\syscno,%eax					# set syscall-number
	mov		4(%esp),%ecx					# set arg1
	int		$SYSCALL_IRQ
	test	%ecx,%ecx
	jz		1f										# no-error?
	STORE_ERRNO
	mov		%ecx,%eax							# return error-code
1:
	ret
.endm

.macro SYSC_RET_2ARGS_ERR name syscno
.global \name
.type \name, @function
\name:
	mov		$\syscno,%eax					# set syscall-number
	mov		4(%esp),%ecx					# set arg1
	mov		8(%esp),%edx					# set arg2
	int		$SYSCALL_IRQ
	test	%ecx,%ecx
	jz		1f										# no-error?
	STORE_ERRNO
	mov		%ecx,%eax							# return error-code
1:
	ret
.endm

.macro SYSC_RET_3ARGS_ERR name syscno
.global \name
.type \name, @function
\name:
	push	%ebp
	mov		%esp,%ebp
	mov		8(%ebp),%ecx					# set arg1
	mov		12(%ebp),%edx					# set arg2
	pushl	16(%ebp)							# push arg3
	mov		$\syscno,%eax					# set syscall-number
	int		$SYSCALL_IRQ
	add		$4,%esp								# remove arg3
	test	%ecx,%ecx
	jz		1f										# no-error?
	STORE_ERRNO
	mov		%ecx,%eax							# return error-code
1:
	leave
	ret
.endm

.macro SYSC_RET_4ARGS_ERR name syscno
.global \name
.type \name, @function
\name:
	push	%ebp
	mov		%esp,%ebp
	mov		8(%ebp),%ecx					# set arg1
	mov		12(%ebp),%edx					# set arg2
	pushl	20(%ebp)							# push arg4
	pushl	16(%ebp)							# push arg3
	mov		$\syscno,%eax					# set syscall-number
	int		$SYSCALL_IRQ
	add		$8,%esp								# remove arg3 and arg4
	test	%ecx,%ecx
	jz		1f										# no-error?
	STORE_ERRNO
	mov		%ecx,%eax							# return error-code
1:
	leave
	ret
.endm

.macro SYSC_RET_5ARGS_ERR name syscno
.global \name
.type \name, @function
\name:
	push	%ebp
	mov		%esp,%ebp
	mov		8(%ebp),%ecx					# set arg1
	mov		12(%ebp),%edx					# set arg2
	pushl	24(%ebp)							# push arg5
	pushl	20(%ebp)							# push arg4
	pushl	16(%ebp)							# push arg3
	mov		$\syscno,%eax					# set syscall-number
	int		$SYSCALL_IRQ
	add		$12,%esp							# remove arg3, arg4 and arg5
	test	%ecx,%ecx
	jz		1f										# no-error?
	STORE_ERRNO
	mov		%ecx,%eax							# return error-code
1:
	leave
	ret
.endm

.macro SYSC_RET_7ARGS_ERR name syscno
.global \name
.type \name, @function
\name:
	push	%ebp
	mov		%esp,%ebp
	mov		8(%ebp),%ecx					# set arg1
	mov		12(%ebp),%edx					# set arg2
	pushl	32(%ebp)							# push arg7
	pushl	28(%ebp)							# push arg6
	pushl	24(%ebp)							# push arg5
	pushl	20(%ebp)							# push arg4
	pushl	16(%ebp)							# push arg3
	mov		$\syscno,%eax					# set syscall-number
	int		$SYSCALL_IRQ
	add		$20,%esp							# remove args
	test	%ecx,%ecx
	jz		1f										# no-error?
	STORE_ERRNO
	mov		%ecx,%eax							# return error-code
1:
	leave
	ret
.endm

# stores the received error-code (in ecx) to the global variable errno
.macro STORE_ERRNO
	# use GOT for shared-libraries
	.ifdef SHAREDLIB
	# first get address of GOT
	call	__i686.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_,%eax
	# now access symbol and store errno
	mov		errno@GOT(%eax),%eax
	mov		%ecx,(%eax)
	.else
	# otherwise access errno directly
	mov		%ecx,(errno)					# store error-code
	.endif
.endm

