;////////////////////////////////////////////////////////////////////////////////////////////////////////
;// crittercam - camera tools for ELDEN RING NIGHTREIGN
;// Built on Injectable Generic Camera System, Copyright(c) 2020, Frans Bouma (BSD-2, see LICENSE-IGCS.txt)
;// https://github.com/FransBouma/InjectableGenericCameraSystem
;////////////////////////////////////////////////////////////////////////////////////////////////////////
;---------------------------------------------------------------
; Game specific asm file to intercept execution flow to obtain addresses, prevent writes etc.
;---------------------------------------------------------------

;---------------------------------------------------------------
; Public definitions so the linker knows which names are present in this file
PUBLIC cameraStructInterceptor
PUBLIC hudOpacityReadInterceptor
;---------------------------------------------------------------

;---------------------------------------------------------------
; Externs which are used and set by the system. Read / write these
; values in asm to communicate with the system
EXTERN g_cameraEnabled: byte
EXTERN g_hudHidden: byte
EXTERN g_cameraStructAddress: qword
EXTERN g_activeCameraStructAddress: qword
EXTERN g_cameraOverrideBuffer: qword
EXTERN g_cameraCandidates: qword
;---------------------------------------------------------------

;---------------------------------------------------------------
; Own externs, defined in InterceptorHelper.cpp
EXTERN _cameraStructInterceptionContinue: qword
;---------------------------------------------------------------

MAX_CAMERA_CANDIDATES = 8			; must match GameConstants.h

.data

.code

cameraStructInterceptor PROC
; The game's camera-select routine. It picks one of several source cameras (based on flag bits) and copies it into the
; render camera. RCX = source camera, [R9] = destination (render) camera. Layout: matrix +10h..+4Fh, fov +50h, aspect +54h,
; near +58h, far +5Ch. Nightreign's protection layer split this function into chunks joined by jmps, so we hook the
; chunk itself, before its jmp.
;
;nightreign.exe+1A40C27 - 8B 41 50              - mov eax,[rcx+50]			<< INTERCEPT HERE
;nightreign.exe+1A40C2A - 49 8B 11              - mov rdx,[r9]
;nightreign.exe+1A40C2D - 89 42 50              - mov [rdx+50],eax
;nightreign.exe+1A40C30 - 8B 41 54              - mov eax,[rcx+54]
;nightreign.exe+1A40C33 - 89 42 54              - mov [rdx+54],eax
;nightreign.exe+1A40C36 - 8B 41 58              - mov eax,[rcx+58]			<< CONTINUE HERE
;nightreign.exe+1A40C39 - E9 1D03B205           - jmp nightreign.exe+7560F5B
;  ...the jmp target copies [rcx+58h], [rcx+5Ch] and the 4 matrix rows from rcx to rdx, then:
;nightreign.exe+7560F84 - 41 83 61 20 C0        - and dword ptr [r9+20],-40
;nightreign.exe+7560F89 - C3                    - ret							<< rcx isn't used after the copy, so we can swap it.
;
; When our camera is enabled and the destination is the camera we locked onto, we keep the game's aspect/near/far in our
; buffer and then point RCX at our buffer, so the rest of the routine copies OUR camera into the render camera.
	mov rdx, qword ptr [r9]
	cmp byte ptr [g_cameraEnabled], 1
	jne recordCandidate
	cmp rdx, qword ptr [g_activeCameraStructAddress]
	jne originalCode
	mov r10, qword ptr [g_cameraOverrideBuffer]
	mov eax, dword ptr [rcx+54h]			; aspect ratio, keep it live (window resizes)
	mov dword ptr [r10+54h], eax
	mov eax, dword ptr [rcx+58h]			; near plane
	mov dword ptr [r10+58h], eax
	mov eax, dword ptr [rcx+5Ch]			; far plane
	mov dword ptr [r10+5Ch], eax
	mov rcx, r10							; copy from our camera from here on.
	jmp originalCode
recordCandidate:
	; camera disabled: remember the last destination and collect the distinct destinations we've seen.
	mov qword ptr [g_cameraStructAddress], rdx
	lea r10, g_cameraCandidates
	xor r11d, r11d
candidateLoop:
	mov rax, qword ptr [r10+r11*8]
	cmp rax, rdx
	je originalCode							; already known
	test rax, rax
	jz storeCandidate						; free slot
	inc r11d
	cmp r11d, MAX_CAMERA_CANDIDATES
	jb candidateLoop
	jmp originalCode						; table full
storeCandidate:
	mov qword ptr [r10+r11*8], rdx
originalCode:
	mov eax, dword ptr [rcx+50h]
	mov dword ptr [rdx+50h], eax
	mov eax, dword ptr [rcx+54h]
	mov dword ptr [rdx+54h], eax
	jmp qword ptr [_cameraStructInterceptionContinue]	; jmp back into the original game code, which is the location after the original statements above.
cameraStructInterceptor ENDP


hudOpacityReadInterceptor PROC
; EXPERIMENTAL. Otis_Inf's Elden Ring tools hook this function for the HUD toggle. It's a small leaf function returning a
; 32 byte value. We replace the whole function. When the HUD is hidden we return zeros. If hiding the HUD breaks something
; else in the game, toggle it back on: this function might be shared with other code.
;nightreign.exe+12C5C60 - 41 0F28 00            - movaps xmm0,[r8]			<< INTERCEPT HERE
;nightreign.exe+12C5C64 - 48 8B C2              - mov rax,rdx
;nightreign.exe+12C5C67 - 41 0F28 48 10         - movaps xmm1,[r8+10]
;nightreign.exe+12C5C6C - 0F29 02               - movaps [rdx],xmm0
;nightreign.exe+12C5C6F - 0F29 4A 10            - movaps [rdx+10],xmm1
;nightreign.exe+12C5C73 - C3                    - ret
	movaps xmm0, xmmword ptr [r8]
	mov rax, rdx
	movaps xmm1, xmmword ptr [r8+10h]
	cmp byte ptr [g_hudHidden], 1
	jne writeResult
	xorps xmm0, xmm0
	xorps xmm1, xmm1
writeResult:
	movaps xmmword ptr [rdx], xmm0
	movaps xmmword ptr [rdx+10h], xmm1
	ret
hudOpacityReadInterceptor ENDP

END
