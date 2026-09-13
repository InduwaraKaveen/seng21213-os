[BITS 32]

global context_switch

context_switch:
    pushad

    mov eax, [esp + 36]
    mov [eax], esp

    mov esp, [esp + 40]

    popad
    ret
