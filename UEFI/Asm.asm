
_text segment

ASM_CPUID PROC

    sub rsp, 8
    mov qword ptr [rsp],rcx
    xor         eax,eax  
    xor         ecx,ecx  
    cpuid
    mov rdi,qword ptr[rsp]
    mov dword ptr [rdi],ebx
    mov dword ptr [rdi+4],edx  
    mov dword ptr [rdi+8],ecx  
    add rsp,8
    ret
ASM_CPUID ENDP

_text ends

end