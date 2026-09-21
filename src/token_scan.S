.text
.global scan_token_end
.type scan_token_end, @function

scan_token_end:
    xor %rax, %rax

scan_loop:
    cmp %rsi, %rax
    jae scan_done

    movzbl (%rdi,%rax), %r8d

    cmp $32, %r8d
    je scan_done
    cmp $9, %r8d
    je scan_done
    cmp $124, %r8d
    je scan_done
    cmp $60, %r8d
    je scan_done
    cmp $62, %r8d
    je scan_done
    cmp $38, %r8d
    je scan_done

    inc %rax
    jmp scan_loop

scan_done:
    ret
