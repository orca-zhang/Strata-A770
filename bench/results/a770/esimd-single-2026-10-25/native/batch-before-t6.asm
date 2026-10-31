L0:
(W)     mov (8|M0)               r255.0<1>:ud  0x0:ud                              {A@1}
(W)     and (1|M0)               r255.2<1>:ud  r0.0<0;1,0>:ud    0xFFFFFFC0:ud             
(W)     and (1|M0)               r255.0<1>:uw  r0.4<0;1,0>:uw    0xFF:uw             
(W)     add (1|M0)               r255.2<1>:ud  r255.2<0;1,0>:ud  0x60:ud              {I@2}
(W)     mad (1|M0)               r255.2<1>:ud  r255.2<0;0>:ud    r255.0<0;0>:uw    0x20:uw              {I@1}
(W)     send.dc0 (8|M0)          r1       r255    null:0  0x0            0x021842FD           {A@1,$0} // wr:1h+0, rd:1; oword aligned block read x2
        nop                    
        nop                    
(W)     mov (8|M0)               r255.0<1>:ud  0x0:ud                              {$0.src}
(W)     and (1|M0)               r255.2<1>:ud  r0.0<0;1,0>:ud    0xFFFFFFC0:ud             
(W)     send.dc0 (8|M0)          r2       r255    null:0  0x0            0x022843FD           {A@1,$1} // wr:1h+0, rd:2; oword aligned block read x4
(W)     add (1|M0)               r255.2<1>:ud  r255.2<0;1,0>:ud  0x40:uw              {$1.src}
(W)     send.dc0 (8|M0)          r4       r255    null:0  0x0            0x021842FD           {A@1,$2} // wr:1h+0, rd:1; oword aligned block read x2
        and (1|M0)               cr0.0<1>:ud   cr0.0<0;1,0>:ud   -1265:w               {A@1}
        or (1|M0)                cr0.0<1>:ud   cr0.0<0;1,0>:ud   1216:w               {A@1}
        sync.nop                             null                             {Compacted,A@1}
        sync.nop                             null                             {Compacted,$1.dst}
(W)     mul (1|M0)               acc0.0<1>:d   r0.1<0;1,0>:d     r2.0<0;1,0>:uw   {Compacted,A@1}
        send.ugm (1|M0)          r46      r4      null:0  0x0            0x04100580           {$2} // wr:2+0, rd:1; load.ugm.d32.a64
        mov (1|M0)               r43.0<1>:d    r1.0<0;1,0>:uw                   {$0.dst}
        mach (1|M0)              r5.0<1>:d     r0.1<0;1,0>:d     r2.0<0;1,0>:d    {Compacted,$2.src}
        mov (1|M0)               r43.4<1>:d    0:w                              
        add (1|M0)               r5.0<1>:d     r5.0<0;1,0>:d     r1.0<0;1,0>:uw   {I@2}
(W)     mul (1|M0)               acc0.0<1>:ud  r5.0<0;1,0>:ud    0xAAAB:uw              {I@1}
        mach (1|M0)              r6.0<1>:ud    r5.0<0;1,0>:ud    0xAAAAAAAB:ud              {AccWrEn}
        shr (1|M0)               r6.0<1>:ud    r6.0<0;1,0>:ud    0x9:uw              {I@1}
        mul (1|M0)               acc0.0<1>:ud  r6.0<0;1,0>:ud    0x7800:uw              {I@1}
        mul (1|M0)               r43.1<1>:d    r6.0<0;1,0>:d     -768:w               {Compacted}
        mach (1|M0)              r8.0<1>:ud    r6.0<0;1,0>:ud    0xC7800:ud              {AccWrEn}
        add (1|M0)               r43.1<1>:d    r43.1<0;1,0>:d    r5.0<0;1,0>:d    {Compacted,I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r8.0<0;1,0>:d     2:w               {Compacted,I@3}
        shl (1|M0)               r1.2<1>:ud    r43.1<0;1,0>:ud   0x3:uw              {I@3}
        shr (1|M0)               r43.2<1>:ud   r43.1<0;1,0>:ud   0x4:uw             
        shl (1|M0)               r4.2<1>:d     r7.0<0;1,0>:d     2:w               {Compacted,I@4}
        shr (1|M0)               r2.4<1>:ud    r7.0<0;1,0>:ud    0x1E:uw             
        and (1|M0)               r43.3<1>:ud   r1.2<0;1,0>:ud    0x60:uw              {I@4}
        addc (1|M0)              r5.0<1>:ud    r2.6<0;1,0>:ud    r4.2<0;1,0>:ud   {AccWrEn,I@3}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@3}
        mov (1|M0)               r43.5<1>:d    r5.0<0;1,0>:d                    {I@2}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r10.0<1>:ud   r43.5<0;1,0>:ud   0x300000:ud              {AccWrEn,I@2}
        add3 (1|M0)              r43.6<1>:d    r9.0<0;0>:d       r2.7<0;0>:d       r4.3<0>:d        {I@2}
        mov (1|M0)               r11.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r6.0<0;1,0>:ud    0xF000:uw             
        mov (1|M0)               r44.0<1>:f    r10.0<0;1,0>:f                   {Compacted,I@4}
        mach (1|M0)              r13.0<1>:ud   r6.0<0;1,0>:ud    0xF000:ud              {AccWrEn}
        add3 (1|M0)              r44.1<1>:d    r11.0<0;0>:d      r43.6<0;0>:d      0:w               {I@3}
        mov (1|M0)               r12.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r1.2<1>:d     r13.0<0;1,0>:d    2:w               {Compacted,I@3}
        shl (1|M0)               r1.4<1>:d     r12.0<0;1,0>:d    2:w               {Compacted,I@2}
        shr (1|M0)               r1.3<1>:ud    r12.0<0;1,0>:ud   0x1E:uw             
        addc (1|M0)              r5.0<1>:ud    r3.0<0;1,0>:ud    r1.4<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r1.5<1>:d     r1.2<0;1,0>:d     r1.3<0;1,0>:d    {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r8.0<1>:ud    r3.2<0;1,0>:ud    r1.4<0;1,0>:ud   {AccWrEn}
        mov (1|M0)               r44.2<1>:d    r5.0<0;1,0>:d                    {I@4}
        add3 (1|M0)              r44.3<1>:d    r7.0<0;0>:d       r3.1<0;0>:d       r1.5<0>:d        {I@3}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r6.0<0;1,0>:ud    0x120:uw             
        mov (1|M0)               r44.4<1>:d    r8.0<0;1,0>:d                    {I@5}
        mach (1|M0)              r11.0<1>:ud   r6.0<0;1,0>:ud    0x120:ud              {AccWrEn}
        add3 (1|M0)              r44.5<1>:d    r9.0<0;0>:d       r3.3<0;0>:d       r1.5<0>:d        {I@4}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted,F@1}
        shl (1|M0)               r2.3<1>:d     r11.0<0;1,0>:d    2:w               {Compacted,I@3}
        cmp (1|M0)    (lt)f0.0   null<1>:d     r46.0<0;1,0>:d    1:w               {$2.dst}
        shl (1|M0)               r4.2<1>:d     r10.0<0;1,0>:d    2:w               {Compacted,I@3}
        shr (1|M0)               r2.4<1>:ud    r10.0<0;1,0>:ud   0x1E:uw             
        addc (1|M0)              r5.0<1>:ud    r3.4<0;1,0>:ud    r4.2<0;1,0>:ud   {AccWrEn,I@2}
(W)     and (1|M0)               f0.0<1>:uw    f0.0<0;1,0>:uw    0x1:uw             
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@3}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r8.0<1>:ud    r3.6<0;1,0>:ud    r4.2<0;1,0>:ud   {AccWrEn}
        mov (1|M0)               r44.6<1>:d    r5.0<0;1,0>:d                    {I@5}
        add3 (1|M0)              r44.7<1>:d    r7.0<0;0>:d       r3.5<0;0>:d       r4.3<0>:d        {I@3}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r45.0<1>:f    r8.0<0;1,0>:f                    {Compacted,I@4}
        add3 (1|M0)              r45.1<1>:d    r9.0<0;0>:d       r3.7<0;0>:d       r4.3<0>:d        {I@1}
(f0.0.any16h) goto (16|M0)                   L10640                  L10640                
L1080:
        cmp (1|M0)    (lt)f1.1   null<1>:ud    r43.1<0;1,0>:ud   0x280:uw             
        shl (1|M0)               r43.7<1>:ud   r43.1<0;1,0>:ud   0x4:uw             
(W)     and (1|M0)               f1.1<1>:uw    f1.1<0;1,0>:uw    0x1:uw             
(~f1.1.any16h) goto (16|M0)                  L3904                  L3904                
L1144:
        shl (1|M0)               r1.3<1>:d     r43.7<0;1,0>:d    2:w               {I@3}
        cmp (1|M0)    (lt)f1.0   null<1>:ud    r46.0<0;1,0>:ud   0x3:uw             
        shr (1|M0)               r1.2<1>:ud    r43.7<0;1,0>:ud   0x1E:uw             
        mov (1|M0)               r45.2<1>:d    r46.0<0;1,0>:d                  
        mov (1|M0)               r45.3<1>:d    0:w                              
        addc (1|M0)              r5.0<1>:ud    r44.4<0;1,0>:ud   r1.3<0;1,0>:ud   {AccWrEn,I@5}
        or (1|M0)                r1.4<1>:d     r1.2<0;1,0>:d     0:w               {I@4}
(W)     and (1|M0)               f1.0<1>:uw    f1.0<0;1,0>:uw    0x1:uw             
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r43.7<0;1,0>:ud   0x3:uw             
        mov (1|M0)               r45.4<1>:d    r5.0<0;1,0>:d                    {I@5}
        mach (1|M0)              r8.0<1>:ud    r43.7<0;1,0>:ud   0x3:ud              {AccWrEn,F@1}
        add3 (1|M0)              r45.5<1>:d    r6.0<0;0>:d       r44.5<0;0>:d      r1.4<0>:d        {I@4}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r8.0<0;1,0>:d     2:w               {Compacted,I@3}
        shl (1|M0)               r4.2<1>:d     r7.0<0;1,0>:d     2:w               {Compacted,I@2}
        shr (1|M0)               r2.4<1>:ud    r7.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r9.0<1>:ud    r44.0<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r47.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@3}
        add3 (1|M0)              r47.1<1>:d    r10.0<0;0>:d      r44.1<0;0>:d      r4.3<0>:d        {I@1}
(f1.0.any16h) goto (16|M0)                   L1648                  L1648                
L1464:
        add (1|M0)               r1.2<1>:ud    r46.0<0;1,0>:ud   0xFFFFFFFD:ud             
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x2800:uw              {I@1}
        mach (1|M0)              r6.0<1>:ud    r1.2<0;1,0>:ud    0x2800:ud              {AccWrEn}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r6.0<0;1,0>:d     2:w               {Compacted,I@2}
        shl (1|M0)               r4.2<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@2}
        shr (1|M0)               r2.4<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r7.0<1>:ud    r45.4<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r9.0<1>:f     r7.0<0;1,0>:f                    {Compacted,I@3}
        add3 (1|M0)              r9.1<1>:d     r8.0<0;0>:d       r45.5<0;0>:d      r4.3<0>:d        {I@1}
        send.ugm (1|M0)          r52      r9      null:0  0x0            0x0220D580           {A@1,$3} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        goto (16|M0)                         L1648                  L2736                
L1648:
        join (16|M0)                         L2736                                
L1664:
        mov (4|M0)               r4.8<1>:w     0x32103210:v                              
        shl (1|M0)               r1.4<1>:d     r45.2<0;1,0>:d    2:w              
        shl (1|M0)               r1.2<1>:d     r45.3<0;1,0>:d    2:w              
        shr (1|M0)               r1.3<1>:ud    r45.2<0;1,0>:ud   0x1E:uw             
        cmp (1|M0)    (eq)f0.1   null<1>:d     r46.0<0;1,0>:d    2:w              
        mul (4|M0)               r7.0<1>:d     r4.8<1;1,0>:w     12:w               {I@5}
        mul (4|M0)               r8.0<2>:d     r4.8<1;1,0>:w     12:w              
        addc (1|M0)              r5.0<1>:ud    r47.0<0;1,0>:ud   r1.4<0;1,0>:ud   {AccWrEn,I@6}
        or (1|M0)                r1.5<1>:d     r1.2<0;1,0>:d     r1.3<0;1,0>:d    {I@5}
        asr (4|M0)               r8.1<2>:d     r7.0<1;1,0>:d     31:w               {I@4}
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r2.4<1>:d     r5.0<0;1,0>:d                    {I@4}
(W)     and (1|M0)               f0.1<1>:uw    f0.1<0;1,0>:uw    0x1:uw             
        sync.nop                             null                             {Compacted,$3.src}
        addc (4|M0)              r9.0<1>:ud    r8.0<2;1,0>:ud    0x30:ud              {AccWrEn,I@4}
        mov (4|M0)               r48.0<2>:ud   r8.0<2;1,0>:ud                  
        add3 (1|M0)              r2.5<1>:d     r6.0<0;0>:d       r47.1<0;0>:d      r1.5<0>:d        {I@5}
        mov (8|M0)               r12.0<2>:ud   r2.4<0;1,0>:ud                   {I@5}
        mov (4|M0)               r48.1<2>:ud   r8.1<2;1,0>:ud                  
        mov (4|M0)               r10.0<1>:ud   acc0.0<1;1,0>:ud                
        addc (4|M0)              r5.0<1>:ud    r8.0<2;1,0>:ud    0x60:ud              {AccWrEn}
        mov (4|M0)               r49.0<2>:d    r9.0<1;1,0>:d                    {I@7}
        mov (8|M0)               r12.1<2>:ud   r2.5<0;1,0>:ud                   {I@6}
        add3 (4|M0)              r49.1<2>:d    r10.0<1;0>:d      r8.1<4;2>:d       0:w               {I@4}
        mov (4|M0)               r6.0<1>:ud    acc0.0<1;1,0>:ud                
        addc (4|M0)              r11.0<1>:ud   r8.0<2;1,0>:ud    0x90:ud              {AccWrEn}
        mov (4|M0)               r50.0<2>:d    r5.0<1;1,0>:d                    {I@6}
        mov (8|M0)               r18.0<1>:d    r48.1<2;1,0>:d                   {I@4}
        add3 (4|M0)              r50.1<2>:d    r6.0<1;0>:d       r8.1<4;2>:d       0:w               {I@4}
        mov (4|M0)               r7.0<1>:ud    acc0.0<1;1,0>:ud                
        mov (4|M0)               r51.0<2>:d    r11.0<1;1,0>:d                   {I@5}
        addc (8|M0)              r9.0<1>:ud    r12.0<2;1,0>:ud   r48.0<2;1,0>:ud  {AccWrEn,Compacted}
        add3 (4|M0)              r51.1<2>:d    r7.0<1;0>:d       r8.1<4;2>:d       0:w               {I@3}
        mov (8|M0)               r10.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        mov (8|M0)               r14.0<2>:d    r9.0<1;1,0>:d                    {I@3}
        addc (8|M0)              r6.0<1>:ud    r12.0<2;1,0>:ud   r50.0<2;1,0>:ud  {AccWrEn,Compacted,I@3}
        mov (8|M0)               r7.0<1>:d     r50.1<2;1,0>:d                  
        add3 (8|M0)              r5.0<1>:d     r10.0<1;0>:d      r12.1<4;2>:d      r18.0<1>:d       {I@4}
        mov (8|M0)               r19.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        mov (8|M0)               r16.0<2>:d    r6.0<1;1,0>:d                    {I@4}
        mov (8|M0)               r14.1<2>:d    r5.0<1;1,0>:d                    {I@3}
        addc (1|M0)              r11.0<1>:ud   r45.2<0;1,0>:ud   0x1:ud              {AccWrEn}
        add3 (8|M0)              r8.0<1>:d     r19.0<1;0>:d      r12.1<4;2>:d      r7.0<1>:d        {I@4}
        mov (1|M0)               r20.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r45.6<1>:d    r11.0<0;1,0>:d                   {I@3}
        mov (8|M0)               r16.1<2>:d    r8.0<1;1,0>:d                    {I@3}
        add3 (1|M0)              r45.7<1>:d    r20.0<0;0>:d      r45.3<0;0>:d      0:w               {I@3}
        send.dc1 (16|M0)         r52      r14     null:0  0x0            0x082411FF           {A@2,$3} // wr:4+0, rd:2; a64 dword gathering read
(f0.1.any16h) goto (16|M0)                   L2736                  L2736                
L2392:
        shl (1|M0)               r1.4<1>:d     r45.6<0;1,0>:d    2:w              
        shl (1|M0)               r1.2<1>:d     r45.7<0;1,0>:d    2:w               {I@3}
        shr (1|M0)               r1.3<1>:ud    r45.6<0;1,0>:ud   0x1E:uw             
        mov (8|M0)               r15.0<1>:d    r48.1<2;1,0>:d                   {$3.src}
        addc (1|M0)              r5.0<1>:ud    r47.0<0;1,0>:ud   r1.4<0;1,0>:ud   {AccWrEn,I@4}
        or (1|M0)                r1.5<1>:d     r1.2<0;1,0>:d     r1.3<0;1,0>:d    {I@3}
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r2.4<1>:d     r5.0<0;1,0>:d                    {I@3}
        mov (8|M0)               r5.0<1>:d     r50.1<2;1,0>:d                  
        add3 (1|M0)              r2.5<1>:d     r6.0<0;0>:d       r47.1<0;0>:d      r1.5<0>:d        {I@3}
        mov (8|M0)               r7.0<2>:ud    r2.4<0;1,0>:ud                   {I@3}
        mov (8|M0)               r7.1<2>:ud    r2.5<0;1,0>:ud                   {I@2}
        addc (8|M0)              r9.0<1>:ud    r7.0<2;1,0>:ud    r48.0<2;1,0>:ud  {AccWrEn,Compacted,I@1}
        mov (8|M0)               r10.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        addc (8|M0)              r17.0<1>:ud   r7.0<2;1,0>:ud    r50.0<2;1,0>:ud  {AccWrEn,Compacted}
        mov (8|M0)               r11.0<2>:d    r9.0<1;1,0>:d                    {I@3}
        add3 (8|M0)              r16.0<1>:d    r10.0<1;0>:d      r7.1<4;2>:d       r15.0<1>:d       {I@3}
        mov (8|M0)               r18.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        mov (8|M0)               r13.0<2>:d    r17.0<1;1,0>:d                   {I@4}
        mov (8|M0)               r11.1<2>:d    r16.0<1;1,0>:d                   {I@3}
        add3 (8|M0)              r6.0<1>:d     r18.0<1;0>:d      r7.1<4;2>:d       r5.0<1>:d        {I@3}
        mov (8|M0)               r13.1<2>:d    r6.0<1;1,0>:d                    {I@1}
        send.dc1 (16|M0)         r54      r11     null:0  0x0            0x082411FF           {A@1,$4} // wr:4+0, rd:2; a64 dword gathering read
        goto (16|M0)                         L2736                  L2920                
L2736:
        join (16|M0)                         L2920                                
L2752:
        add (1|M0)               r1.2<1>:d     r46.0<0;1,0>:d    -2:w               {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x2800:uw              {I@1}
        mach (1|M0)              r6.0<1>:ud    r1.2<0;1,0>:ud    0x2800:ud              {AccWrEn}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r6.0<0;1,0>:d     2:w               {Compacted,I@2}
        shl (1|M0)               r4.2<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@2}
        shr (1|M0)               r2.4<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r7.0<1>:ud    r45.4<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        sync.nop                             null                             {Compacted,$3.src}
        mov (1|M0)               r9.0<1>:f     r7.0<0;1,0>:f                    {Compacted,I@3}
        add3 (1|M0)              r9.1<1>:d     r8.0<0;0>:d       r45.5<0;0>:d      r4.3<0>:d        {I@1}
        send.ugm (1|M0)          r54      r9      null:0  0x0            0x0220D580           {A@1,$4} // wr:1+0, rd:2; load.ugm.d32x16t.a64
L2920:
        join (16|M0)                         L3904                                
L2936:
        add (1|M0)               r1.2<1>:d     r46.0<0;1,0>:d    -1:w               {Compacted}
        sync.nop                             null                             {Compacted,$4.src}
        mov (1|M0)               r14.5<1>:f    r52.7<0;1,0>:f                   {$3.dst}
        mov (1|M0)               r12.3<1>:f    r52.1<0;1,0>:f                  
        mov (1|M0)               r13.4<1>:ud   r52.4<0;1,0>:ud                 
        mov (1|M0)               r15.6<1>:f    r53.2<0;1,0>:f                  
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x2800:uw              {I@2}
        mov (1|M0)               r52.7<1>:f    r53.5<0;1,0>:f                  
        mov (1|M0)               r12.0<1>:ud   r52.0<0;1,0>:ud                 
        mach (1|M0)              r6.0<1>:ud    r1.2<0;1,0>:ud    0x2800:ud              {AccWrEn}
        mov (1|M0)               r12.6<1>:f    r52.2<0;1,0>:f                  
        mov (1|M0)               r13.1<1>:f    r52.3<0;1,0>:f                  
        mov (1|M0)               r13.7<1>:f    r52.5<0;1,0>:f                  
        mov (1|M0)               r14.2<1>:f    r52.6<0;1,0>:f                  
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r6.0<0;1,0>:d     2:w               {Compacted,I@2}
        mov (1|M0)               r15.0<1>:ud   r53.0<0;1,0>:ud                 
        mov (1|M0)               r15.3<1>:f    r53.1<0;1,0>:f                  
        mov (1|M0)               r12.1<1>:f    r54.0<0;1,0>:f                   {Compacted,$4.dst}
        shl (1|M0)               r4.2<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@3}
        shr (1|M0)               r2.4<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        mov (1|M0)               r12.4<1>:f    r54.1<0;1,0>:f                  
        mov (1|M0)               r12.7<1>:f    r54.2<0;1,0>:f                  
        mov (1|M0)               r13.2<1>:f    r54.3<0;1,0>:f                  
        addc (1|M0)              r7.0<1>:ud    r45.4<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        mov (1|M0)               r13.5<1>:f    r54.4<0;1,0>:f                  
        mov (1|M0)               r14.0<1>:f    r54.5<0;1,0>:f                   {Compacted}
        mov (1|M0)               r14.3<1>:f    r54.6<0;1,0>:f                  
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r9.0<1>:d     r7.0<0;1,0>:d                    {I@3}
        addc (1|M0)              r5.0<1>:ud    r47.0<0;1,0>:ud   0x80:ud              {AccWrEn}
        mov (1|M0)               r14.6<1>:f    r54.7<0;1,0>:f                  
        mov (1|M0)               r15.1<1>:f    r55.0<0;1,0>:f                   {Compacted}
        add3 (1|M0)              r9.1<1>:d     r8.0<0;0>:d       r45.5<0;0>:d      r4.3<0>:d        {I@3}
        mov (1|M0)               r15.4<1>:f    r55.1<0;1,0>:f                  
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r15.7<1>:f    r55.2<0;1,0>:f                  
        mov (1|M0)               r52.1<1>:f    r53.3<0;1,0>:f                  
        send.ugm (1|M0)          r10      r9      null:0  0x0            0x0220D580           {A@2,$5} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        mov (1|M0)               r52.4<1>:ud   r53.4<0;1,0>:ud                 
        mov (1|M0)               r53.2<1>:f    r53.6<0;1,0>:f                  
        mov (1|M0)               r53.5<1>:f    r53.7<0;1,0>:f                  
        mov (1|M0)               r52.2<1>:f    r55.3<0;1,0>:f                  
        mov (1|M0)               r52.5<1>:f    r55.4<0;1,0>:f                  
        mov (1|M0)               r53.0<1>:f    r55.5<0;1,0>:f                   {Compacted}
        mov (1|M0)               r7.0<1>:d     r5.0<0;1,0>:d                   
        mov (1|M0)               r53.3<1>:f    r55.6<0;1,0>:f                  
        add3 (1|M0)              r7.1<1>:d     r6.0<0;0>:d       r47.1<0;0>:d      0:w               {I@3}
        mov (1|M0)               r53.6<1>:f    r55.7<0;1,0>:f                  
        mov (1|M0)               r12.2<1>:f    r10.0<0;1,0>:f                   {Compacted,$5.dst}
        mov (1|M0)               r12.5<1>:f    r10.1<0;1,0>:f                  
        mov (1|M0)               r13.0<1>:f    r10.2<0;1,0>:f                   {Compacted}
        mov (1|M0)               r13.3<1>:ud   r10.3<0;1,0>:ud                 
        mov (1|M0)               r13.6<1>:f    r10.4<0;1,0>:f                  
        mov (1|M0)               r14.1<1>:f    r10.5<0;1,0>:f                  
        mov (1|M0)               r14.4<1>:f    r10.6<0;1,0>:f                  
        mov (1|M0)               r14.7<1>:ud   r10.7<0;1,0>:ud                 
        mov (1|M0)               r15.2<1>:f    r11.0<0;1,0>:f                   {Compacted}
        mov (1|M0)               r15.5<1>:f    r11.1<0;1,0>:f                  
        mov (1|M0)               r52.0<1>:f    r11.2<0;1,0>:f                   {Compacted}
        mov (1|M0)               r52.3<1>:ud   r11.3<0;1,0>:ud                 
        mov (1|M0)               r52.6<1>:f    r11.4<0;1,0>:f                  
        mov (1|M0)               r53.1<1>:f    r11.5<0;1,0>:f                  
        mov (1|M0)               r53.4<1>:f    r11.6<0;1,0>:f                   {I@6}
        mov (1|M0)               r53.7<1>:ud   r11.7<0;1,0>:ud                 
        send.ugm (1|M0)          null     r47     r12:4   0x0            0x0200D784           {A@3,$6} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r7      r52:2   0x0            0x0200C784           {A@1,$7} // wr:1+2, rd:0; store.ugm.d64x8t.a64
L3904:
        join (16|M0)                         L10640                                
L3920:
        shl (1|M0)               r1.3<1>:d     r43.2<0;1,0>:d    7:w              
        shr (1|M0)               r1.2<1>:ud    r43.2<0;1,0>:ud   0x19:uw             
        shl (1|M0)               r46.1<1>:d    r43.3<0;1,0>:d    2:w              
        shl (1|M0)               r5.3<1>:d     r43.4<0;1,0>:d    2:w              
        shr (1|M0)               r5.4<1>:ud    r43.3<0;1,0>:ud   0x1E:uw             
        shl (1|M0)               r4.2<1>:d     r1.3<0;1,0>:d     2:w               {I@5}
        or (1|M0)                r1.4<1>:d     r1.2<0;1,0>:d     0:w               {I@5}
        shl (1|M0)               r1.2<1>:d     r43.0<0;1,0>:d    5:w               {Compacted}
        shr (1|M0)               r2.4<1>:ud    r1.3<0;1,0>:ud    0x1E:uw             
        or (1|M0)                r46.2<1>:d    r5.3<0;1,0>:d     r5.4<0;1,0>:d    {I@5}
        addc (1|M0)              r5.0<1>:ud    r43.5<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@5}
        shl (1|M0)               r2.3<1>:d     r1.4<0;1,0>:d     2:w               {I@5}
        shl (1|M0)               r47.2<1>:d    r43.2<0;1,0>:d    2:w               {$6.src}
        shr (1|M0)               r4.2<1>:ud    r1.2<0;1,0>:ud    0x1E:uw              {I@6}
        shl (1|M0)               r47.6<1>:d    r43.2<0;1,0>:d    7:w              
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        sync.nop                             null                             {Compacted,$7.src}
        addc (1|M0)              r7.0<1>:ud    r5.0<0;1,0>:ud    r46.1<0;1,0>:ud  {AccWrEn,I@6}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@6}
        mov (1|M0)               r47.7<1>:d    0:w                              
        sel (1|M0)    (lt)f0.0   r56.2<1>:ud   r46.0<0;1,0>:ud   0x6:uw             
        mov (1|M0)               r56.3<1>:d    0:w                              
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x1800:uw             
        add3 (1|M0)              r5.2<1>:d     r6.0<0;0>:d       r43.6<0;0>:d      r4.3<0>:d        {I@6}
        mov (1|M0)               r46.7<1>:f    0.0:f                               {Compacted}
        mach (1|M0)              r10.0<1>:ud   r1.2<0;1,0>:ud    0x1800:ud              {AccWrEn}
        mov (1|M0)               r56.4<2>:f    0.0:f                              
        mov (1|M0)               r56.5<2>:f    0.0:f                              
        add3 (1|M0)              r1.6<1>:d     r8.0<0;0>:d       r5.2<0;0>:d       r46.2<0>:d       {I@2}
        shl (1|M0)               r46.5<1>:d    r1.2<0;1,0>:d     2:w              
        sync.nop                             null                             {Compacted,$4.src}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted,$3.src}
        shl (1|M0)               r1.7<1>:d     r10.0<0;1,0>:d    2:w               {Compacted,I@4}
        or (1|M0)                r46.6<1>:d    r4.2<0;1,0>:d     0:w              
        shl (1|M0)               r2.4<1>:d     r9.0<0;1,0>:d     2:w               {Compacted,I@3}
        shr (1|M0)               r2.3<1>:ud    r9.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r6.0<1>:ud    r7.0<0;1,0>:ud    r2.4<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r2.5<1>:d     r1.7<0;1,0>:d     r2.3<0;1,0>:d    {I@2}
        and (1|M0)               r2.3<1>:d     r1.3<0;1,0>:d     1920:w               {Compacted}
        mov (1|M0)               r56.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@3}
        mov (1|M0)               r11.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r46.3<1>:d    r2.3<0;1,0>:d     2:w               {Compacted,I@2}
        addc (1|M0)              r5.0<1>:ud    r56.0<0;1,0>:ud   0x6000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r56.1<1>:d    r11.0<0;0>:d      r1.6<0;0>:d       r2.5<0>:d        {I@3}
        shr (1|M0)               r2.5<1>:ud    r2.3<0;1,0>:ud    0x1E:uw             
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r56.0<0;1,0>:ud   0xC000:ud              {AccWrEn}
        mov (1|M0)               r57.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@5}
        send.ugm (1|M0)          r212     r56     null:0  0x0            0x0240E580           {A@4,$8} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        or (1|M0)                r46.4<1>:d    r2.5<0;1,0>:d     0:w               {I@3}
        add3 (1|M0)              r57.1<1>:d    r8.0<0;0>:d       r56.1<0;0>:d      0:w               {I@3}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r56.0<0;1,0>:ud   0x12000:ud              {AccWrEn}
        mov (1|M0)               r58.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@5}
        send.ugm (1|M0)          r208     r57     null:0  0x0            0x0240E580           {A@2,$9} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r58.1<1>:d    r10.0<0;0>:d      r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r56.0<0;1,0>:ud   0x18000:ud              {AccWrEn}
        mov (1|M0)               r59.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r204     r58     null:0  0x0            0x0240E580           {A@2,$10} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r59.1<1>:d    r7.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r56.0<0;1,0>:ud   0x1E000:ud              {AccWrEn}
        mov (1|M0)               r60.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r200     r59     null:0  0x0            0x0240E580           {A@2,$11} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r60.1<1>:d    r8.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r56.0<0;1,0>:ud   0x24000:ud              {AccWrEn}
        mov (1|M0)               r61.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r196     r60     null:0  0x0            0x0240E580           {A@2,$12} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r61.1<1>:d    r10.0<0;0>:d      r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r56.0<0;1,0>:ud   0x2A000:ud              {AccWrEn}
        mov (1|M0)               r62.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r192     r61     null:0  0x0            0x0240E580           {A@2,$13} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r62.1<1>:d    r7.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r56.0<0;1,0>:ud   0x30000:ud              {AccWrEn}
        mov (1|M0)               r63.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r188     r62     null:0  0x0            0x0240E580           {A@2,$14} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r63.1<1>:d    r8.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r56.0<0;1,0>:ud   0x36000:ud              {AccWrEn}
        mov (1|M0)               r64.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r184     r63     null:0  0x0            0x0240E580           {A@2,$15} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r64.1<1>:d    r10.0<0;0>:d      r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r56.0<0;1,0>:ud   0x3C000:ud              {AccWrEn}
        mov (1|M0)               r65.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r180     r64     null:0  0x0            0x0240E580           {A@2,$0} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r65.1<1>:d    r7.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r56.0<0;1,0>:ud   0x42000:ud              {AccWrEn}
        mov (1|M0)               r66.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r176     r65     null:0  0x0            0x0240E580           {A@2,$1} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r66.1<1>:d    r8.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r56.0<0;1,0>:ud   0x48000:ud              {AccWrEn}
        mov (1|M0)               r67.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r172     r66     null:0  0x0            0x0240E580           {A@2,$2} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r67.1<1>:d    r10.0<0;0>:d      r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r56.0<0;1,0>:ud   0x4E000:ud              {AccWrEn}
        mov (1|M0)               r68.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r168     r67     null:0  0x0            0x0240E580           {A@2,$4} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r68.1<1>:d    r7.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r56.0<0;1,0>:ud   0x54000:ud              {AccWrEn}
        mov (1|M0)               r69.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r164     r68     null:0  0x0            0x0240E580           {A@2,$5} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r69.1<1>:d    r8.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r56.0<0;1,0>:ud   0x5A000:ud              {AccWrEn}
        mov (1|M0)               r70.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r160     r69     null:0  0x0            0x0240E580           {A@2,$6} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r70.1<1>:d    r10.0<0;0>:d      r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r56.0<0;1,0>:ud   0x60000:ud              {AccWrEn}
        mov (1|M0)               r71.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r156     r70     null:0  0x0            0x0240E580           {A@2,$7} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r71.1<1>:d    r7.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r56.0<0;1,0>:ud   0x66000:ud              {AccWrEn}
        mov (1|M0)               r72.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r152     r71     null:0  0x0            0x0240E580           {A@2,$3} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r72.1<1>:d    r8.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r56.0<0;1,0>:ud   0x6C000:ud              {AccWrEn}
        mov (1|M0)               r73.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r148     r72     null:0  0x0            0x0240E580           {A@2,$8} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r73.1<1>:d    r10.0<0;0>:d      r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r56.0<0;1,0>:ud   0x72000:ud              {AccWrEn}
        mov (1|M0)               r74.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r144     r73     null:0  0x0            0x0240E580           {A@2,$9} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r74.1<1>:d    r7.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r56.0<0;1,0>:ud   0x78000:ud              {AccWrEn}
        mov (1|M0)               r75.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r140     r74     null:0  0x0            0x0240E580           {A@2,$10} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r75.1<1>:d    r8.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r56.0<0;1,0>:ud   0x7E000:ud              {AccWrEn}
        mov (1|M0)               r76.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r136     r75     null:0  0x0            0x0240E580           {A@2,$11} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r76.1<1>:d    r10.0<0;0>:d      r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r56.0<0;1,0>:ud   0x84000:ud              {AccWrEn}
        mov (1|M0)               r77.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r132     r76     null:0  0x0            0x0240E580           {A@2,$12} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r77.1<1>:d    r7.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r56.0<0;1,0>:ud   0x8A000:ud              {AccWrEn}
        mov (1|M0)               r78.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r128     r77     null:0  0x0            0x0240E580           {A@2,$13} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r78.1<1>:d    r8.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r56.0<0;1,0>:ud   0x90000:ud              {AccWrEn}
        mov (1|M0)               r79.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r124     r78     null:0  0x0            0x0240E580           {A@2,$14} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r79.1<1>:d    r10.0<0;0>:d      r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r56.0<0;1,0>:ud   0x96000:ud              {AccWrEn}
        mov (1|M0)               r80.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r120     r79     null:0  0x0            0x0240E580           {A@2,$15} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r80.1<1>:d    r7.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r56.0<0;1,0>:ud   0x9C000:ud              {AccWrEn}
        mov (1|M0)               r81.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r116     r80     null:0  0x0            0x0240E580           {A@2,$0} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r81.1<1>:d    r8.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r56.0<0;1,0>:ud   0xA2000:ud              {AccWrEn}
        mov (1|M0)               r82.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r112     r81     null:0  0x0            0x0240E580           {A@2,$1} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r82.1<1>:d    r10.0<0;0>:d      r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r56.0<0;1,0>:ud   0xA8000:ud              {AccWrEn}
        mov (1|M0)               r83.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r108     r82     null:0  0x0            0x0240E580           {A@2,$2} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r83.1<1>:d    r7.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r56.0<0;1,0>:ud   0xAE000:ud              {AccWrEn}
        mov (1|M0)               r84.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r104     r83     null:0  0x0            0x0240E580           {A@2,$4} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r84.1<1>:d    r8.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r56.0<0;1,0>:ud   0xB4000:ud              {AccWrEn}
        mov (1|M0)               r85.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r100     r84     null:0  0x0            0x0240E580           {A@2,$5} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r85.1<1>:d    r10.0<0;0>:d      r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r56.0<0;1,0>:ud   0xBA000:ud              {AccWrEn}
        mov (1|M0)               r86.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r96      r85     null:0  0x0            0x0240E580           {A@2,$6} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r86.1<1>:d    r7.0<0;0>:d       r56.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r87.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@3}
        addc (1|M0)              r6.0<1>:ud    r44.6<0;1,0>:ud   r47.2<0;1,0>:ud  {AccWrEn,F@2}
        shr (1|M0)               r5.0<1>:ud    r43.2<0;1,0>:ud   0x1E:uw              {F@1}
        send.ugm (1|M0)          r92      r86     null:0  0x0            0x0240E580           {A@4,$7} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r87.1<1>:d    r8.0<0;0>:d       r56.1<0;0>:d      0:w               {I@3}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        or (1|M0)                r47.3<1>:d    r5.0<0;1,0>:d     0:w               {Compacted,I@3}
        mov (1|M0)               r47.4<1>:d    r6.0<0;1,0>:d                   
        send.ugm (1|M0)          r88      r87     null:0  0x0            0x0240E580           {A@4,$3} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add3 (1|M0)              r47.5<1>:d    r7.0<0;0>:d       r44.7<0;0>:d      r47.3<0>:d       {I@2}
L6632:
        mul (1|M0)               acc0.0<1>:ud  r56.4<0;1,0>:ud   0x2800:uw             
        mul (1|M0)               r1.2<1>:d     r56.5<0;1,0>:d    10240:w              
        mul (1|M0)               r2.3<1>:d     r56.5<0;1,0>:d    48:w              
        mach (1|M0)              r6.0<1>:ud    r56.4<0;1,0>:ud   0x2800:ud              {AccWrEn}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        add (1|M0)               r2.4<1>:d     r1.2<0;1,0>:d     r6.0<0;1,0>:d    {I@2}
        shl (1|M0)               r1.2<1>:d     r56.4<0;1,0>:d    9:w              
        shl (1|M0)               r4.4<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@3}
        shr (1|M0)               r4.3<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        shl (1|M0)               r4.2<1>:d     r2.4<0;1,0>:d     2:w               {I@4}
        addc (1|M0)              r7.0<1>:ud    r44.2<0;1,0>:ud   r4.4<0;1,0>:ud   {AccWrEn,I@3}
        or (1|M0)                r4.5<1>:d     r4.2<0;1,0>:d     r4.3<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r7.0<0;1,0>:ud    0x2000:ud              {AccWrEn,I@3}
        add3 (1|M0)              r7.2<1>:d     r8.0<0;0>:d       r44.3<0;0>:d      r4.5<0>:d        {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r11.0<1>:ud   r9.0<0;1,0>:ud    r46.3<0;1,0>:ud  {AccWrEn,I@3}
        add3 (1|M0)              r7.4<1>:d     r10.0<0;0>:d      r7.2<0;0>:d       0:w               {I@2}
        mov (1|M0)               r12.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r13.0<1>:ud   r11.0<0;1,0>:ud   r46.5<0;1,0>:ud  {AccWrEn,I@3}
        add3 (1|M0)              r1.3<1>:d     r12.0<0;0>:d      r7.4<0;0>:d       r46.4<0>:d       {I@2}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r6.0<1>:d     r13.0<0;1,0>:d                   {I@3}
        mul (1|M0)               acc0.0<1>:ud  r56.4<0;1,0>:ud   0x30:uw             
        add3 (1|M0)              r6.1<1>:d     r5.0<0;0>:d       r1.3<0;0>:d       r46.6<0>:d       {I@3}
        mach (1|M0)              r19.0<1>:ud   r56.4<0;1,0>:ud   0x30:ud              {AccWrEn}
        sync.nop                             null                             {Compacted,$3.src}
        send.ugm (1|M0)          r14      r6      null:0  0x0            0x0240E580           {A@2,$8} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        mov (1|M0)               r18.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        add (1|M0)               r4.3<1>:d     r2.3<0;1,0>:d     r19.0<0;1,0>:d   {Compacted,I@2}
        shl (1|M0)               r7.5<1>:d     r18.0<0;1,0>:d    2:w               {Compacted,I@2}
        shr (1|M0)               r7.4<1>:ud    r18.0<0;1,0>:ud   0x1E:uw             
        shl (1|M0)               r7.3<1>:d     r4.3<0;1,0>:d     2:w               {Compacted,I@3}
        addc (1|M0)              r5.0<1>:ud    r47.4<0;1,0>:ud   r7.5<0;1,0>:ud   {AccWrEn,I@3}
        or (1|M0)                r7.6<1>:d     r7.3<0;1,0>:d     r7.4<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r10.0<1>:d    r5.0<0;1,0>:d                    {I@3}
        and (1|M0)               r5.0<1>:d     r1.2<0;1,0>:d     512:w               {Compacted}
        add3 (1|M0)              r10.1<1>:d    r8.0<0;0>:d       r47.5<0;0>:d      r7.6<0>:d        {I@3}
        send.ugm (1|M0)          r6       r10     null:0  0x0            0x04100580           {A@1,$9} // wr:2+0, rd:1; load.ugm.d32.a64
        mad (1|M0)               r10.0<1>:d    r5.0<0;0>:d       r1.0<0;0>:uw      128:w               {$9.src}
        mad (16|M0)              acc0.0<1>:f   r46.7<0;0>:f      r212.0<1;0>:f     r14.0<0>:f       {Compacted,$8.dst}
        mad (16|M0)              acc2.0<1>:f   r46.7<0;0>:f      r214.0<1;0>:f     r14.0<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r208.0<1;0>:f     r14.1<0>:f       {Compacted,$9.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r210.0<1;0>:f     r14.1<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r204.0<1;0>:f     r14.2<0>:f       {Compacted,$10.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r206.0<1;0>:f     r14.2<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r200.0<1;0>:f     r14.3<0>:f       {Compacted,$11.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r202.0<1;0>:f     r14.3<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r196.0<1;0>:f     r14.4<0>:f       {Compacted,$12.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r198.0<1;0>:f     r14.4<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r192.0<1;0>:f     r14.5<0>:f       {Compacted,$13.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r194.0<1;0>:f     r14.5<0>:f       {Compacted}
        mul (1|M0)               r6.0<1>:f     r6.0<0;1,0>:f     1.442695e+00:f               {$9.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r188.0<1;0>:f     r14.6<0>:f       {Compacted,$14.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r190.0<1;0>:f     r14.6<0>:f       {Compacted}
        math.exp (1|M0)          r6.0<1>:f     r6.0<0;1,0>:f                    {@3,$10}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r184.0<1;0>:f     r14.7<0>:f       {Compacted,$15.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r186.0<1;0>:f     r14.7<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r180.0<1;0>:f     r15.0<0>:f       {Compacted,$0.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r182.0<1;0>:f     r15.0<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r176.0<1;0>:f     r15.1<0>:f       {Compacted,$1.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r178.0<1;0>:f     r15.1<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r172.0<1;0>:f     r15.2<0>:f       {Compacted,$2.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r174.0<1;0>:f     r15.2<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r168.0<1;0>:f     r15.3<0>:f       {Compacted,$4.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r170.0<1;0>:f     r15.3<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r164.0<1;0>:f     r15.4<0>:f       {Compacted,$5.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r166.0<1;0>:f     r15.4<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r160.0<1;0>:f     r15.5<0>:f       {Compacted,$6.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r162.0<1;0>:f     r15.5<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r156.0<1;0>:f     r15.6<0>:f       {Compacted,$7.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r158.0<1;0>:f     r15.6<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r152.0<1;0>:f     r15.7<0>:f       {Compacted,$3.dst}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r154.0<1;0>:f     r15.7<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r148.0<1;0>:f     r16.0<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r150.0<1;0>:f     r16.0<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r144.0<1;0>:f     r16.1<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r146.0<1;0>:f     r16.1<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r140.0<1;0>:f     r16.2<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r142.0<1;0>:f     r16.2<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r136.0<1;0>:f     r16.3<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r138.0<1;0>:f     r16.3<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r132.0<1;0>:f     r16.4<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r134.0<1;0>:f     r16.4<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r128.0<1;0>:f     r16.5<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r130.0<1;0>:f     r16.5<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r124.0<1;0>:f     r16.6<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r126.0<1;0>:f     r16.6<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r120.0<1;0>:f     r16.7<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r122.0<1;0>:f     r16.7<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r116.0<1;0>:f     r17.0<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r118.0<1;0>:f     r17.0<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r112.0<1;0>:f     r17.1<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r114.0<1;0>:f     r17.1<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r108.0<1;0>:f     r17.2<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r110.0<1;0>:f     r17.2<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r104.0<1;0>:f     r17.3<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r106.0<1;0>:f     r17.3<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r100.0<1;0>:f     r17.4<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r102.0<1;0>:f     r17.4<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r96.0<1;0>:f      r17.5<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r98.0<1;0>:f      r17.5<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r92.0<1;0>:f      r17.6<0>:f       {Compacted}
        mad (16|M0)              acc2.0<1>:f   acc2.0<1;0>:f     r94.0<1;0>:f      r17.6<0>:f       {Compacted}
        mad (16|M0)              r18.0<1>:f    acc0.0<1;0>:f     r88.0<1;0>:f      r17.7<0>:f       {Compacted}
        mad (16|M0)              r20.0<1>:f    acc2.0<1;0>:f     r90.0<1;0>:f      r17.7<0>:f       {Compacted}
        send.slm (1|M0)          null     r10     r18:4   0x0            0x0200D704           {A@1,$11} // wr:1+4, rd:0; store.slm.d64x16t.a32
(W)     send.slm (1|M0)          r11      r0      null:0  0x0            0x0210001F           {$12} // wr:1+0, rd:1; fence.slm.none.group
(W)     mov (1|M0)               r12.2<1>:ud   0x0:ud                             
(W)     mov (2|M0)               r12.10<1>:ub  r0.11<0;1,0>:ub                 
(W)     mov (8|M0)               null<1>:ud    r11.0<1;1,0>:ud                  {$12.dst}
(W)     send.gtwy (1|M0)         null     r12     null:0  0x0            0x02000004           {A@2,$13} // wr:1+0, rd:0; signal barrier
(W)     sync.bar                             null                            
        addc (1|M0)              r9.0<1>:ud    r7.0<0;1,0>:ud    0x4000:ud              {AccWrEn}
        shl (1|M0)               r1.6<1>:d     r47.6<0;1,0>:d    2:w              
        shl (1|M0)               r1.4<1>:d     r47.7<0;1,0>:d    2:w              
        shr (1|M0)               r1.5<1>:ud    r47.6<0;1,0>:ud   0x1E:uw             
        send.slm (1|M0)          r22      r5      null:0  0x0            0x0280E700           {$14} // wr:1+0, rd:8; load.slm.d64x32t.a32
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted,$11.src}
        addc (1|M0)              r11.0<1>:ud   r9.0<0;1,0>:ud    r1.6<0;1,0>:ud   {AccWrEn,I@4}
        or (1|M0)                r8.0<1>:ud    r5.0<0;1,0>:ud    0x100:uw             
        or (1|M0)                r1.7<1>:d     r1.4<0;1,0>:d     r1.5<0;1,0>:d    {I@4}
        add3 (1|M0)              r1.3<1>:d     r10.0<0;0>:d      r7.2<0;0>:d       0:w               {I@4}
        mov (1|M0)               r12.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted,$13.src}
        sync.nop                             null                             {Compacted,$14.src}
        addc (1|M0)              r5.0<1>:ud    r11.0<0;1,0>:ud   r46.1<0;1,0>:ud  {AccWrEn,I@5}
        send.slm (1|M0)          r30      r8      null:0  0x0            0x0280E700           {A@5,$15} // wr:1+0, rd:8; load.slm.d64x32t.a32
        add3 (1|M0)              r2.4<1>:d     r12.0<0;0>:d      r1.3<0;0>:d       r1.7<0>:d        {I@2}
        mov (1|M0)               r13.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        sync.nop                             null                             {Compacted,$15.src}
        mov (1|M0)               r8.0<1>:d     r5.0<0;1,0>:d                    {I@3}
        addc (1|M0)              r5.0<1>:ud    r45.0<0;1,0>:ud   r47.2<0;1,0>:ud  {AccWrEn}
        add (16|M0)              acc2.0<1>:f   r22.0<1;1,0>:f    r26.0<1;1,0>:f   {Compacted,$14.dst}
        add3 (1|M0)              r8.1<1>:d     r13.0<0;0>:d      r2.4<0;0>:d       r46.2<0>:d       {I@3}
        add (16|M0)              acc4.0<1>:f   r24.0<1;1,0>:f    r28.0<1;1,0>:f   {Compacted}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r13.0<1>:ud   r5.0<0;1,0>:ud    r7.5<0;1,0>:ud   {AccWrEn,I@3}
        send.ugm (1|M0)          r18      r8      null:0  0x0            0x0240E580           {A@3,$0} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        add (16|M0)              acc2.0<1>:f   acc2.0<1;1,0>:f   r30.0<1;1,0>:f   {Compacted,$15.dst}
        add3 (1|M0)              r1.3<1>:d     r7.0<0;0>:d       r45.1<0;0>:d      r47.3<0>:d       {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted,$0.src}
        mov (1|M0)               r42.0<1>:d    r13.0<0;1,0>:d                   {I@3}
        add (16|M0)              acc0.0<1>:f   acc4.0<1;1,0>:f   r32.0<1;1,0>:f   {Compacted,I@2}
        add (16|M0)              acc2.0<1>:f   acc2.0<1;1,0>:f   r34.0<1;1,0>:f   {Compacted}
        add3 (1|M0)              r42.1<1>:d    r8.0<0;0>:d       r1.3<0;0>:d       r7.6<0>:d       
        add (16|M0)              r38.0<1>:f    acc0.0<1;1,0>:f   r36.0<1;1,0>:f   {Compacted}
        addc (1|M0)              r5.0<1>:ud    r56.4<0;1,0>:ud   0x1:ud              {AccWrEn,A@1}
        send.ugm (1|M0)          r22      r42     null:0  0x0            0x04100580           {A@2,$3} // wr:2+0, rd:1; load.ugm.d32.a64
        mov (1|M0)               r56.6<1>:d    r5.0<0;1,0>:d                    {I@1}
        mov (1|M0)               r13.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        cmp (1|M0)    (eq)f0.1   null<1>:d     r56.6<0;1,0>:d    r56.2<0;1,0>:d   {I@2}
        add3 (1|M0)              r56.7<1>:d    r13.0<0;0>:d      r56.5<0;0>:d      0:w               {I@2}
(f0.1)  cmp (1|M0)    (eq)f0.1   null<1>:d     r56.7<0;1,0>:d    r56.3<0;1,0>:d   {I@1}
(W)     and (1|M0)               f0.0<1>:uw    f0.1<0;1,0>:uw    0x1:uw             
        sync.nop                             null                             {Compacted,$0.dst}
        mad (16|M0)              acc0.0<1>:f   r18.0<1;0>:f      -acc2.0<1;0>:f    r6.0<0>:f        {$10.dst}
        mad (16|M0)              acc2.0<1>:f   r20.0<1;0>:f      -r38.0<1;0>:f     r6.0<0>:f        {Compacted}
        mul (16|M0)              acc0.0<1>:f   acc0.0<1;1,0>:f   r22.0<0;1,0>:f   {Compacted,$3.dst}
        mul (16|M0)              acc2.0<1>:f   acc2.0<1;1,0>:f   r22.0<0;1,0>:f   {Compacted}
        mul (16|M0)              acc4.0<1>:f   r14.0<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r14.1<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r7.0<1>:f     r14.2<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r14.0<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r25.0<1>:f    r14.1<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r27.0<1>:f    r14.2<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r212.0<1>:f   acc4.0<1;0>:f     r212.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r208.0<1>:f   acc6.0<1;0>:f     r208.0<1;0>:f     r6.0<0>:f        {Compacted,$7.src}
        sync.nop                             null                             {Compacted,$6.src}
        mad (16|M0)              r204.0<1>:f   r7.0<1;0>:f       r204.0<1;0>:f     r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r214.0<1>:f   r18.0<1;0>:f      r214.0<1;0>:f     r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r210.0<1>:f   r25.0<1;0>:f      r210.0<1;0>:f     r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r206.0<1>:f   r27.0<1;0>:f      r206.0<1;0>:f     r6.0<0>:f        {Compacted,F@6}
        mul (16|M0)              acc4.0<1>:f   r14.3<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r14.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r14.4<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r7.0<1>:f     r14.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r14.4<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r24.0<1>:f    r14.5<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r26.0<1>:f    r14.6<0;1,0>:f    acc0.0<1;1,0>:f 
        mad (16|M0)              r200.0<1>:f   acc4.0<1;0>:f     r200.0<1;0>:f     r6.0<0>:f        {Compacted,$5.src}
        mad (16|M0)              r202.0<1>:f   acc6.0<1;0>:f     r202.0<1;0>:f     r6.0<0>:f        {Compacted}
        sync.nop                             null                             {Compacted,$4.src}
        mad (16|M0)              r198.0<1>:f   r20.0<1;0>:f      r198.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        sync.nop                             null                             {Compacted,$2.src}
        mad (16|M0)              r194.0<1>:f   r7.0<1;0>:f       r194.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r196.0<1>:f   r18.0<1;0>:f      r196.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r192.0<1>:f   r24.0<1;0>:f      r192.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        sync.nop                             null                             {Compacted,$1.src}
        mad (16|M0)              r188.0<1>:f   r26.0<1;0>:f      r188.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mul (16|M0)              acc4.0<1>:f   r14.6<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r14.7<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r15.0<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r7.0<1>:f     r15.1<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r14.7<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r24.0<1>:f    r15.0<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r26.0<1>:f    r15.1<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r190.0<1>:f   acc4.0<1;0>:f     r190.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r184.0<1>:f   acc6.0<1;0>:f     r184.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r180.0<1>:f   r20.0<1;0>:f      r180.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r176.0<1>:f   r7.0<1;0>:f       r176.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r186.0<1>:f   r18.0<1;0>:f      r186.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r182.0<1>:f   r24.0<1;0>:f      r182.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r178.0<1>:f   r26.0<1;0>:f      r178.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mul (16|M0)              acc4.0<1>:f   r15.2<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r15.2<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r15.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r7.0<1>:f     r15.4<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r15.3<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r24.0<1>:f    r15.4<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r26.0<1>:f    r15.5<0;1,0>:f    acc0.0<1;1,0>:f 
        mad (16|M0)              r172.0<1>:f   acc4.0<1;0>:f     r172.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r174.0<1>:f   acc6.0<1;0>:f     r174.0<1;0>:f     r6.0<0>:f        {Compacted}
        sync.nop                             null                             {Compacted,$8.src}
        mad (16|M0)              r170.0<1>:f   r20.0<1;0>:f      r170.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        sync.nop                             null                             {Compacted,$9.src}
        mad (16|M0)              r166.0<1>:f   r7.0<1;0>:f       r166.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r168.0<1>:f   r18.0<1;0>:f      r168.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r164.0<1>:f   r24.0<1;0>:f      r164.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        sync.nop                             null                             {Compacted,$10.src}
        mad (16|M0)              r160.0<1>:f   r26.0<1;0>:f      r160.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mul (16|M0)              acc4.0<1>:f   r15.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r15.6<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r15.7<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r7.0<1>:f     r16.0<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r15.6<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r24.0<1>:f    r15.7<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r26.0<1>:f    r16.0<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r162.0<1>:f   acc4.0<1;0>:f     r162.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r156.0<1>:f   acc6.0<1;0>:f     r156.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r152.0<1>:f   r20.0<1;0>:f      r152.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r148.0<1>:f   r7.0<1;0>:f       r148.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r158.0<1>:f   r18.0<1;0>:f      r158.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r154.0<1>:f   r24.0<1;0>:f      r154.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r150.0<1>:f   r26.0<1;0>:f      r150.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mul (16|M0)              acc4.0<1>:f   r16.1<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r16.1<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r16.2<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r7.0<1>:f     r16.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r16.2<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r24.0<1>:f    r16.3<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r26.0<1>:f    r16.4<0;1,0>:f    acc0.0<1;1,0>:f 
        mad (16|M0)              r144.0<1>:f   acc4.0<1;0>:f     r144.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r146.0<1>:f   acc6.0<1;0>:f     r146.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r142.0<1>:f   r20.0<1;0>:f      r142.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r138.0<1>:f   r7.0<1;0>:f       r138.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r140.0<1>:f   r18.0<1;0>:f      r140.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r136.0<1>:f   r24.0<1;0>:f      r136.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r132.0<1>:f   r26.0<1;0>:f      r132.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mul (16|M0)              acc4.0<1>:f   r16.4<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r16.5<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r16.6<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r7.0<1>:f     r16.7<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r16.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r24.0<1>:f    r16.6<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r26.0<1>:f    r16.7<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r134.0<1>:f   acc4.0<1;0>:f     r134.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r128.0<1>:f   acc6.0<1;0>:f     r128.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r124.0<1>:f   r20.0<1;0>:f      r124.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r120.0<1>:f   r7.0<1;0>:f       r120.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r130.0<1>:f   r18.0<1;0>:f      r130.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r126.0<1>:f   r24.0<1;0>:f      r126.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r122.0<1>:f   r26.0<1;0>:f      r122.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mul (16|M0)              acc4.0<1>:f   r17.0<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r17.0<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r17.1<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r7.0<1>:f     r17.2<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r17.1<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r24.0<1>:f    r17.2<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r26.0<1>:f    r17.3<0;1,0>:f    acc0.0<1;1,0>:f 
        mad (16|M0)              r116.0<1>:f   acc4.0<1;0>:f     r116.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r118.0<1>:f   acc6.0<1;0>:f     r118.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r114.0<1>:f   r20.0<1;0>:f      r114.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r110.0<1>:f   r7.0<1;0>:f       r110.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r112.0<1>:f   r18.0<1;0>:f      r112.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r108.0<1>:f   r24.0<1;0>:f      r108.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r104.0<1>:f   r26.0<1;0>:f      r104.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mul (16|M0)              acc4.0<1>:f   r17.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r17.4<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r17.5<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r7.0<1>:f     r17.6<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r17.4<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r24.0<1>:f    r17.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r26.0<1>:f    r17.6<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc0.0<1>:f   r17.7<0;1,0>:f    acc0.0<1;1,0>:f 
        mul (16|M0)              acc2.0<1>:f   r17.7<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r106.0<1>:f   acc4.0<1;0>:f     r106.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r100.0<1>:f   acc6.0<1;0>:f     r100.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r96.0<1>:f    r20.0<1;0>:f      r96.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r92.0<1>:f    r7.0<1;0>:f       r92.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r102.0<1>:f   r18.0<1;0>:f      r102.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r98.0<1>:f    r24.0<1;0>:f      r98.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r94.0<1>:f    r26.0<1;0>:f      r94.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r88.0<1>:f    acc0.0<1;0>:f     r88.0<1;0>:f      r6.0<0>:f        {Compacted}
        mad (16|M0)              r90.0<1>:f    acc2.0<1;0>:f     r90.0<1;0>:f      r6.0<0>:f        {Compacted}
(~f0.0.any16h) goto (16|M0)                  L10592                  L10592                
L10040:
        send.ugm (1|M0)          null     r56     r212:4  0x0            0x0200D784           {$13} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r57     r208:4  0x0            0x0200D784           {$7} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r58     r204:4  0x0            0x0200D784           {$6} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r59     r200:4  0x0            0x0200D784           {$5} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r60     r196:4  0x0            0x0200D784           {$4} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r61     r192:4  0x0            0x0200D784           {$2} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r62     r188:4  0x0            0x0200D784           {$1} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r63     r184:4  0x0            0x0200D784           {$0} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r64     r180:4  0x0            0x0200D784           {$15} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r65     r176:4  0x0            0x0200D784           {$14} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r66     r172:4  0x0            0x0200D784           {$3} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r67     r168:4  0x0            0x0200D784           {$8} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r68     r164:4  0x0            0x0200D784           {$9} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r69     r160:4  0x0            0x0200D784           {$10} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r70     r156:4  0x0            0x0200D784           {$7} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r71     r152:4  0x0            0x0200D784           {$11} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r72     r148:4  0x0            0x0200D784           {$12} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r73     r144:4  0x0            0x0200D784           {$13} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r74     r140:4  0x0            0x0200D784           {$6} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r75     r136:4  0x0            0x0200D784           {$14} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r76     r132:4  0x0            0x0200D784           {$15} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r77     r128:4  0x0            0x0200D784           {$0} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r78     r124:4  0x0            0x0200D784           {$1} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r79     r120:4  0x0            0x0200D784           {$2} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r80     r116:4  0x0            0x0200D784           {$4} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r81     r112:4  0x0            0x0200D784           {$5} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r82     r108:4  0x0            0x0200D784           {$10} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        sync.nop                             null                             {Compacted,F@7}
        send.ugm (1|M0)          null     r83     r104:4  0x0            0x0200D784           {$6} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r84     r100:4  0x0            0x0200D784           {A@5,$7} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        sync.nop                             null                             {Compacted,F@4}
        send.ugm (1|M0)          null     r85     r96:4   0x0            0x0200D784           {$3} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        sync.nop                             null                             {Compacted,F@3}
        send.ugm (1|M0)          null     r86     r92:4   0x0            0x0200D784           {$8} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r87     r88:4   0x0            0x0200D784           {A@1,$9} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        goto (16|M0)                         L10592                  L10640                
L10592:
        join (16|M0)                         L10640                                
L10608:
(W)     mov (2|M0)               r56.4<1>:ud   r56.6<1;1,0>:ud                  {$13.src}
        goto.b (16|M0)                       L10640                  L6632                
L10640:
        join (16|M0)                         L10656                                
L10656:
(W)     mov (8|M0)               r255.0<1>:f   r0.0<1;1,0>:f                    {Compacted}
(W)     send.ugm (1|M0)          r5       r0      null:0  0x0            0x0210641F           {$5} // wr:1+0, rd:1; fence invalid flush type scoped to tile
(W)     mov (8|M0)               null<1>:ud    r5.0<1;1,0>:ud                   {$5.dst}
(W)     csel (4|M0)   (eq)f0.0   r1.0<1>:w     r1.0<1;0>:w       r1.0<1;0>:w       r1.0<1>:w       
(W)     csel (4|M0)   (eq)f0.0   r1.4<1>:f     r1.4<1;0>:f       r1.4<1;0>:f       r1.4<1>:f        {A@1}
(W)     send.gtwy (1|M0)         null     r255    null:0  0x0            0x02000010           {EOT,A@1} // wr:1+0, rd:0; end of thread
L10744:
        nop                    
        illegal                
        illegal                
        illegal                
        illegal                
        illegal                
        illegal                
        illegal                
        illegal                
        illegal                
        illegal                
        illegal                
