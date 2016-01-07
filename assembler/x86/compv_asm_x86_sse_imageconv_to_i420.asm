; Copyright (C) 2016 Doubango Telecom <https://www.doubango.org>
;
; This file is part of Open Source ComputerVision (a.k.a CompV) project.
; Source code hosted at https://github.com/DoubangoTelecom/compv
; Website hosted at http://compv.org
;
; CompV is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; CompV is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with CompV.
;
%include "compv_asm_x86_common.asm"

COMPV_YASM_DEFAULT_REL

global sym(rgbaToI420Kernel11_CompY_Asm_X86_Aligned0x_SSSE3)
global sym(rgbaToI420Kernel11_CompY_Asm_X86_Aligned1x_SSSE3)
global sym(rgbaToI420Kernel41_CompY_Asm_X86_Aligned00_SSSE3)
global sym(rgbaToI420Kernel41_CompY_Asm_X86_Aligned01_SSSE3)
global sym(rgbaToI420Kernel41_CompY_Asm_X86_Aligned10_SSSE3)
global sym(rgbaToI420Kernel41_CompY_Asm_X86_Aligned11_SSSE3)
global sym(rgbaToI420Kernel11_CompUV_Asm_X86_Aligned0xx_SSSE3)
global sym(rgbaToI420Kernel11_CompUV_Asm_X86_Aligned1xx_SSSE3)
global sym(rgbaToI420Kernel41_CompUV_Asm_X86_Aligned0xx_SSSE3)
global sym(rgbaToI420Kernel41_CompUV_Asm_X86_Aligned1xx_SSSE3)

section .data
	extern sym(k16_i16)
	extern sym(k128_i16)
	extern sym(kRGBAToYUV_YCoeffs8)
	extern sym(kRGBAToYUV_UCoeffs8)
	extern sym(kRGBAToYUV_VCoeffs8)
	extern sym(kRGBAToYUV_U2V2Coeffs8)

section .text

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outYPtr
; arg(2) -> size_t height
; arg(3) -> size_t width
; arg(4) -> size_t stride
; %1 -> 1: rgbaPtr is aligned, 0: rgbaPtr not aligned
%macro rgbaToI420Kernel11_CompY_Asm_X86_SSSE3 1
push rbp
	mov rbp, rsp
	COMPV_YASM_SHADOW_ARGS_TO_STACK 5
	push rsi
	push rdi
	push rbx
	; end prolog

	mov rax, arg(3)
	add rax, 3
	and rax, -4
	mov rcx, arg(4)
	sub rcx, rax
	mov rdx, rcx ; rdx = padY
	shl rcx, 2 ; rcx =  padRGBA
	
	mov rax, arg(0) ; rgbaPtr
	mov rsi, arg(2) ; height
	mov rbx, arg(1) ; outYPtr

	movdqa xmm0, [sym(kRGBAToYUV_YCoeffs8)]
	movdqa xmm1, [sym(k16_i16)]

	.LoopHeight:
		xor rdi, rdi
		.LoopWidth:
			%if %1 == 1
			movdqa xmm2, [rax] ; 4 RGBA samples
			%else
			movdqu xmm2, [rax] ; 4 RGBA samples
			%endif
			pmaddubsw xmm2, xmm0
			phaddw xmm2, xmm2
			psraw xmm2, 7
			paddw xmm2, xmm1
			packuswb xmm2, xmm2
			movd [rbx], xmm2

			add rbx, 4
			add rax, 16

			; end-of-LoopWidth
			add rdi, 4
			cmp rdi, arg(3)
			jl .LoopWidth
	add rbx, rdx
	add rax, rcx
	; end-of-LoopHeight
	sub rsi, 1
	cmp rsi, 0
	jg .LoopHeight

	; begin epilog
	pop rbx
	pop rdi
	pop rsi
	COMPV_YASM_UNSHADOW_ARGS
	mov rsp, rbp
	pop rbp
	ret
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outYPtr
; arg(2) -> size_t height
; arg(3) -> size_t width
; arg(4) -> size_t stride
;;; void rgbaToI420Kernel11_CompY_Asm_X86_Aligned0x_SSSE3(const uint8_t* rgbaPtr, uint8_t* outYPtr, size_t height, size_t width, size_t stride)
sym(rgbaToI420Kernel11_CompY_Asm_X86_Aligned0x_SSSE3):
	rgbaToI420Kernel11_CompY_Asm_X86_SSSE3 0

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> COMV_ALIGNED(16) const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outYPtr
; arg(2) -> size_t height
; arg(3) -> size_t width
; arg(4) -> size_t stride
;;; void rgbaToI420Kernel11_CompY_Asm_X86_Aligned1x_SSSE3(COMV_ALIGNED(16) const uint8_t* rgbaPtr, uint8_t* outYPtr, size_t height, size_t width, size_t stride)
sym(rgbaToI420Kernel11_CompY_Asm_X86_Aligned1x_SSSE3):
	rgbaToI420Kernel11_CompY_Asm_X86_SSSE3 1

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outYPtr
; arg(2) -> size_t height
; arg(3) -> size_t width
; arg(4) -> size_t stride
; %1 -> 1: rgbaPtr aligned, 0: rgbaPtr not aligned
; %2 -> 1: outYPtr aligned, 0: outYPtr not aligned
%macro rgbaToI420Kernel41_CompY_Asm_SSSE3 2
	push rbp
	mov rbp, rsp
	COMPV_YASM_SHADOW_ARGS_TO_STACK 5
	push rsi
	push rdi
	push rbx
	; end prolog

	mov rdx, arg(3)
	add rdx, 15
	and rdx, -16
	mov rcx, arg(4)
	sub rcx, rdx ; rcx = padY
	mov rdx, rcx 
	shl rdx, 2 ; rdx = padRGBA
		
	mov rax, arg(0) ; rgbaPtr
	mov rsi, arg(2) ; height
	mov rbx, arg(1) ; outYPtr

	movdqa xmm0, [sym(kRGBAToYUV_YCoeffs8)]
	movdqa xmm1, [sym(k16_i16)]

	.LoopHeight:
		xor rdi, rdi
		.LoopWidth:
			%if %1 == 1
			movdqa xmm2, [rax] ; 4 RGBA samples
			movdqa xmm3, [rax + 16] ; 4 RGBA samples
			movdqa xmm4, [rax + 32] ; 4 RGBA samples
			movdqa xmm5, [rax + 48] ; 4 RGBA samples
			%else
			movdqu xmm2, [rax] ; 4 RGBA samples
			movdqu xmm3, [rax + 16] ; 4 RGBA samples
			movdqu xmm4, [rax + 32] ; 4 RGBA samples
			movdqu xmm5, [rax + 48] ; 4 RGBA samples
			%endif

			pmaddubsw xmm2, xmm0
			pmaddubsw xmm3, xmm0
			pmaddubsw xmm4, xmm0
			pmaddubsw xmm5, xmm0

			phaddw xmm2, xmm3
			phaddw xmm4, xmm5
			
			psraw xmm2, 7
			psraw xmm4, 7
			
			paddw xmm2, xmm1
			paddw xmm4, xmm1
						
			packuswb xmm2, xmm4
			%if %2 == 1
			movdqa [rbx], xmm2
			%else
			movdqu [rbx], xmm2
			%endif

			add rbx, 16
			add rax, 64

			; end-of-LoopWidth
			add rdi, 16
			cmp rdi, arg(3)
			jl .LoopWidth
	add rbx, rcx
	add rax, rdx
	; end-of-LoopHeight
	sub rsi, 1
	cmp rsi, 0
	jg .LoopHeight

	; begin epilog
	pop rbx
	pop rdi
	pop rsi
    COMPV_YASM_UNSHADOW_ARGS
	mov rsp, rbp
	pop rbp
	ret
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outYPtr
; arg(2) -> size_t height
; arg(3) -> size_t width
; arg(4) -> size_t stride
;;; void rgbaToI420Kernel41_CompY_Asm_X86_Aligned00_SSSE3(const uint8_t* rgbaPtr, uint8_t* outYPtr, size_t height, size_t width, size_t stride)
sym(rgbaToI420Kernel41_CompY_Asm_X86_Aligned00_SSSE3):
	rgbaToI420Kernel41_CompY_Asm_SSSE3 0, 0

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> const uint8_t* rgbaPtr
; arg(1) -> COMV_ALIGNED(16) uint8_t* outYPtr
; arg(2) -> size_t height
; arg(3) -> size_t width
; arg(4) -> size_t stride
;;; void rgbaToI420Kernel41_CompY_Asm_X86_Aligned01_SSSE3(const uint8_t* rgbaPtr, COMV_ALIGNED(16) uint8_t* outYPtr, size_t height, size_t width, size_t stride)
sym(rgbaToI420Kernel41_CompY_Asm_X86_Aligned01_SSSE3):
	rgbaToI420Kernel41_CompY_Asm_SSSE3 0, 1

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> COMV_ALIGNED(16) const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outYPtr
; arg(2) -> size_t height
; arg(3) -> size_t width
; arg(4) -> size_t stride
;;; void rgbaToI420Kernel41_CompY_Asm_X86_Aligned10_SSSE3(COMV_ALIGNED(16) const uint8_t* rgbaPtr, uint8_t* outYPtr, size_t height, size_t width, size_t stride)
sym(rgbaToI420Kernel41_CompY_Asm_X86_Aligned10_SSSE3):
	rgbaToI420Kernel41_CompY_Asm_SSSE3 1, 0

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> COMV_ALIGNED(16) const uint8_t* rgbaPtr
; arg(1) -> COMV_ALIGNED(16) uint8_t* outYPtr
; arg(2) -> size_t height
; arg(3) -> size_t width
; arg(4) -> size_t stride
;;; void rgbaToI420Kernel41_CompY_Asm_X86_Aligned11_SSSE3(COMV_ALIGNED(16) const uint8_t* rgbaPtr, COMV_ALIGNED(16) uint8_t* outYPtr, size_t height, size_t width, size_t stride)
sym(rgbaToI420Kernel41_CompY_Asm_X86_Aligned11_SSSE3):
	rgbaToI420Kernel41_CompY_Asm_SSSE3 1, 1

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outUPtr
; arg(2) -> uint8_t* outVPtr
; arg(3) -> size_t height
; arg(4) -> size_t width
; arg(5) -> size_t stride
; %1 -> 1: rgbaPtr is aligned, 0: rgbaPtr not aligned
%macro rgbaToI420Kernel11_CompUV_Asm_X86_SSSE3 1
	push rbp
	mov rbp, rsp
	COMPV_YASM_SHADOW_ARGS_TO_STACK 6
	push rsi
	push rdi
	push rbx
	sub rsp, 8+8
	; end prolog

	mov rax, arg(4)
	add rax, 3
	and rax, -4
	mov rcx, arg(5)
	sub rcx, rax
	mov rdx, rcx
	shr rdx, 1
	mov [rsp + 0], rdx ; [rsp + 0] = padUV
	add rcx, arg(5)
	shl rcx, 2
	mov [rsp + 8], rcx ; [rsp + 8] = padRGBA

	mov rax, arg(0) ; rgbaPtr
	mov rbx, arg(1) ; outUPtr
	mov rcx, arg(2) ; outVPtr
	mov rsi, arg(3) ; height

	movdqa xmm0, [sym(kRGBAToYUV_U2V2Coeffs8)] ; UV coeffs interleaved: each appear #2 times
	movdqa xmm1, [sym(k128_i16)]

	.LoopHeight:
		xor rdi, rdi
		.LoopWidth:
			%if %1 == 1
			movdqa xmm2, [rax] ; 4 RGBA samples = 16bytes (2 are useless, we want 1 out of 2): axbx
			%else
			movdqu xmm2, [rax] ; 4 RGBA samples = 16bytes (2 are useless, we want 1 out of 2): axbx
			%endif
			punpckldq xmm2, xmm2 ; aaxx
			punpckhdq xmm2, xmm2 ; bbxx
			punpckldq xmm2, xmm2 ; abab
			pmaddubsw xmm2, xmm0 ; Ua Ub Va Vb
			phaddw xmm2, xmm2
			psraw xmm2, 8 ; >> 8
			paddw xmm2, xmm1 ; + 128 -> UUVV----
			packuswb xmm2, xmm2 ; Saturate(I16 -> U8)

			movd rdx, xmm2
			mov [rbx], dx
			shr rdx, 16
			mov [rcx], dx
			
			add rax, 16 ; rgbaPtr += 16
			add rbx, 2 ; outUPtr += 2
			add rcx, 2 ; outVPtr += 2

			; end-of-LoopWidth
			add rdi, 4
			cmp rdi, arg(4)
			jl .LoopWidth
	add rax, [rsp + 8] ; rgbaPtr += padRGBA
	add rbx, [rsp + 0] ; outUPtr += padUV
	add rcx, [rsp + 0] ; outVPtr += padUV
	; end-of-LoopHeight
	sub rsi, 2
	cmp rsi, 0
	jg .LoopHeight

	; begin epilog
	add rsp, 8+8
	pop rbx
	pop rdi
	pop rsi
	COMPV_YASM_UNSHADOW_ARGS
	mov rsp, rbp
	pop rbp
	ret
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outUPtr
; arg(2) -> uint8_t* outVPtr
; arg(3) -> size_t height
; arg(4) -> size_t width
; arg(5) -> size_t stride
;;; void rgbaToI420Kernel11_CompUV_Asm_X86_Aligned0xx_SSSE3(const uint8_t* rgbaPtr, uint8_t* outUPtr, uint8_t* outVPtr, size_t height, size_t width, size_t stride)
sym(rgbaToI420Kernel11_CompUV_Asm_X86_Aligned0xx_SSSE3):
	rgbaToI420Kernel11_CompUV_Asm_X86_SSSE3 0

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> COMV_ALIGNED(16) const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outUPtr
; arg(2) -> uint8_t* outVPtr
; arg(3) -> size_t height
; arg(4) -> size_t width
; arg(5) -> size_t stride
;;; void rgbaToI420Kernel11_CompUV_Asm_X86_Aligned1xx_SSSE3(COMV_ALIGNED(16) const uint8_t* rgbaPtr, uint8_t* outUPtr, uint8_t* outVPtr, size_t height, size_t width, size_t stride)
sym(rgbaToI420Kernel11_CompUV_Asm_X86_Aligned1xx_SSSE3):
	rgbaToI420Kernel11_CompUV_Asm_X86_SSSE3 1


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outUPtr
; arg(2) -> uint8_t* outVPtr
; arg(3) -> size_t height
; arg(4) -> size_t width
; arg(5) -> size_t stride
; %1 -> 1: rgbaPtr is aligned, 0: rgbaPtr not aligned
%macro rgbaToI420Kernel41_CompUV_Asm_X86_SSSE3 1
	push rbp
	mov rbp, rsp
	COMPV_YASM_SHADOW_ARGS_TO_STACK 6
	COMPV_YASM_SAVE_XMM 7
	push rsi
	push rdi
	push rbx
	sub rsp, 8+8
	; end prolog

	mov rax, arg(4)
	add rax, 15
	and rax, -16
	mov rcx, arg(5)
	sub rcx, rax
	mov rdx, rcx
	shr rdx, 1
	mov [rsp + 0], rdx ; [rsp + 0] = padUV
	add rcx, arg(5)
	shl rcx, 2
	mov [rsp + 8], rcx ; [rsp + 8] = padRGBA

	mov rax, arg(0) ; rgbaPtr
	mov rbx, arg(1) ; outUPtr
	mov rcx, arg(2) ; outVPtr
	mov rsi, arg(3) ; height

	movdqa xmm0, [sym(kRGBAToYUV_UCoeffs8)] ; xmmUCoeffs
	movdqa xmm1, [sym(kRGBAToYUV_VCoeffs8)] ; xmmVCoeffs
	movdqa xmm7, [sym(k128_i16)] ; xmm128

	.LoopHeight:
		xor rdi, rdi
		.LoopWidth:
%if %1 == 1
			movdqa xmm2, [rax] ; 4 RGBA samples = 16bytes (2 are useless, we want 1 out of 2): 0x1x
			movdqa xmm3, [rax + 16] ; 4 RGBA samples = 16bytes :2x3x
			movdqa xmm4, [rax + 32] ; 4 RGBA samples = 16bytes : 4x5x
			movdqa xmm5, [rax + 48] ; 4 RGBA samples = 16bytes : 6x7x
%else
			movdqu xmm2, [rax] ; 4 RGBA samples = 16bytes (2 are useless, we want 1 out of 2): 0x1x
			movdqu xmm3, [rax + 16] ; 4 RGBA samples = 16bytes :2x3x
			movdqu xmm4, [rax + 32] ; 4 RGBA samples = 16bytes : 4x5x
			movdqu xmm5, [rax + 48] ; 4 RGBA samples = 16bytes : 6x7x
%endif

			movdqa xmm6, xmm2
			punpckldq xmm6, xmm3 ; 02xx
			punpckhdq xmm2, xmm3 ; 13xx
			punpckldq xmm2, xmm6 ; 0123
			movdqa xmm3, xmm2
			
			movdqa xmm6, xmm4
			punpckldq xmm6, xmm5 ; 46xx
			punpckhdq xmm4, xmm5 ; 57xx
			punpckldq xmm4, xmm6 ; 4567
			movdqa xmm5, xmm4

			; U = (xmm2, xmm4)
			; V = (xmm3, xmm5)

			pmaddubsw xmm2, xmm0
			pmaddubsw xmm4, xmm0
			pmaddubsw xmm3, xmm1
			pmaddubsw xmm5, xmm1

			; U = xmm2
			; V = xmm3

			phaddw xmm2, xmm4
			phaddw xmm3, xmm5

			psraw xmm2, 8 ; >> 8
			psraw xmm3, 8 ; >> 8

			paddw xmm2, xmm7 ; + 128 -> UUVV----
			paddw xmm3, xmm7 ; + 128 -> UUVV----

			; UV = xmm2

			packuswb xmm2, xmm3 ; Packs + Saturate(I16 -> U8)

			movq [rbx], xmm2
			psrldq xmm2, 8 ; >> 8
			movq [rcx], xmm2
			
			add rbx, 8 ; outUPtr += 8
			add rcx, 8 ; outVPtr += 8
			add rax, 64 ; rgbaPtr += 64

			; end-of-LoopWidth
			add rdi, 16
			cmp rdi, arg(4)
			jl .LoopWidth
	add rax, [rsp + 8] ; rgbaPtr += padRGBA
	add rbx, [rsp + 0] ; outUPtr += padUV
	add rcx, [rsp + 0] ; outVPtr += padUV
	; end-of-LoopHeight
	sub rsi, 2
	cmp rsi, 0
	jg .LoopHeight

	; begin epilog
	add rsp, 8+8
	pop rbx
	pop rdi
	pop rsi
	COMPV_YASM_RESTORE_XMM
	COMPV_YASM_UNSHADOW_ARGS
	mov rsp, rbp
	pop rbp
	ret
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outUPtr
; arg(2) -> uint8_t* outVPtr
; arg(3) -> size_t height
; arg(4) -> size_t width
; arg(5) -> size_t stride
;;; void rgbaToI420Kernel41_CompUV_Asm_X86_Aligned0xx_SSSE3(COMV_ALIGNED(16) const uint8_t* rgbaPtr, uint8_t* outUPtr, uint8_t* outVPtr, size_t height, size_t width, size_t stride)
sym(rgbaToI420Kernel41_CompUV_Asm_X86_Aligned0xx_SSSE3):
	rgbaToI420Kernel41_CompUV_Asm_X86_SSSE3 0

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arg(0) -> COMV_ALIGNED(16) const uint8_t* rgbaPtr
; arg(1) -> uint8_t* outUPtr
; arg(2) -> uint8_t* outVPtr
; arg(3) -> size_t height
; arg(4) -> size_t width
; arg(5) -> size_t stride
;;; void rgbaToI420Kernel41_CompUV_Asm_X86_Aligned1xx_SSSE3(COMV_ALIGNED(16) const uint8_t* rgbaPtr, uint8_t* outUPtr, uint8_t* outVPtr, size_t height, size_t width, size_t stride)
sym(rgbaToI420Kernel41_CompUV_Asm_X86_Aligned1xx_SSSE3):
	rgbaToI420Kernel41_CompUV_Asm_X86_SSSE3 1
