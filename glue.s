| SPDX-License-Identifier: MIT
| FM2OP custom machine registration and post-playback render hook.
        .section .run, "ax"
        .globl ds_inject_s
ds_inject_s:
        lea     -60(%sp), %sp
        movem.l %d0-%d7/%a0-%a6, (%sp)
        jsr     ds_inject
        movem.l (%sp), %d0-%d7/%a0-%a6
        lea     60(%sp), %sp
        lea     0x4199e444, %a4
        rts

        .equ BMP_VT, 0x401b73b4
        .balign 4
        .globl ds_machine
ds_machine:
        .long   7, ds_name, ds_short, ds_icon_bmp, 3, 7
ds_name: .asciz "FM2OP"
ds_short: .asciz "FM2"
        .balign 4
ds_icon_bmp:
        .long BMP_VT, 11, 7, 1, ds_icon_px, ds_icon_mask, 0
ds_icon_px:
        .long 0x10400000,0x28a00000,0x55400000,0xaa800000
        .long 0x55400000,0x28a00000,0x10400000,0x00000000
        .long 0x00000000,0x00000000,0x00000000
ds_icon_mask:
        .long 0xfe000000,0xfe000000,0xfe000000,0xfe000000
        .long 0xfe000000,0xfe000000,0xfe000000,0x00000000
        .long 0x00000000,0x00000000,0x00000000

| Dedicated FM2OP SRC layout and presentation.  All eight controls retain
| SLICE's persistent storage slots, preserving locks and external control.
        .equ DS_ID,7
        .equ LAY_SLICE,0x4197cf5c
        .equ P_TUNE,0x84
        .equ P_RATIO,0x85
        .equ P_INDEX,0x86
        .equ P_SAMP,0x87
        .equ P_ATTACK,0x88
        .equ P_DECAY,0x89
        .equ P_FEEDBACK,0x8a
        .equ P_TONE,0x8b
        .section .bss,"aw"
        .balign 4
        .globl ds_page_m,ds_d_turned
ds_page_m: .long 0
ds_d_turned: .long 0
ds_lay_ok: .long 0
ds_lay: .space 44
ds_txt: .space 12
        .section .run,"ax"
        .globl ds_layout
ds_layout:
        move.l 4(%sp),%d0
        move.l %d0,ds_page_m
        cmpi.l #DS_ID,%d0
        beq.s 1f
        moveq #3,%d1
        jmp 0x400657d2
1:      tst.l ds_lay_ok
        bne.s 3f
        lea LAY_SLICE,%a0
        lea ds_lay,%a1
        moveq #11,%d0
2:      move.l (%a0)+,(%a1)+
        subq.l #1,%d0
        bne.s 2b
        moveq #1,%d0
        move.l %d0,ds_lay_ok
3:      move.l #ds_lay,%d0
        rts

ds_pick:
        moveq #DS_ID,%d1
        cmp.l ds_page_m,%d1
        bne.s 8f
        move.l 12(%sp),%d0
        subi.l #P_TUNE,%d0
        cmpi.l #7,%d0
        bhi.s 8f
        lsl.l #2,%d0
        move.l 0(%a0,%d0.l),%d0
        rts
8:      moveq #0,%d0
        rts
        .globl ds_lab_short,ds_lab_long
ds_lab_short:
        lea ds_short_tab,%a0
        bsr.s ds_pick
        bne.s 9f
        move.l 8(%sp),%d1
        cmpi.l #164,%d1
        jmp 0x4000fe94
ds_lab_long:
        lea ds_long_tab,%a0
        bsr.s ds_pick
        bne.s 9f
        move.l 8(%sp),%d1
        cmpi.l #164,%d1
        jmp 0x4000feb6
9:      rts

| The LFO destination renderer reads the shared SLICE parameter descriptor
| directly, bypassing ds_lab_short.  Keep its target ID and drawing path,
| replacing only the displayed name while Sophie's layout is active.
        .globl ds_lfo_label
ds_lfo_label:
        moveq #DS_ID,%d1
        cmp.l ds_page_m,%d1
        bne.s 8f
        move.l %d0,%d1              | mapped destination parameter ID
        subi.l #P_TUNE,%d1
        cmpi.l #7,%d1
        bhi.s 8f
        lsl.l #2,%d1
        lea ds_short_tab,%a0
        move.l 0(%a0,%d1.l),%d1
        beq.s 8f                   | SAMP keeps its stock label
        lea 0x401a9d9c,%a0
        move.l %d1,(%sp)            | replace the stock name argument
        jmp 0x40060baa             | resume drawing at the next instruction
8:      lea 0x401a9d9c,%a0
        jmp 0x40060b94             | original descriptor lookup

| The destination popup formats rows separately as MACHINE:Parameter.
| Its first and fallback draws both need Sophie's full parameter names.
ds_chooser_name:
        move.l %d1,-(%sp)
        move.l %a0,-(%sp)
        move.l %d0,%d1              | descriptor byte offset
        subi.l #(P_TUNE*52),%d1
        cmpi.l #(7*52),%d1
        bhi.s 1f
        moveq #DS_ID,%d0
        cmp.l ds_page_m,%d0
        bne.s 1f
        divu #52,%d1
        andi.l #0xffff,%d1
        lsl.l #2,%d1
        lea ds_chooser_tab,%a0
        move.l 0(%a0,%d1.l),%d0
        move.l #ds_short,%d6
        bra.s 2f
1:      lea 0x401a9dc4,%a0          | descriptor table +40
        move.l 0(%a0,%d0.l),%d0
2:      move.l (%sp)+,%a0
        move.l (%sp)+,%d1
        rts
        .globl ds_lfo_popup_name,ds_lfo_popup_fallback
ds_lfo_popup_name:
        bsr ds_chooser_name
        move.l %d0,-(%sp)
        move.l %d6,-(%sp)
        jmp 0x400a437a
ds_lfo_popup_fallback:
        move.l %d2,%d0
        bsr ds_chooser_name
        move.l %d0,-(%sp)
        move.l %d6,-(%sp)
        jmp 0x400a43f6

| The LFO overview uses descriptor fields +44 and +48 for its two-line DEST.
        .globl ds_lfo_overview_group,ds_lfo_overview_name
ds_lfo_overview_group:
        move.l %d1,-(%sp)
        moveq #DS_ID,%d1
        cmp.l ds_page_m,%d1
        bne.s 1f
        cmpi.l #P_TUNE,%d3
        bcs.s 1f
        cmpi.l #P_TONE,%d3
        bhi.s 1f
        move.l #ds_short,%d0
        bra.s 2f
1:      move.l 44(%a0,%d0.l),%d0
2:      move.l (%sp)+,%d1
        move.l %d0,-(%sp)
        move.l %d2,-(%sp)
        jmp 0x40065dec
ds_lfo_overview_name:
        moveq #52,%d0
        muls.l %d0,%d3
        move.l %d1,-(%sp)
        move.l %a0,-(%sp)
        move.l %d3,%d1
        divu #52,%d1
        andi.l #0xffff,%d1
        moveq #DS_ID,%d0
        cmp.l ds_page_m,%d0
        bne.s 1f
        subi.l #P_TUNE,%d1
        cmpi.l #7,%d1
        bhi.s 1f
        lsl.l #2,%d1
        lea ds_overview_tab,%a0
        move.l 0(%a0,%d1.l),%d0
        bra.s 2f
1:      lea 0x401a9dcc,%a0          | descriptor table +48
        move.l 0(%a0,%d3.l),%d0
2:      move.l (%sp)+,%a0
        move.l (%sp)+,%d1
        move.l %d0,-(%sp)
        jmp 0x40065e68

ds_is_control:
        moveq #DS_ID,%d1
        cmp.l ds_page_m,%d1
        bne.s 8f
        cmpi.l #P_TUNE,%d0
        bcs.s 8f
        cmpi.l #P_TONE,%d0
        bhi.s 8f
        moveq #1,%d1
        rts
8:      moveq #0,%d1
        rts
        .equ NUM_BOX,0x400607fa      | SAMP's knob: (fn, value, canvas, x, y, flag)
        .globl ds_knob_gfx
ds_knob_gfx:
        move.l 8(%sp),%d0
        bsr.s ds_is_control
        beq.s 2f
        cmpi.l #P_TUNE,%d0
        beq.s 2f
        cmpi.l #P_RATIO,%d0
        bne.s 4f
        move.l 32(%sp),-(%sp)         | ds_algo_gfx(value, canvas, x, y)
        move.l 32(%sp),-(%sp)
        move.l 32(%sp),-(%sp)
        move.l 24(%sp),-(%sp)
        jsr ds_algo_gfx
        lea 16(%sp),%sp
        rts
4:      cmpi.l #P_SAMP,%d0
        bne.s 1f
        move.l 12(%sp),%d0            | OP: its number in SAMP's box
        lsr.l #8,%d0
        andi.l #0x7f,%d0
        lsr.l #3,%d0
        moveq #3,%d1
        cmp.l %d1,%d0
        bls.s 3f
        move.l %d1,%d0
3:      addq.l #1,%d0
        lsl.l #8,%d0
        move.l 16(%sp),-(%sp)         | flag, y, x, canvas: fresh slots,
        move.l 36(%sp),-(%sp)         | as NUM_BOX rewrites its own
        move.l 36(%sp),-(%sp)
        move.l 36(%sp),-(%sp)
        move.l %d0,-(%sp)
        clr.l -(%sp)
        jsr NUM_BOX
        lea 24(%sp),%sp
        rts
1:      move.l #P_INDEX,%d0
        move.l %d0,8(%sp)
2:      lea -20(%sp),%sp
        movem.l %d2-%d6,(%sp)
        jmp 0x4000f2c4
        .globl ds_ui_rec
ds_ui_rec:
        move.l 4(%sp),%d0
        bsr.w ds_is_control
        beq.s 1f
        cmpi.l #P_TUNE,%d0
        beq.s 1f
        move.l #P_INDEX,%d1
        bra.s 2f
1:      move.l 4(%sp),%d1
2:      cmpi.l #164,%d1
        jmp 0x4006579e

| Replaces SamplePageView's cmpi.l #135 (SAMP): Z=1 opens the sample list.
        .globl ds_samp_chk0,ds_samp_chk2
ds_samp_chk2:
        move.l %d2,%d0
ds_samp_chk0:
        cmpi.l #P_SAMP,%d0
        bne.s 9f
        moveq #DS_ID,%d1
        cmp.l ds_page_m,%d1
        bne.s 8f
        move.b %d1,ds_d_turned
        moveq #1,%d1
        rts
8:      moveq #0,%d1
9:      rts

| SERIALIZE(data, project, ...) and DESERIALIZE(project, data, cb), hooked at
| entry: the hidden operators ride in the storage block's unused header gap.
        .equ SERIALIZE,0x4007f30a
        .equ DESERIALIZE,0x4007efe8
        .globl ds_proj_save,ds_proj_load
ds_proj_save:
        move.l 20(%sp),-(%sp)
        move.l 20(%sp),-(%sp)
        move.l 20(%sp),-(%sp)
        move.l 20(%sp),-(%sp)
        move.l 20(%sp),-(%sp)
        bsr.s 1f
        lea 20(%sp),%sp
        move.l %d0,-(%sp)             | garbage when no progress callback
        tst.l 8(%sp)                  | data
        beq.s 2f
        tst.l 12(%sp)                 | project
        beq.s 2f
        move.l 8(%sp),-(%sp)
        jsr ds_proj_put
        addq.l #4,%sp
2:      move.l (%sp)+,%d0
        rts
1:      lea -44(%sp),%sp              | the replaced entry
        movem.l %d2-%d7/%a2-%a6,(%sp)
        jmp SERIALIZE+8
ds_proj_load:
        move.l 12(%sp),-(%sp)
        move.l 12(%sp),-(%sp)
        move.l 12(%sp),-(%sp)
        bsr.s 1f
        lea 12(%sp),%sp
        move.l %d0,-(%sp)
        tst.b %d0
        beq.s 2f
        move.l 12(%sp),-(%sp)         | data
        jsr ds_proj_get
        addq.l #4,%sp
2:      move.l (%sp)+,%d0
        rts
1:      lea -36(%sp),%sp              | the replaced entry
        movem.l %d2-%d5/%a2-%a6,(%sp)
        jmp DESERIALIZE+8

| Range lookup is reached by display, stepper, setter and validator.
        .globl ds_prange,ds_prange_f
ds_prange:
        movea.l %a2,%a1
        bra.s 1f
ds_prange_f:
        movea.l 36(%sp),%a1
1:      move.l %a1,-(%sp)
        move.l %a0,-(%sp)
        move.l 12(%sp),-(%sp)
        jsr 0x40078f0c
        addq.l #4,%sp
        movea.l (%sp)+,%a0
        movea.l (%sp)+,%a1
        move.l 4(%sp),%d1
        cmpi.l #P_RATIO,%d1
        bcs.s 9f
        cmpi.l #P_TONE,%d1
        bhi.s 9f
        move.l (%a1),%d0
        cmpi.l #0x4017eb58,%d0
        bne.s 9f
        movea.l 16(%a1),%a1
        move.l (%a1),%d0
        cmpi.l #0x40181330,%d0
        bne.s 9f
        movea.l 16(%a1),%a1
        moveq #0,%d0
        move.b 126(%a1),%d0
        cmpi.l #DS_ID,%d0
        bne.s 9f
        subi.l #P_RATIO,%d1
        lsl.l #3,%d1
        lea ds_range_tab,%a1
        clr.l (%a0)
        move.l 0(%a1,%d1.l),%d0
        move.l %d0,4(%a0)
        move.l 4(%a1,%d1.l),%d0
        move.l %d0,8(%a0)
9:      move.l %a0,%d0
        rts

        .globl ds_val_text,ds_pop_text
ds_val_text:
        move.l 8(%sp),%d0
        cmpi.l #P_RATIO,%d0
        beq.s 2f
        cmpi.l #P_SAMP,%d0
        beq.s 6f
        cmpi.l #P_INDEX,%d0
        beq.s 7f
        cmpi.l #P_DECAY,%d0
        beq.s 5f
        cmpi.l #P_FEEDBACK,%d0
        beq.s 4f
        cmpi.l #P_ATTACK,%d0
        beq.s 5f
        cmpi.l #P_TONE,%d0
        bne.s 1f
5:      move.l #ds_fmt_u7,%a0
        bra.s 3f
4:
        move.l #ds_fmt_decay,%a0
        bra.s 3f
6:
        move.l #ds_fmt_op,%a0
        bra.s 3f
7:      move.l #ds_fmt_ratio,%a0
        bra.s 3f
2:      move.l #ds_fmt_algo,%a0
3:
        moveq #DS_ID,%d1
        cmp.l ds_page_m,%d1
        bne.s 1f
        move.l 12(%sp),-(%sp)
        move.l 20(%sp),-(%sp)
        jsr (%a0)
        addq.l #8,%sp
        rts
1:      lea -20(%sp),%sp
        movem.l %d2-%d4/%a2-%a3,(%sp)
        jmp 0x4000f32c
ds_pop_text:
        move.l 4(%sp),%d0
        cmpi.l #P_RATIO,%d0
        beq.s 2f
        cmpi.l #P_SAMP,%d0
        beq.s 6f
        cmpi.l #P_INDEX,%d0
        beq.s 7f
        cmpi.l #P_DECAY,%d0
        beq.s 5f
        cmpi.l #P_FEEDBACK,%d0
        beq.s 4f
        cmpi.l #P_ATTACK,%d0
        beq.s 5f
        cmpi.l #P_TONE,%d0
        bne.s 1f
5:      move.l #ds_fmt_u7,%a0
        bra.s 3f
4:
        move.l #ds_fmt_decay,%a0
        bra.s 3f
6:
        move.l #ds_fmt_op,%a0
        bra.s 3f
7:      move.l #ds_fmt_ratio,%a0
        bra.s 3f
2:      move.l #ds_fmt_algo,%a0
3:
        moveq #DS_ID,%d1
        cmp.l ds_page_m,%d1
        bne.s 1f
        move.l 8(%sp),-(%sp)
        pea ds_txt
        jsr (%a0)
        addq.l #8,%sp
        rts
1:      move.l 4(%sp),%d1
        cmpi.l #164,%d1
        jmp 0x400657f8
        .balign 4
ds_short_tab: .long ds_s_tune,ds_s_algo,ds_s_ratio,ds_s_op,ds_s_level,ds_s_attack,ds_s_decay,ds_s_char
ds_long_tab: .long ds_l_tune,ds_l_algo,ds_l_ratio,ds_l_op,ds_l_level,ds_l_attack,ds_l_decay,ds_l_char
ds_chooser_tab: .long ds_l_tune,ds_l_algo,ds_l_ratio,ds_l_op,ds_l_level,ds_l_attack,ds_l_decay,ds_l_char
ds_overview_tab: .long ds_s_tune,ds_s_algo,ds_s_ratio,ds_s_op,ds_s_level,ds_s_attack,ds_s_decay,ds_s_char
ds_range_tab: .long 0x7f00,0x0000,0x7f00,0x1000,0x1f00,0x0000,0x7f00,0x7f00,0x7f00,0x0000,0x7f00,0x7f00,0x7f00,0x4000
ds_s_tune: .asciz "TUNE"
ds_s_algo: .asciz "ALGO"
ds_s_ratio: .asciz "RATIO"
ds_s_op: .asciz "OP"
ds_s_level: .asciz "LEVEL"
ds_s_attack: .asciz "ATTK"
ds_s_decay: .asciz "DECAY"
ds_s_char: .asciz "CHAR"
ds_l_tune: .asciz "Tune"
ds_l_algo: .asciz "Algorithm"
ds_l_ratio: .asciz "Ratio"
ds_l_op: .asciz "Operator"
ds_l_level: .asciz "Level"
ds_l_attack: .asciz "Attack"
ds_l_decay: .asciz "Decay"
ds_l_char: .asciz "Character"
