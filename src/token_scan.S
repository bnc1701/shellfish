.text
.global scan_token_end
.type scan_token_end, @function

scan_token_end:
    xor %rax, %rax

scan_loop:
    cmp %rsi, %rax
    jae scan_done

    movzb (%rdi, %rax), %r8b

    cmp $32, %r8b
    je scan_done
    cmp $9, %r8b
    je scan_done
    cmp $124, %r8b
    je scan_done
    cmp $60, %r8b
    je scan_done
    cmp $62, %r8b
    je scan_done
    cmp $38, %r8b
    je scan_done

    inc %rax
    jmp scan_loop

scan_done:
    ret
