| SPDX-License-Identifier: MIT
| Digitakt mk1 MAIN OS 1.54: the stock code and data glue.s names (os153.inc
| and os154.inc, one per OS; glue.s includes the one OS154 picks).
        .equ INJECT_A4,         0x4199f444    | what the inject site's lea loads into a4
        .equ BMP_VT,            0x401b7734    | the firmware's Bitmap vtable
        .equ LAY_SLICE,         0x4197df5c    | SLICE's SRC page layout
        .equ DESC_TAB,          0x401aa09c    | the parameter descriptor table (52-byte rows)
        .equ PARAM_VT,          0x4017ee58    | the SRC page's parameter object's vtable
        .equ SNDREF_VT,         0x40181630    | the vtable of the object at its +16
        .equ PRANGE_FN,         0x40078f0c    | the parameter range lookup
        .equ NUM_BOX,           0x400607fa    | SAMP's knob: (fn, value, canvas, x, y, flag)
        .equ SERIALIZE,         0x4007f30a
        .equ DESERIALIZE,       0x4007efe8
        .equ KIT_SAVE,          0x4007a6a6
        .equ KIT_LOAD,          0x4007a4ce
        .equ LAYOUT_ON,         0x400657d2    | the replaced code's continuations
        .equ LAB_SHORT_ON,      0x4000fe94
        .equ LAB_LONG_ON,       0x4000feb6
        .equ LFO_LABEL_ORIG,    0x40060b94
        .equ LFO_LABEL_ON,      0x40060baa
        .equ POPUP_NAME_ON,     0x400a44d6
        .equ POPUP_FALLBACK_ON, 0x400a4552
        .equ OVERVIEW_GROUP_ON, 0x40065dec
        .equ OVERVIEW_NAME_ON,  0x40065e68
        .equ KNOB_GFX_ON,       0x4000f2c4
        .equ UI_REC_ON,         0x4006579e
        .equ VAL_TEXT_ON,       0x4000f32c
        .equ POP_TEXT_ON,       0x400657f8
