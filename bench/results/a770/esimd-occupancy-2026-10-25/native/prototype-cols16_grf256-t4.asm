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
        send.ugm (1|M0)          r33      r4      null:0  0x0            0x04100580           {$2} // wr:2+0, rd:1; load.ugm.d32.a64
        mov (1|M0)               r30.0<1>:d    r1.0<0;1,0>:uw                   {$0.dst}
        mach (1|M0)              r5.0<1>:d     r0.1<0;1,0>:d     r2.0<0;1,0>:d    {Compacted,$2.src}
        mov (1|M0)               r30.4<1>:d    0:w                              
        add (1|M0)               r5.0<1>:d     r5.0<0;1,0>:d     r1.0<0;1,0>:uw   {I@2}
(W)     mul (1|M0)               acc0.0<1>:ud  r5.0<0;1,0>:ud    0xAAAB:uw              {I@1}
        mach (1|M0)              r6.0<1>:ud    r5.0<0;1,0>:ud    0xAAAAAAAB:ud              {AccWrEn}
        shr (1|M0)               r6.0<1>:ud    r6.0<0;1,0>:ud    0xA:uw              {I@1}
        mul (1|M0)               acc0.0<1>:ud  r6.0<0;1,0>:ud    0x7800:uw              {I@1}
        mul (1|M0)               r30.1<1>:d    r6.0<0;1,0>:d     -1536:w               {Compacted}
        mach (1|M0)              r8.0<1>:ud    r6.0<0;1,0>:ud    0xC7800:ud              {AccWrEn}
        add (1|M0)               r30.1<1>:d    r30.1<0;1,0>:d    r5.0<0;1,0>:d    {Compacted,I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r8.0<0;1,0>:d     2:w               {Compacted,I@3}
        shl (1|M0)               r1.2<1>:ud    r30.1<0;1,0>:ud   0x2:uw              {I@3}
        shr (1|M0)               r30.2<1>:ud   r30.1<0;1,0>:ud   0x5:uw             
        shl (1|M0)               r4.2<1>:d     r7.0<0;1,0>:d     2:w               {Compacted,I@4}
        shr (1|M0)               r2.4<1>:ud    r7.0<0;1,0>:ud    0x1E:uw             
        and (1|M0)               r30.3<1>:ud   r1.2<0;1,0>:ud    0x70:uw              {I@4}
        addc (1|M0)              r5.0<1>:ud    r2.6<0;1,0>:ud    r4.2<0;1,0>:ud   {AccWrEn,I@3}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@3}
        mov (1|M0)               r30.5<1>:d    r5.0<0;1,0>:d                    {I@2}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r10.0<1>:ud   r30.5<0;1,0>:ud   0x300000:ud              {AccWrEn,I@2}
        add3 (1|M0)              r30.6<1>:d    r9.0<0;0>:d       r2.7<0;0>:d       r4.3<0>:d        {I@2}
        mov (1|M0)               r11.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r6.0<0;1,0>:ud    0xA000:uw             
        mov (1|M0)               r31.0<1>:f    r10.0<0;1,0>:f                   {Compacted,I@4}
        mach (1|M0)              r13.0<1>:ud   r6.0<0;1,0>:ud    0xA000:ud              {AccWrEn}
        add3 (1|M0)              r31.1<1>:d    r11.0<0;0>:d      r30.6<0;0>:d      0:w               {I@3}
        mov (1|M0)               r12.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r1.2<1>:d     r13.0<0;1,0>:d    2:w               {Compacted,I@3}
        shl (1|M0)               r1.4<1>:d     r12.0<0;1,0>:d    2:w               {Compacted,I@2}
        shr (1|M0)               r1.3<1>:ud    r12.0<0;1,0>:ud   0x1E:uw             
        addc (1|M0)              r5.0<1>:ud    r3.0<0;1,0>:ud    r1.4<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r1.5<1>:d     r1.2<0;1,0>:d     r1.3<0;1,0>:d    {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r8.0<1>:ud    r3.2<0;1,0>:ud    r1.4<0;1,0>:ud   {AccWrEn}
        mov (1|M0)               r31.2<1>:d    r5.0<0;1,0>:d                    {I@4}
        add3 (1|M0)              r31.3<1>:d    r7.0<0;0>:d       r3.1<0;0>:d       r1.5<0>:d        {I@3}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r6.0<0;1,0>:ud    0xC0:uw             
        mov (1|M0)               r31.4<1>:d    r8.0<0;1,0>:d                    {I@5}
        mach (1|M0)              r11.0<1>:ud   r6.0<0;1,0>:ud    0xC0:ud              {AccWrEn}
        add3 (1|M0)              r31.5<1>:d    r9.0<0;0>:d       r3.3<0;0>:d       r1.5<0>:d        {I@4}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted,F@1}
        shl (1|M0)               r2.3<1>:d     r11.0<0;1,0>:d    2:w               {Compacted,I@3}
        cmp (1|M0)    (lt)f0.0   null<1>:d     r33.0<0;1,0>:d    1:w               {$2.dst}
        shl (1|M0)               r4.2<1>:d     r10.0<0;1,0>:d    2:w               {Compacted,I@3}
        shr (1|M0)               r2.4<1>:ud    r10.0<0;1,0>:ud   0x1E:uw             
        addc (1|M0)              r5.0<1>:ud    r3.4<0;1,0>:ud    r4.2<0;1,0>:ud   {AccWrEn,I@2}
(W)     and (1|M0)               f0.0<1>:uw    f0.0<0;1,0>:uw    0x1:uw             
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@3}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r8.0<1>:ud    r3.6<0;1,0>:ud    r4.2<0;1,0>:ud   {AccWrEn}
        mov (1|M0)               r31.6<1>:d    r5.0<0;1,0>:d                    {I@5}
        add3 (1|M0)              r31.7<1>:d    r7.0<0;0>:d       r3.5<0;0>:d       r4.3<0>:d        {I@3}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r32.0<1>:f    r8.0<0;1,0>:f                    {Compacted,I@4}
        add3 (1|M0)              r32.1<1>:d    r9.0<0;0>:d       r3.7<0;0>:d       r4.3<0>:d        {I@1}
(f0.0.any16h) goto (16|M0)                   L9568                  L9568                
L1080:
        cmp (1|M0)    (lt)f1.1   null<1>:ud    r30.1<0;1,0>:ud   0x280:uw             
        shl (1|M0)               r30.7<1>:ud   r30.1<0;1,0>:ud   0x4:uw             
(W)     and (1|M0)               f1.1<1>:uw    f1.1<0;1,0>:uw    0x1:uw             
(~f1.1.any16h) goto (16|M0)                  L3904                  L3904                
L1144:
        shl (1|M0)               r1.3<1>:d     r30.7<0;1,0>:d    2:w               {I@3}
        cmp (1|M0)    (lt)f1.0   null<1>:ud    r33.0<0;1,0>:ud   0x3:uw             
        shr (1|M0)               r1.2<1>:ud    r30.7<0;1,0>:ud   0x1E:uw             
        mov (1|M0)               r32.2<1>:d    r33.0<0;1,0>:d                  
        mov (1|M0)               r32.3<1>:d    0:w                              
        addc (1|M0)              r5.0<1>:ud    r31.4<0;1,0>:ud   r1.3<0;1,0>:ud   {AccWrEn,I@5}
        or (1|M0)                r1.4<1>:d     r1.2<0;1,0>:d     0:w               {I@4}
(W)     and (1|M0)               f1.0<1>:uw    f1.0<0;1,0>:uw    0x1:uw             
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r30.7<0;1,0>:ud   0x3:uw             
        mov (1|M0)               r32.4<1>:d    r5.0<0;1,0>:d                    {I@5}
        mach (1|M0)              r8.0<1>:ud    r30.7<0;1,0>:ud   0x3:ud              {AccWrEn,F@1}
        add3 (1|M0)              r32.5<1>:d    r6.0<0;0>:d       r31.5<0;0>:d      r1.4<0>:d        {I@4}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r8.0<0;1,0>:d     2:w               {Compacted,I@3}
        shl (1|M0)               r4.2<1>:d     r7.0<0;1,0>:d     2:w               {Compacted,I@2}
        shr (1|M0)               r2.4<1>:ud    r7.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r9.0<1>:ud    r31.0<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r34.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@3}
        add3 (1|M0)              r34.1<1>:d    r10.0<0;0>:d      r31.1<0;0>:d      r4.3<0>:d        {I@1}
(f1.0.any16h) goto (16|M0)                   L1648                  L1648                
L1464:
        add (1|M0)               r1.2<1>:ud    r33.0<0;1,0>:ud   0xFFFFFFFD:ud             
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x2800:uw              {I@1}
        mach (1|M0)              r6.0<1>:ud    r1.2<0;1,0>:ud    0x2800:ud              {AccWrEn}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r6.0<0;1,0>:d     2:w               {Compacted,I@2}
        shl (1|M0)               r4.2<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@2}
        shr (1|M0)               r2.4<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r7.0<1>:ud    r32.4<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r9.0<1>:f     r7.0<0;1,0>:f                    {Compacted,I@3}
        add3 (1|M0)              r9.1<1>:d     r8.0<0;0>:d       r32.5<0;0>:d      r4.3<0>:d        {I@1}
        send.ugm (1|M0)          r39      r9      null:0  0x0            0x0220D580           {A@1,$3} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        goto (16|M0)                         L1648                  L2736                
L1648:
        join (16|M0)                         L2736                                
L1664:
        mov (4|M0)               r4.8<1>:w     0x32103210:v                              
        shl (1|M0)               r1.4<1>:d     r32.2<0;1,0>:d    2:w              
        shl (1|M0)               r1.2<1>:d     r32.3<0;1,0>:d    2:w              
        shr (1|M0)               r1.3<1>:ud    r32.2<0;1,0>:ud   0x1E:uw             
        cmp (1|M0)    (eq)f0.1   null<1>:d     r33.0<0;1,0>:d    2:w              
        mul (4|M0)               r7.0<1>:d     r4.8<1;1,0>:w     12:w               {I@5}
        mul (4|M0)               r8.0<2>:d     r4.8<1;1,0>:w     12:w              
        addc (1|M0)              r5.0<1>:ud    r34.0<0;1,0>:ud   r1.4<0;1,0>:ud   {AccWrEn,I@6}
        or (1|M0)                r1.5<1>:d     r1.2<0;1,0>:d     r1.3<0;1,0>:d    {I@5}
        asr (4|M0)               r8.1<2>:d     r7.0<1;1,0>:d     31:w               {I@4}
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r2.4<1>:d     r5.0<0;1,0>:d                    {I@4}
(W)     and (1|M0)               f0.1<1>:uw    f0.1<0;1,0>:uw    0x1:uw             
        sync.nop                             null                             {Compacted,$3.src}
        addc (4|M0)              r9.0<1>:ud    r8.0<2;1,0>:ud    0x30:ud              {AccWrEn,I@4}
        mov (4|M0)               r35.0<2>:ud   r8.0<2;1,0>:ud                  
        add3 (1|M0)              r2.5<1>:d     r6.0<0;0>:d       r34.1<0;0>:d      r1.5<0>:d        {I@5}
        mov (8|M0)               r12.0<2>:ud   r2.4<0;1,0>:ud                   {I@5}
        mov (4|M0)               r35.1<2>:ud   r8.1<2;1,0>:ud                  
        mov (4|M0)               r10.0<1>:ud   acc0.0<1;1,0>:ud                
        addc (4|M0)              r5.0<1>:ud    r8.0<2;1,0>:ud    0x60:ud              {AccWrEn}
        mov (4|M0)               r36.0<2>:d    r9.0<1;1,0>:d                    {I@7}
        mov (8|M0)               r12.1<2>:ud   r2.5<0;1,0>:ud                   {I@6}
        add3 (4|M0)              r36.1<2>:d    r10.0<1;0>:d      r8.1<4;2>:d       0:w               {I@4}
        mov (4|M0)               r6.0<1>:ud    acc0.0<1;1,0>:ud                
        addc (4|M0)              r11.0<1>:ud   r8.0<2;1,0>:ud    0x90:ud              {AccWrEn}
        mov (4|M0)               r37.0<2>:d    r5.0<1;1,0>:d                    {I@6}
        mov (8|M0)               r18.0<1>:d    r35.1<2;1,0>:d                   {I@4}
        add3 (4|M0)              r37.1<2>:d    r6.0<1;0>:d       r8.1<4;2>:d       0:w               {I@4}
        mov (4|M0)               r7.0<1>:ud    acc0.0<1;1,0>:ud                
        mov (4|M0)               r38.0<2>:d    r11.0<1;1,0>:d                   {I@5}
        addc (8|M0)              r9.0<1>:ud    r12.0<2;1,0>:ud   r35.0<2;1,0>:ud  {AccWrEn,Compacted}
        add3 (4|M0)              r38.1<2>:d    r7.0<1;0>:d       r8.1<4;2>:d       0:w               {I@3}
        mov (8|M0)               r10.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        mov (8|M0)               r14.0<2>:d    r9.0<1;1,0>:d                    {I@3}
        addc (8|M0)              r6.0<1>:ud    r12.0<2;1,0>:ud   r37.0<2;1,0>:ud  {AccWrEn,Compacted,I@3}
        mov (8|M0)               r7.0<1>:d     r37.1<2;1,0>:d                  
        add3 (8|M0)              r5.0<1>:d     r10.0<1;0>:d      r12.1<4;2>:d      r18.0<1>:d       {I@4}
        mov (8|M0)               r19.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        mov (8|M0)               r16.0<2>:d    r6.0<1;1,0>:d                    {I@4}
        mov (8|M0)               r14.1<2>:d    r5.0<1;1,0>:d                    {I@3}
        addc (1|M0)              r11.0<1>:ud   r32.2<0;1,0>:ud   0x1:ud              {AccWrEn}
        add3 (8|M0)              r8.0<1>:d     r19.0<1;0>:d      r12.1<4;2>:d      r7.0<1>:d        {I@4}
        mov (1|M0)               r20.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r32.6<1>:d    r11.0<0;1,0>:d                   {I@3}
        mov (8|M0)               r16.1<2>:d    r8.0<1;1,0>:d                    {I@3}
        add3 (1|M0)              r32.7<1>:d    r20.0<0;0>:d      r32.3<0;0>:d      0:w               {I@3}
        send.dc1 (16|M0)         r39      r14     null:0  0x0            0x082411FF           {A@2,$3} // wr:4+0, rd:2; a64 dword gathering read
(f0.1.any16h) goto (16|M0)                   L2736                  L2736                
L2392:
        shl (1|M0)               r1.4<1>:d     r32.6<0;1,0>:d    2:w              
        shl (1|M0)               r1.2<1>:d     r32.7<0;1,0>:d    2:w               {I@3}
        shr (1|M0)               r1.3<1>:ud    r32.6<0;1,0>:ud   0x1E:uw             
        mov (8|M0)               r15.0<1>:d    r35.1<2;1,0>:d                   {$3.src}
        addc (1|M0)              r5.0<1>:ud    r34.0<0;1,0>:ud   r1.4<0;1,0>:ud   {AccWrEn,I@4}
        or (1|M0)                r1.5<1>:d     r1.2<0;1,0>:d     r1.3<0;1,0>:d    {I@3}
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r2.4<1>:d     r5.0<0;1,0>:d                    {I@3}
        mov (8|M0)               r5.0<1>:d     r37.1<2;1,0>:d                  
        add3 (1|M0)              r2.5<1>:d     r6.0<0;0>:d       r34.1<0;0>:d      r1.5<0>:d        {I@3}
        mov (8|M0)               r7.0<2>:ud    r2.4<0;1,0>:ud                   {I@3}
        mov (8|M0)               r7.1<2>:ud    r2.5<0;1,0>:ud                   {I@2}
        addc (8|M0)              r9.0<1>:ud    r7.0<2;1,0>:ud    r35.0<2;1,0>:ud  {AccWrEn,Compacted,I@1}
        mov (8|M0)               r10.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        addc (8|M0)              r17.0<1>:ud   r7.0<2;1,0>:ud    r37.0<2;1,0>:ud  {AccWrEn,Compacted}
        mov (8|M0)               r11.0<2>:d    r9.0<1;1,0>:d                    {I@3}
        add3 (8|M0)              r16.0<1>:d    r10.0<1;0>:d      r7.1<4;2>:d       r15.0<1>:d       {I@3}
        mov (8|M0)               r18.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        mov (8|M0)               r13.0<2>:d    r17.0<1;1,0>:d                   {I@4}
        mov (8|M0)               r11.1<2>:d    r16.0<1;1,0>:d                   {I@3}
        add3 (8|M0)              r6.0<1>:d     r18.0<1;0>:d      r7.1<4;2>:d       r5.0<1>:d        {I@3}
        mov (8|M0)               r13.1<2>:d    r6.0<1;1,0>:d                    {I@1}
        send.dc1 (16|M0)         r41      r11     null:0  0x0            0x082411FF           {A@1,$4} // wr:4+0, rd:2; a64 dword gathering read
        goto (16|M0)                         L2736                  L2920                
L2736:
        join (16|M0)                         L2920                                
L2752:
        add (1|M0)               r1.2<1>:d     r33.0<0;1,0>:d    -2:w               {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x2800:uw              {I@1}
        mach (1|M0)              r6.0<1>:ud    r1.2<0;1,0>:ud    0x2800:ud              {AccWrEn}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r6.0<0;1,0>:d     2:w               {Compacted,I@2}
        shl (1|M0)               r4.2<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@2}
        shr (1|M0)               r2.4<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r7.0<1>:ud    r32.4<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        sync.nop                             null                             {Compacted,$3.src}
        mov (1|M0)               r9.0<1>:f     r7.0<0;1,0>:f                    {Compacted,I@3}
        add3 (1|M0)              r9.1<1>:d     r8.0<0;0>:d       r32.5<0;0>:d      r4.3<0>:d        {I@1}
        send.ugm (1|M0)          r41      r9      null:0  0x0            0x0220D580           {A@1,$4} // wr:1+0, rd:2; load.ugm.d32x16t.a64
L2920:
        join (16|M0)                         L3904                                
L2936:
        add (1|M0)               r1.2<1>:d     r33.0<0;1,0>:d    -1:w               {Compacted}
        sync.nop                             null                             {Compacted,$4.src}
        mov (1|M0)               r14.5<1>:f    r39.7<0;1,0>:f                   {$3.dst}
        mov (1|M0)               r12.3<1>:f    r39.1<0;1,0>:f                  
        mov (1|M0)               r13.4<1>:ud   r39.4<0;1,0>:ud                 
        mov (1|M0)               r15.6<1>:f    r40.2<0;1,0>:f                  
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x2800:uw              {I@2}
        mov (1|M0)               r39.7<1>:f    r40.5<0;1,0>:f                  
        mov (1|M0)               r12.0<1>:ud   r39.0<0;1,0>:ud                 
        mach (1|M0)              r6.0<1>:ud    r1.2<0;1,0>:ud    0x2800:ud              {AccWrEn}
        mov (1|M0)               r12.6<1>:f    r39.2<0;1,0>:f                  
        mov (1|M0)               r13.1<1>:f    r39.3<0;1,0>:f                  
        mov (1|M0)               r13.7<1>:f    r39.5<0;1,0>:f                  
        mov (1|M0)               r14.2<1>:f    r39.6<0;1,0>:f                  
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r6.0<0;1,0>:d     2:w               {Compacted,I@2}
        mov (1|M0)               r15.0<1>:ud   r40.0<0;1,0>:ud                 
        mov (1|M0)               r15.3<1>:f    r40.1<0;1,0>:f                  
        mov (1|M0)               r12.1<1>:f    r41.0<0;1,0>:f                   {Compacted,$4.dst}
        shl (1|M0)               r4.2<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@3}
        shr (1|M0)               r2.4<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        mov (1|M0)               r12.4<1>:f    r41.1<0;1,0>:f                  
        mov (1|M0)               r12.7<1>:f    r41.2<0;1,0>:f                  
        mov (1|M0)               r13.2<1>:f    r41.3<0;1,0>:f                  
        addc (1|M0)              r7.0<1>:ud    r32.4<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        mov (1|M0)               r13.5<1>:f    r41.4<0;1,0>:f                  
        mov (1|M0)               r14.0<1>:f    r41.5<0;1,0>:f                   {Compacted}
        mov (1|M0)               r14.3<1>:f    r41.6<0;1,0>:f                  
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r9.0<1>:d     r7.0<0;1,0>:d                    {I@3}
        addc (1|M0)              r5.0<1>:ud    r34.0<0;1,0>:ud   0x80:ud              {AccWrEn}
        mov (1|M0)               r14.6<1>:f    r41.7<0;1,0>:f                  
        mov (1|M0)               r15.1<1>:f    r42.0<0;1,0>:f                   {Compacted}
        add3 (1|M0)              r9.1<1>:d     r8.0<0;0>:d       r32.5<0;0>:d      r4.3<0>:d        {I@3}
        mov (1|M0)               r15.4<1>:f    r42.1<0;1,0>:f                  
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r15.7<1>:f    r42.2<0;1,0>:f                  
        mov (1|M0)               r39.1<1>:f    r40.3<0;1,0>:f                  
        send.ugm (1|M0)          r10      r9      null:0  0x0            0x0220D580           {A@2,$5} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        mov (1|M0)               r39.4<1>:ud   r40.4<0;1,0>:ud                 
        mov (1|M0)               r40.2<1>:f    r40.6<0;1,0>:f                  
        mov (1|M0)               r40.5<1>:f    r40.7<0;1,0>:f                  
        mov (1|M0)               r39.2<1>:f    r42.3<0;1,0>:f                  
        mov (1|M0)               r39.5<1>:f    r42.4<0;1,0>:f                  
        mov (1|M0)               r40.0<1>:f    r42.5<0;1,0>:f                   {Compacted}
        mov (1|M0)               r7.0<1>:d     r5.0<0;1,0>:d                   
        mov (1|M0)               r40.3<1>:f    r42.6<0;1,0>:f                  
        add3 (1|M0)              r7.1<1>:d     r6.0<0;0>:d       r34.1<0;0>:d      0:w               {I@3}
        mov (1|M0)               r40.6<1>:f    r42.7<0;1,0>:f                  
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
        mov (1|M0)               r39.0<1>:f    r11.2<0;1,0>:f                   {Compacted}
        mov (1|M0)               r39.3<1>:ud   r11.3<0;1,0>:ud                 
        mov (1|M0)               r39.6<1>:f    r11.4<0;1,0>:f                  
        mov (1|M0)               r40.1<1>:f    r11.5<0;1,0>:f                  
        mov (1|M0)               r40.4<1>:f    r11.6<0;1,0>:f                   {I@6}
        mov (1|M0)               r40.7<1>:ud   r11.7<0;1,0>:ud                 
        send.ugm (1|M0)          null     r34     r12:4   0x0            0x0200D784           {A@3,$6} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r7      r39:2   0x0            0x0200C784           {A@1,$7} // wr:1+2, rd:0; store.ugm.d64x8t.a64
L3904:
        join (16|M0)                         L9568                                
L3920:
        shl (1|M0)               r1.3<1>:d     r30.2<0;1,0>:d    7:w              
        shr (1|M0)               r1.2<1>:ud    r30.2<0;1,0>:ud   0x19:uw             
        shl (1|M0)               r33.1<1>:d    r30.3<0;1,0>:d    2:w              
        shl (1|M0)               r5.3<1>:d     r30.4<0;1,0>:d    2:w              
        shr (1|M0)               r5.4<1>:ud    r30.3<0;1,0>:ud   0x1E:uw             
        shl (1|M0)               r4.2<1>:d     r1.3<0;1,0>:d     2:w               {I@5}
        or (1|M0)                r1.4<1>:d     r1.2<0;1,0>:d     0:w               {I@5}
        shl (1|M0)               r1.2<1>:d     r30.0<0;1,0>:d    5:w               {Compacted}
        shr (1|M0)               r2.4<1>:ud    r1.3<0;1,0>:ud    0x1E:uw             
        or (1|M0)                r33.2<1>:d    r5.3<0;1,0>:d     r5.4<0;1,0>:d    {I@5}
        addc (1|M0)              r5.0<1>:ud    r30.5<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@5}
        shl (1|M0)               r2.3<1>:d     r1.4<0;1,0>:d     2:w               {I@5}
        shl (1|M0)               r34.2<1>:d    r30.2<0;1,0>:d    2:w               {$6.src}
        shr (1|M0)               r4.2<1>:ud    r1.2<0;1,0>:ud    0x1E:uw              {I@6}
        shl (1|M0)               r34.6<1>:d    r30.2<0;1,0>:d    7:w              
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        sync.nop                             null                             {Compacted,$7.src}
        addc (1|M0)              r7.0<1>:ud    r5.0<0;1,0>:ud    r33.1<0;1,0>:ud  {AccWrEn,I@6}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@6}
        mov (1|M0)               r34.7<1>:d    0:w                              
        sel (1|M0)    (lt)f0.0   r43.2<1>:ud   r33.0<0;1,0>:ud   0x4:uw             
        mov (1|M0)               r43.3<1>:d    0:w                              
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x1800:uw             
        add3 (1|M0)              r5.2<1>:d     r6.0<0;0>:d       r30.6<0;0>:d      r4.3<0>:d        {I@6}
        mov (1|M0)               r33.7<1>:f    0.0:f                               {Compacted}
        mach (1|M0)              r10.0<1>:ud   r1.2<0;1,0>:ud    0x1800:ud              {AccWrEn}
        mov (1|M0)               r43.4<2>:f    0.0:f                              
        mov (1|M0)               r43.5<2>:f    0.0:f                              
        add3 (1|M0)              r1.6<1>:d     r8.0<0;0>:d       r5.2<0;0>:d       r33.2<0>:d       {I@2}
        shl (1|M0)               r33.5<1>:d    r1.2<0;1,0>:d     2:w              
        sync.nop                             null                             {Compacted,$4.src}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted,$3.src}
        shl (1|M0)               r1.7<1>:d     r10.0<0;1,0>:d    2:w               {Compacted,I@4}
        or (1|M0)                r33.6<1>:d    r4.2<0;1,0>:d     0:w              
        shl (1|M0)               r2.4<1>:d     r9.0<0;1,0>:d     2:w               {Compacted,I@3}
        shr (1|M0)               r2.3<1>:ud    r9.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r6.0<1>:ud    r7.0<0;1,0>:ud    r2.4<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r2.5<1>:d     r1.7<0;1,0>:d     r2.3<0;1,0>:d    {I@2}
        and (1|M0)               r2.3<1>:d     r1.3<0;1,0>:d     1920:w               {Compacted}
        mov (1|M0)               r43.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@3}
        mov (1|M0)               r11.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r33.3<1>:d    r2.3<0;1,0>:d     2:w               {Compacted,I@2}
        addc (1|M0)              r5.0<1>:ud    r43.0<0;1,0>:ud   0x6000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r43.1<1>:d    r11.0<0;0>:d      r1.6<0;0>:d       r2.5<0>:d        {I@3}
        shr (1|M0)               r2.5<1>:ud    r2.3<0;1,0>:ud    0x1E:uw             
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r43.0<0;1,0>:ud   0xC000:ud              {AccWrEn}
        mov (1|M0)               r44.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@5}
        send.ugm (1|M0)          r137     r43     null:0  0x0            0x0220D580           {A@4,$8} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        or (1|M0)                r33.4<1>:d    r2.5<0;1,0>:d     0:w               {I@3}
        add3 (1|M0)              r44.1<1>:d    r8.0<0;0>:d       r43.1<0;0>:d      0:w               {I@3}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r43.0<0;1,0>:ud   0x12000:ud              {AccWrEn}
        mov (1|M0)               r45.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@5}
        send.ugm (1|M0)          r135     r44     null:0  0x0            0x0220D580           {A@2,$9} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r45.1<1>:d    r10.0<0;0>:d      r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r43.0<0;1,0>:ud   0x18000:ud              {AccWrEn}
        mov (1|M0)               r46.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r133     r45     null:0  0x0            0x0220D580           {A@2,$10} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r46.1<1>:d    r7.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r43.0<0;1,0>:ud   0x1E000:ud              {AccWrEn}
        mov (1|M0)               r47.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r131     r46     null:0  0x0            0x0220D580           {A@2,$11} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r47.1<1>:d    r8.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r43.0<0;1,0>:ud   0x24000:ud              {AccWrEn}
        mov (1|M0)               r48.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r129     r47     null:0  0x0            0x0220D580           {A@2,$12} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r48.1<1>:d    r10.0<0;0>:d      r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r43.0<0;1,0>:ud   0x2A000:ud              {AccWrEn}
        mov (1|M0)               r49.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r127     r48     null:0  0x0            0x0220D580           {A@2,$13} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r49.1<1>:d    r7.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r43.0<0;1,0>:ud   0x30000:ud              {AccWrEn}
        mov (1|M0)               r50.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r125     r49     null:0  0x0            0x0220D580           {A@2,$14} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r50.1<1>:d    r8.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r43.0<0;1,0>:ud   0x36000:ud              {AccWrEn}
        mov (1|M0)               r51.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r123     r50     null:0  0x0            0x0220D580           {A@2,$15} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r51.1<1>:d    r10.0<0;0>:d      r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r43.0<0;1,0>:ud   0x3C000:ud              {AccWrEn}
        mov (1|M0)               r52.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r121     r51     null:0  0x0            0x0220D580           {A@2,$0} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r52.1<1>:d    r7.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r43.0<0;1,0>:ud   0x42000:ud              {AccWrEn}
        mov (1|M0)               r53.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r119     r52     null:0  0x0            0x0220D580           {A@2,$1} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r53.1<1>:d    r8.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r43.0<0;1,0>:ud   0x48000:ud              {AccWrEn}
        mov (1|M0)               r54.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r117     r53     null:0  0x0            0x0220D580           {A@2,$2} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r54.1<1>:d    r10.0<0;0>:d      r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r43.0<0;1,0>:ud   0x4E000:ud              {AccWrEn}
        mov (1|M0)               r55.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r115     r54     null:0  0x0            0x0220D580           {A@2,$4} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r55.1<1>:d    r7.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r43.0<0;1,0>:ud   0x54000:ud              {AccWrEn}
        mov (1|M0)               r56.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r113     r55     null:0  0x0            0x0220D580           {A@2,$5} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r56.1<1>:d    r8.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r43.0<0;1,0>:ud   0x5A000:ud              {AccWrEn}
        mov (1|M0)               r57.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r111     r56     null:0  0x0            0x0220D580           {A@2,$6} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r57.1<1>:d    r10.0<0;0>:d      r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r43.0<0;1,0>:ud   0x60000:ud              {AccWrEn}
        mov (1|M0)               r58.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r109     r57     null:0  0x0            0x0220D580           {A@2,$7} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r58.1<1>:d    r7.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r43.0<0;1,0>:ud   0x66000:ud              {AccWrEn}
        mov (1|M0)               r59.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r107     r58     null:0  0x0            0x0220D580           {A@2,$3} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r59.1<1>:d    r8.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r43.0<0;1,0>:ud   0x6C000:ud              {AccWrEn}
        mov (1|M0)               r60.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r105     r59     null:0  0x0            0x0220D580           {A@2,$8} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r60.1<1>:d    r10.0<0;0>:d      r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r43.0<0;1,0>:ud   0x72000:ud              {AccWrEn}
        mov (1|M0)               r61.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r103     r60     null:0  0x0            0x0220D580           {A@2,$9} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r61.1<1>:d    r7.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r43.0<0;1,0>:ud   0x78000:ud              {AccWrEn}
        mov (1|M0)               r62.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r101     r61     null:0  0x0            0x0220D580           {A@2,$10} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r62.1<1>:d    r8.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r43.0<0;1,0>:ud   0x7E000:ud              {AccWrEn}
        mov (1|M0)               r63.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r99      r62     null:0  0x0            0x0220D580           {A@2,$11} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r63.1<1>:d    r10.0<0;0>:d      r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r43.0<0;1,0>:ud   0x84000:ud              {AccWrEn}
        mov (1|M0)               r64.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r97      r63     null:0  0x0            0x0220D580           {A@2,$12} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r64.1<1>:d    r7.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r43.0<0;1,0>:ud   0x8A000:ud              {AccWrEn}
        mov (1|M0)               r65.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r95      r64     null:0  0x0            0x0220D580           {A@2,$13} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r65.1<1>:d    r8.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r43.0<0;1,0>:ud   0x90000:ud              {AccWrEn}
        mov (1|M0)               r66.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r93      r65     null:0  0x0            0x0220D580           {A@2,$14} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r66.1<1>:d    r10.0<0;0>:d      r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r43.0<0;1,0>:ud   0x96000:ud              {AccWrEn}
        mov (1|M0)               r67.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r91      r66     null:0  0x0            0x0220D580           {A@2,$15} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r67.1<1>:d    r7.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r43.0<0;1,0>:ud   0x9C000:ud              {AccWrEn}
        mov (1|M0)               r68.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r89      r67     null:0  0x0            0x0220D580           {A@2,$0} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r68.1<1>:d    r8.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r43.0<0;1,0>:ud   0xA2000:ud              {AccWrEn}
        mov (1|M0)               r69.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r87      r68     null:0  0x0            0x0220D580           {A@2,$1} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r69.1<1>:d    r10.0<0;0>:d      r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r43.0<0;1,0>:ud   0xA8000:ud              {AccWrEn}
        mov (1|M0)               r70.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r85      r69     null:0  0x0            0x0220D580           {A@2,$2} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r70.1<1>:d    r7.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r43.0<0;1,0>:ud   0xAE000:ud              {AccWrEn}
        mov (1|M0)               r71.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r83      r70     null:0  0x0            0x0220D580           {A@2,$4} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r71.1<1>:d    r8.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r43.0<0;1,0>:ud   0xB4000:ud              {AccWrEn}
        mov (1|M0)               r72.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r81      r71     null:0  0x0            0x0220D580           {A@2,$5} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r72.1<1>:d    r10.0<0;0>:d      r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r43.0<0;1,0>:ud   0xBA000:ud              {AccWrEn}
        mov (1|M0)               r73.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@4}
        send.ugm (1|M0)          r79      r72     null:0  0x0            0x0220D580           {A@2,$6} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r73.1<1>:d    r7.0<0;0>:d       r43.1<0;0>:d      0:w               {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r74.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@3}
        addc (1|M0)              r6.0<1>:ud    r31.6<0;1,0>:ud   r34.2<0;1,0>:ud  {AccWrEn,F@2}
        shr (1|M0)               r5.0<1>:ud    r30.2<0;1,0>:ud   0x1E:uw              {F@1}
        send.ugm (1|M0)          r77      r73     null:0  0x0            0x0220D580           {A@4,$7} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r74.1<1>:d    r8.0<0;0>:d       r43.1<0;0>:d      0:w               {I@3}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        or (1|M0)                r34.3<1>:d    r5.0<0;1,0>:d     0:w               {Compacted,I@3}
        mov (1|M0)               r34.4<1>:d    r6.0<0;1,0>:d                   
        send.ugm (1|M0)          r75      r74     null:0  0x0            0x0220D580           {A@4,$3} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r34.5<1>:d    r7.0<0;0>:d       r31.7<0;0>:d      r34.3<0>:d       {I@2}
L6632:
        mul (1|M0)               acc0.0<1>:ud  r43.4<0;1,0>:ud   0x2800:uw             
        mul (1|M0)               r1.2<1>:d     r43.5<0;1,0>:d    10240:w              
        mul (1|M0)               r2.3<1>:d     r43.5<0;1,0>:d    48:w              
        mach (1|M0)              r6.0<1>:ud    r43.4<0;1,0>:ud   0x2800:ud              {AccWrEn}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        add (1|M0)               r2.4<1>:d     r1.2<0;1,0>:d     r6.0<0;1,0>:d    {I@2}
        shl (1|M0)               r1.2<1>:d     r43.4<0;1,0>:d    8:w              
        shl (1|M0)               r4.4<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@3}
        shr (1|M0)               r4.3<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        shl (1|M0)               r4.2<1>:d     r2.4<0;1,0>:d     2:w               {I@4}
        addc (1|M0)              r7.0<1>:ud    r31.2<0;1,0>:ud   r4.4<0;1,0>:ud   {AccWrEn,I@3}
        or (1|M0)                r4.5<1>:d     r4.2<0;1,0>:d     r4.3<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r7.0<0;1,0>:ud    0x2000:ud              {AccWrEn,I@3}
        add3 (1|M0)              r7.2<1>:d     r8.0<0;0>:d       r31.3<0;0>:d      r4.5<0>:d        {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r11.0<1>:ud   r9.0<0;1,0>:ud    r33.3<0;1,0>:ud  {AccWrEn,I@3}
        add3 (1|M0)              r7.4<1>:d     r10.0<0;0>:d      r7.2<0;0>:d       0:w               {I@2}
        mov (1|M0)               r12.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r13.0<1>:ud   r11.0<0;1,0>:ud   r33.5<0;1,0>:ud  {AccWrEn,I@3}
        add3 (1|M0)              r1.3<1>:d     r12.0<0;0>:d      r7.4<0;0>:d       r33.4<0>:d       {I@2}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r6.0<1>:d     r13.0<0;1,0>:d                   {I@3}
        mul (1|M0)               acc0.0<1>:ud  r43.4<0;1,0>:ud   0x30:uw             
        add3 (1|M0)              r6.1<1>:d     r5.0<0;0>:d       r1.3<0;0>:d       r33.6<0>:d       {I@3}
        mach (1|M0)              r19.0<1>:ud   r43.4<0;1,0>:ud   0x30:ud              {AccWrEn}
        sync.nop                             null                             {Compacted,$3.src}
        send.ugm (1|M0)          r14      r6      null:0  0x0            0x0240E580           {A@2,$8} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        mov (1|M0)               r18.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        add (1|M0)               r4.3<1>:d     r2.3<0;1,0>:d     r19.0<0;1,0>:d   {Compacted,I@2}
        shl (1|M0)               r7.5<1>:d     r18.0<0;1,0>:d    2:w               {Compacted,I@2}
        shr (1|M0)               r7.4<1>:ud    r18.0<0;1,0>:ud   0x1E:uw             
        shl (1|M0)               r7.3<1>:d     r4.3<0;1,0>:d     2:w               {Compacted,I@3}
        addc (1|M0)              r5.0<1>:ud    r34.4<0;1,0>:ud   r7.5<0;1,0>:ud   {AccWrEn,I@3}
        or (1|M0)                r7.6<1>:d     r7.3<0;1,0>:d     r7.4<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r10.0<1>:d    r5.0<0;1,0>:d                    {I@3}
        and (1|M0)               r5.0<1>:d     r1.2<0;1,0>:d     256:w               {Compacted}
        add3 (1|M0)              r10.1<1>:d    r8.0<0;0>:d       r34.5<0;0>:d      r7.6<0>:d        {I@3}
        mad (1|M0)               r8.0<1>:d     r5.0<0;0>:d       r1.0<0;0>:uw      64:w               {I@2}
        send.ugm (1|M0)          r6       r10     null:0  0x0            0x04100580           {A@2,$9} // wr:2+0, rd:1; load.ugm.d32.a64
        mad (16|M0)              acc0.0<1>:f   r33.7<0;0>:f      r137.0<1;0>:f     r14.0<0>:f       {Compacted,$8.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r135.0<1;0>:f     r14.1<0>:f       {Compacted,$9.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r133.0<1;0>:f     r14.2<0>:f       {Compacted,$10.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r131.0<1;0>:f     r14.3<0>:f       {Compacted,$11.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r129.0<1;0>:f     r14.4<0>:f       {Compacted,$12.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r127.0<1;0>:f     r14.5<0>:f       {Compacted,$13.dst}
        mul (1|M0)               r6.0<1>:f     r6.0<0;1,0>:f     1.442695e+00:f               {$9.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r125.0<1;0>:f     r14.6<0>:f       {Compacted,$14.dst}
        math.exp (1|M0)          r6.0<1>:f     r6.0<0;1,0>:f                    {@2,$10}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r123.0<1;0>:f     r14.7<0>:f       {Compacted,$15.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r121.0<1;0>:f     r15.0<0>:f       {Compacted,$0.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r119.0<1;0>:f     r15.1<0>:f       {Compacted,$1.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r117.0<1;0>:f     r15.2<0>:f       {Compacted,$2.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r115.0<1;0>:f     r15.3<0>:f       {Compacted,$4.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r113.0<1;0>:f     r15.4<0>:f       {Compacted,$5.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r111.0<1;0>:f     r15.5<0>:f       {Compacted,$6.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r109.0<1;0>:f     r15.6<0>:f       {Compacted,$7.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r107.0<1;0>:f     r15.7<0>:f       {Compacted,$3.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r105.0<1;0>:f     r16.0<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r103.0<1;0>:f     r16.1<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r101.0<1;0>:f     r16.2<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r99.0<1;0>:f      r16.3<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r97.0<1;0>:f      r16.4<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r95.0<1;0>:f      r16.5<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r93.0<1;0>:f      r16.6<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r91.0<1;0>:f      r16.7<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r89.0<1;0>:f      r17.0<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r87.0<1;0>:f      r17.1<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r85.0<1;0>:f      r17.2<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r83.0<1;0>:f      r17.3<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r81.0<1;0>:f      r17.4<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r79.0<1;0>:f      r17.5<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r77.0<1;0>:f      r17.6<0>:f       {Compacted}
        mad (16|M0)              r11.0<1>:f    acc0.0<1;0>:f     r75.0<1;0>:f      r17.7<0>:f       {Compacted}
        send.slm (1|M0)          null     r8      r11:2   0x0            0x0200C704           {A@1,$12} // wr:1+2, rd:0; store.slm.d64x8t.a32
(W)     send.slm (1|M0)          r9       r0      null:0  0x0            0x0210001F           {$11} // wr:1+0, rd:1; fence.slm.none.group
(W)     mov (1|M0)               r10.2<1>:ud   0x0:ud                             
(W)     mov (2|M0)               r10.10<1>:ub  r0.11<0;1,0>:ub                 
(W)     mov (8|M0)               null<1>:ud    r9.0<1;1,0>:ud                   {$11.dst}
(W)     send.gtwy (1|M0)         null     r10     null:0  0x0            0x02000004           {A@2,$13} // wr:1+0, rd:0; signal barrier
(W)     sync.bar                             null                            
        addc (1|M0)              r13.0<1>:ud   r7.0<0;1,0>:ud    0x4000:ud              {AccWrEn}
        shl (1|M0)               r1.6<1>:d     r34.6<0;1,0>:d    2:w              
        shl (1|M0)               r1.4<1>:d     r34.7<0;1,0>:d    2:w              
        shr (1|M0)               r1.5<1>:ud    r34.6<0;1,0>:ud   0x1E:uw             
        send.slm (1|M0)          r18      r5      null:0  0x0            0x0280E700           {$14} // wr:1+0, rd:8; load.slm.d64x32t.a32
        mov (1|M0)               r26.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        sync.nop                             null                             {Compacted,$12.src}
        addc (1|M0)              r8.0<1>:ud    r13.0<0;1,0>:ud   r1.6<0;1,0>:ud   {AccWrEn,I@4}
        or (1|M0)                r1.7<1>:d     r1.4<0;1,0>:d     r1.5<0;1,0>:d    {I@3}
        add3 (1|M0)              r1.3<1>:d     r26.0<0;0>:d      r7.2<0;0>:d       0:w               {I@3}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        sync.nop                             null                             {Compacted,$13.src}
        addc (1|M0)              r10.0<1>:ud   r8.0<0;1,0>:ud    r33.1<0;1,0>:ud  {AccWrEn,I@4}
        add3 (1|M0)              r2.4<1>:d     r9.0<0;0>:d       r1.3<0;0>:d       r1.7<0>:d        {I@2}
        mov (1|M0)               r11.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r7.0<1>:ud    r32.0<0;1,0>:ud   r34.2<0;1,0>:ud  {AccWrEn}
        sync.nop                             null                             {Compacted,$14.src}
        mov (1|M0)               r5.0<1>:d     r10.0<0;1,0>:d                   {I@4}
        add (16|M0)              acc2.0<1>:f   r18.0<1;1,0>:f    r20.0<1;1,0>:f   {Compacted,$14.dst}
        add3 (1|M0)              r5.1<1>:d     r11.0<0;0>:d      r2.4<0;0>:d       r33.2<0>:d       {I@3}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r8.0<1>:ud    r7.0<0;1,0>:ud    r7.5<0;1,0>:ud   {AccWrEn,I@4}
        add (16|M0)              acc2.0<1>:f   acc2.0<1;1,0>:f   r22.0<1;1,0>:f   {Compacted}
        send.ugm (1|M0)          r28      r5      null:0  0x0            0x0220D580           {A@3,$3} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        add3 (1|M0)              r1.3<1>:d     r9.0<0;0>:d       r32.1<0;0>:d      r34.3<0>:d       {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r12.0<1>:d    r8.0<0;1,0>:d                    {I@3}
        add (16|M0)              r26.0<1>:f    acc2.0<1;1,0>:f   r24.0<1;1,0>:f   {Compacted}
        addc (1|M0)              r5.0<1>:ud    r43.4<0;1,0>:ud   0x1:ud              {AccWrEn,$3.src}
        add3 (1|M0)              r12.1<1>:d    r10.0<0;0>:d      r1.3<0;0>:d       r7.6<0>:d        {I@3}
        mov (1|M0)               r43.6<1>:d    r5.0<0;1,0>:d                    {I@2}
        send.ugm (1|M0)          r11      r12     null:0  0x0            0x04100580           {A@2,$8} // wr:2+0, rd:1; load.ugm.d32.a64
        sync.allwr                           ($3,$10)                
        mad (16|M0)              acc2.0<1>:f   r28.0<1;0>:f      -r26.0<1;0>:f     r6.0<0>:f        {Compacted,F@1}
        mul (16|M0)              acc2.0<1>:f   acc2.0<1;1,0>:f   r11.0<0;1,0>:f   {Compacted,$8.dst}
        mul (16|M0)              acc4.0<1>:f   r14.4<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r14.0<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r14.1<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r129.0<1>:f   acc4.0<1;0>:f     r129.0<1;0>:f     r6.0<0>:f        {Compacted,$4.src}
        mul (16|M0)              acc4.0<1>:f   r15.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r22.0<1>:f    r14.2<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r137.0<1>:f   acc6.0<1;0>:f     r137.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r115.0<1>:f   acc4.0<1;0>:f     r115.0<1;0>:f     r6.0<0>:f        {Compacted}
        mul (16|M0)              acc4.0<1>:f   r16.2<0;1,0>:f    acc2.0<1;1,0>:f 
        sync.nop                             null                             {Compacted,$7.src}
        mad (16|M0)              r135.0<1>:f   r20.0<1;0>:f      r135.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        sync.nop                             null                             {Compacted,$6.src}
        mad (16|M0)              r133.0<1>:f   r22.0<1;0>:f      r133.0<1;0>:f     r6.0<0>:f        {Compacted,F@5}
        mad (16|M0)              r101.0<1>:f   acc4.0<1;0>:f     r101.0<1;0>:f     r6.0<0>:f        {Compacted}
        mul (16|M0)              acc4.0<1>:f   r17.1<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r24.0<1>:f    r14.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r9.0<1>:f     r14.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r87.0<1>:f    acc4.0<1;0>:f     r87.0<1;0>:f      r6.0<0>:f        {Compacted}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mul (16|M0)              r11.0<1>:f    r14.6<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc4.0<1>:f   r14.7<0;1,0>:f    acc2.0<1;1,0>:f 
        add3 (1|M0)              r43.7<1>:d    r7.0<0;0>:d       r43.5<0;0>:d      0:w               {I@1}
        mul (16|M0)              acc6.0<1>:f   r15.0<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc0.0<1>:f   r15.1<0;1,0>:f    acc2.0<1;1,0>:f 
        cmp (1|M0)    (eq)f0.1   null<1>:d     r43.7<0;1,0>:d    r43.3<0;1,0>:d   {I@1}
        sync.nop                             null                             {Compacted,$5.src}
        mad (16|M0)              r131.0<1>:f   r24.0<1;0>:f      r131.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        sync.nop                             null                             {Compacted,$2.src}
        mad (16|M0)              r127.0<1>:f   r9.0<1;0>:f       r127.0<1;0>:f     r6.0<0>:f        {Compacted,F@7}
        sync.nop                             null                             {Compacted,$1.src}
        mad (16|M0)              r125.0<1>:f   r11.0<1;0>:f      r125.0<1;0>:f     r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r123.0<1>:f   acc4.0<1;0>:f     r123.0<1;0>:f     r6.0<0>:f        {Compacted,$0.src}
        mad (16|M0)              r121.0<1>:f   acc6.0<1;0>:f     r121.0<1;0>:f     r6.0<0>:f        {Compacted,$15.src}
        mad (16|M0)              r119.0<1>:f   acc0.0<1;0>:f     r119.0<1;0>:f     r6.0<0>:f        {Compacted}
        mul (16|M0)              acc0.0<1>:f   r15.2<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc4.0<1>:f   r15.4<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r15.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r15.6<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r15.7<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r22.0<1>:f    r16.0<0;1,0>:f    acc2.0<1;1,0>:f 
(f0.1)  cmp (1|M0)    (eq)f0.1   null<1>:d     r43.6<0;1,0>:d    r43.2<0;1,0>:d  
        mad (16|M0)              r117.0<1>:f   acc0.0<1;0>:f     r117.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r113.0<1>:f   acc4.0<1;0>:f     r113.0<1;0>:f     r6.0<0>:f        {Compacted,$8.src}
        mad (16|M0)              r111.0<1>:f   acc6.0<1;0>:f     r111.0<1;0>:f     r6.0<0>:f        {Compacted,$9.src}
        mad (16|M0)              r109.0<1>:f   r18.0<1;0>:f      r109.0<1;0>:f     r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r107.0<1>:f   r20.0<1;0>:f      r107.0<1;0>:f     r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r105.0<1>:f   r22.0<1;0>:f      r105.0<1;0>:f     r6.0<0>:f        {Compacted,F@6}
        mul (16|M0)              acc0.0<1>:f   r16.1<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc4.0<1>:f   r16.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r16.4<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r16.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r16.6<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r22.0<1>:f    r16.7<0;1,0>:f    acc2.0<1;1,0>:f 
(W)     and (1|M0)               f0.0<1>:uw    f0.1<0;1,0>:uw    0x1:uw             
        mad (16|M0)              r103.0<1>:f   acc0.0<1;0>:f     r103.0<1;0>:f     r6.0<0>:f        {Compacted}
        mad (16|M0)              r99.0<1>:f    acc4.0<1;0>:f     r99.0<1;0>:f      r6.0<0>:f        {Compacted}
        mad (16|M0)              r97.0<1>:f    acc6.0<1;0>:f     r97.0<1;0>:f      r6.0<0>:f        {Compacted}
        mad (16|M0)              r95.0<1>:f    r18.0<1;0>:f      r95.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r93.0<1>:f    r20.0<1;0>:f      r93.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r91.0<1>:f    r22.0<1;0>:f      r91.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mul (16|M0)              acc0.0<1>:f   r17.0<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc4.0<1>:f   r17.2<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc6.0<1>:f   r17.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r17.4<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r17.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r22.0<1>:f    r17.6<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc2.0<1>:f   r17.7<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r89.0<1>:f    acc0.0<1;0>:f     r89.0<1;0>:f      r6.0<0>:f        {Compacted}
        mad (16|M0)              r85.0<1>:f    acc4.0<1;0>:f     r85.0<1;0>:f      r6.0<0>:f        {Compacted}
        mad (16|M0)              r83.0<1>:f    acc6.0<1;0>:f     r83.0<1;0>:f      r6.0<0>:f        {Compacted}
        mad (16|M0)              r81.0<1>:f    r18.0<1;0>:f      r81.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r79.0<1>:f    r20.0<1;0>:f      r79.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r77.0<1>:f    r22.0<1;0>:f      r77.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r75.0<1>:f    acc2.0<1;0>:f     r75.0<1;0>:f      r6.0<0>:f        {Compacted}
(~f0.0.any16h) goto (16|M0)                  L9520                  L9520                
L8936:
        send.ugm (1|M0)          null     r43     r137:2  0x0            0x0200C784           {$12} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r44     r135:2  0x0            0x0200C784           {$7} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r45     r133:2  0x0            0x0200C784           {$6} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r46     r131:2  0x0            0x0200C784           {$5} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r47     r129:2  0x0            0x0200C784           {$4} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r48     r127:2  0x0            0x0200C784           {$2} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r49     r125:2  0x0            0x0200C784           {$1} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r50     r123:2  0x0            0x0200C784           {$0} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r51     r121:2  0x0            0x0200C784           {$15} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r52     r119:2  0x0            0x0200C784           {$14} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r53     r117:2  0x0            0x0200C784           {$13} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r54     r115:2  0x0            0x0200C784           {$3} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r55     r113:2  0x0            0x0200C784           {$8} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r56     r111:2  0x0            0x0200C784           {$9} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r57     r109:2  0x0            0x0200C784           {$10} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r58     r107:2  0x0            0x0200C784           {$7} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r59     r105:2  0x0            0x0200C784           {$11} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r60     r103:2  0x0            0x0200C784           {$12} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r61     r101:2  0x0            0x0200C784           {$13} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r62     r99:2   0x0            0x0200C784           {$6} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r63     r97:2   0x0            0x0200C784           {$14} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r64     r95:2   0x0            0x0200C784           {$15} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r65     r93:2   0x0            0x0200C784           {$0} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r66     r91:2   0x0            0x0200C784           {$1} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@7}
        send.ugm (1|M0)          null     r67     r89:2   0x0            0x0200C784           {$2} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r68     r87:2   0x0            0x0200C784           {$4} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@6}
        send.ugm (1|M0)          null     r69     r85:2   0x0            0x0200C784           {$5} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@5}
        send.ugm (1|M0)          null     r70     r83:2   0x0            0x0200C784           {$9} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@4}
        send.ugm (1|M0)          null     r71     r81:2   0x0            0x0200C784           {$6} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@3}
        send.ugm (1|M0)          null     r72     r79:2   0x0            0x0200C784           {$7} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@2}
        send.ugm (1|M0)          null     r73     r77:2   0x0            0x0200C784           {$3} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@1}
        send.ugm (1|M0)          null     r74     r75:2   0x0            0x0200C784           {$8} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        goto (16|M0)                         L9520                  L9568                
L9520:
        join (16|M0)                         L9568                                
L9536:
(W)     mov (2|M0)               r43.4<1>:ud   r43.6<1;1,0>:ud                  {$12.src}
        goto.b (16|M0)                       L9568                  L6632                
L9568:
        join (16|M0)                         L9584                                
L9584:
(W)     mov (8|M0)               r255.0<1>:f   r0.0<1;1,0>:f                    {Compacted}
(W)     send.ugm (1|M0)          r5       r0      null:0  0x0            0x0210641F           {$5} // wr:1+0, rd:1; fence invalid flush type scoped to tile
(W)     mov (8|M0)               null<1>:ud    r5.0<1;1,0>:ud                   {$5.dst}
(W)     csel (4|M0)   (eq)f0.0   r1.0<1>:w     r1.0<1;0>:w       r1.0<1;0>:w       r1.0<1>:w       
(W)     csel (4|M0)   (eq)f0.0   r1.4<1>:f     r1.4<1;0>:f       r1.4<1;0>:f       r1.4<1>:f        {A@1}
(W)     send.gtwy (1|M0)         null     r255    null:0  0x0            0x02000010           {EOT,A@1} // wr:1+0, rd:0; end of thread
L9672:
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
