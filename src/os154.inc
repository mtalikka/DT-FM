| SPDX-License-Identifier: MIT
| Digitakt mk1 MAIN OS 1.54: the stock code glue.s hooks (os153.inc and
| os154.inc, one per OS; glue.s includes the one OS154 picks).
        .equ SERIALIZE,         0x4007f30a
        .equ DESERIALIZE,       0x4007efe8
        .equ KIT_SAVE,          0x4007a6a6
        .equ KIT_LOAD,          0x4007a4ce
