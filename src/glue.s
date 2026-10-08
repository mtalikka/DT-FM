| SPDX-License-Identifier: MIT
| DT-FM's own firmware hooks: the hidden operators' storage. Everything else
| about the machine (its page, its render) goes through core 3.0 and
| machine-pages, from digitakt.c.
        .ifdef  OS154                   | the Digitakt mk1 1.54 (mod.json's port)
        .include "os154.inc"
        .else                           | the Digitakt mk1 1.53
        .include "os153.inc"
        .endif
        .section .run, "ax"

| SERIALIZE(data, project, a, flags, cb) and DESERIALIZE(project, data, cb),
| hooked at entry, and KIT_SAVE(rec, kit, flags) and KIT_LOAD(kit, rec):
| the hidden operators ride in the stored kits' unused tails.
        .globl ds_proj_save,ds_proj_load,ds_kit_save,ds_kit_load
ds_proj_save:
        move.l 20(%sp),-(%sp)
        move.l 20(%sp),-(%sp)
        move.l 20(%sp),-(%sp)
        move.l 20(%sp),-(%sp)
        move.l 20(%sp),-(%sp)
        bsr.s 1f
        lea 20(%sp),%sp
        move.l %d0,-(%sp)             | garbage when no progress callback
        move.l 20(%sp),-(%sp)         | ds_proj_put(data, project, flags)
        move.l 16(%sp),-(%sp)
        move.l 16(%sp),-(%sp)
        jsr ds_proj_put
        lea 12(%sp),%sp
        move.l (%sp)+,%d0
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
        move.l 12(%sp),-(%sp)         | ds_proj_get(project, data)
        move.l 12(%sp),-(%sp)
        jsr ds_proj_get
        addq.l #8,%sp
2:      move.l (%sp)+,%d0
        rts
1:      lea -36(%sp),%sp              | the replaced entry
        movem.l %d2-%d5/%a2-%a6,(%sp)
        jmp DESERIALIZE+8
ds_kit_load:
        move.l 8(%sp),-(%sp)
        move.l 8(%sp),-(%sp)
        bsr.s 1f
        addq.l #8,%sp
        move.l %d0,-(%sp)
        tst.b %d0
        beq.s 2f
        move.l 12(%sp),-(%sp)         | ds_kit_get(kit, rec)
        move.l 12(%sp),-(%sp)
        jsr ds_kit_get
        addq.l #8,%sp
2:      move.l (%sp)+,%d0
        rts
1:      move.l 4(%sp),%d0             | the replaced entry; its beq tests d0
        movea.l 8(%sp),%a0
        jmp KIT_LOAD+8
ds_kit_save:
        move.l 12(%sp),-(%sp)
        move.l 12(%sp),-(%sp)
        move.l 12(%sp),-(%sp)
        bsr.s 1f
        lea 12(%sp),%sp
        move.l %d0,-(%sp)
        tst.b %d0
        beq.s 2f
        move.l 12(%sp),-(%sp)         | ds_kit_put(rec, kit)
        move.l 12(%sp),-(%sp)
        jsr ds_kit_put
        addq.l #8,%sp
2:      move.l (%sp)+,%d0
        rts
1:      lea -28(%sp),%sp              | the replaced entry
        movem.l %d2-%d4/%a2-%a5,(%sp)
        jmp KIT_SAVE+8

