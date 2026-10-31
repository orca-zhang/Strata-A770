L0:
(W)     mov (8|M0)               r127.0<1>:ud  0x0:ud                              {A@1}
(W)     and (1|M0)               r127.2<1>:ud  r0.0<0;1,0>:ud    0xFFFFFFC0:ud             
(W)     and (1|M0)               r127.0<1>:uw  r0.4<0;1,0>:uw    0xFF:uw             
(W)     add (1|M0)               r127.2<1>:ud  r127.2<0;1,0>:ud  0x60:ud              {I@2}
(W)     mad (1|M0)               r127.2<1>:ud  r127.2<0;0>:ud    r127.0<0;0>:uw    0x20:uw              {I@1}
(W)     send.dc0 (8|M0)          r1       r127    null:0  0x0            0x021842FD           {A@1,$0} // wr:1h+0, rd:1; oword aligned block read x2
        nop                    
        nop                    
(W)     mov (8|M0)               r127.0<1>:ud  0x0:ud                              {$0.src}
(W)     and (1|M0)               r127.2<1>:ud  r0.0<0;1,0>:ud    0xFFFFFFC0:ud             
(W)     send.dc0 (8|M0)          r2       r127    null:0  0x0            0x022843FD           {A@1,$1} // wr:1h+0, rd:2; oword aligned block read x4
(W)     add (1|M0)               r127.2<1>:ud  r127.2<0;1,0>:ud  0x40:uw              {$1.src}
(W)     send.dc0 (8|M0)          r4       r127    null:0  0x0            0x021842FD           {A@1,$2} // wr:1h+0, rd:1; oword aligned block read x2
        and (1|M0)               cr0.0<1>:ud   cr0.0<0;1,0>:ud   -1265:w               {A@1}
        or (1|M0)                cr0.0<1>:ud   cr0.0<0;1,0>:ud   1216:w               {A@1}
        send.ugm (1|M0)          r21      r4      null:0  0x0            0x04100580           {A@1,$2} // wr:2+0, rd:1; load.ugm.d32.a64
(W)     mul (1|M0)               acc0.0<1>:d   r0.1<0;1,0>:d     r2.0<0;1,0>:uw   {Compacted,$1.dst}
        mach (1|M0)              r5.0<1>:d     r0.1<0;1,0>:d     r2.0<0;1,0>:d    {Compacted,$2.src}
        sync.nop                             null                             {Compacted,$2.dst}
        mov (1|M0)               r21.1<1>:d    r1.0<0;1,0>:uw                   {$0.dst}
        add (1|M0)               r5.0<1>:d     r5.0<0;1,0>:d     r1.0<0;1,0>:uw   {I@2}
        mov (1|M0)               r21.5<1>:d    0:w                              
(W)     mul (1|M0)               acc0.0<1>:ud  r5.0<0;1,0>:ud    0xAAAB:uw              {I@2}
        mach (1|M0)              r6.0<1>:ud    r5.0<0;1,0>:ud    0xAAAAAAAB:ud              {AccWrEn}
        shr (1|M0)               r6.0<1>:ud    r6.0<0;1,0>:ud    0xA:uw              {I@1}
        mul (1|M0)               acc0.0<1>:ud  r6.0<0;1,0>:ud    0x7800:uw              {I@1}
        mach (1|M0)              r8.0<1>:ud    r6.0<0;1,0>:ud    0xC7800:ud              {AccWrEn}
        mul (1|M0)               r21.6<1>:d    r6.0<0;1,0>:d     -1536:w               {Compacted}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        add (1|M0)               r21.6<1>:d    r21.6<0;1,0>:d    r5.0<0;1,0>:d    {I@2}
        shl (1|M0)               r4.2<1>:d     r7.0<0;1,0>:d     2:w               {Compacted,I@2}
        shl (1|M0)               r2.3<1>:d     r8.0<0;1,0>:d     2:w               {Compacted}
        addc (1|M0)              r5.0<1>:ud    r2.6<0;1,0>:ud    r4.2<0;1,0>:ud   {AccWrEn,I@2}
        shr (1|M0)               r2.4<1>:ud    r7.0<0;1,0>:ud    0x1E:uw             
        mov (1|M0)               r21.2<1>:d    r5.0<0;1,0>:d                    {I@2}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r10.0<1>:ud   r21.2<0;1,0>:ud   0x300000:ud              {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d   
        mov (1|M0)               r11.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r6.0<0;1,0>:ud    0xA000:uw             
        mach (1|M0)              r13.0<1>:ud   r6.0<0;1,0>:ud    0xA000:ud              {AccWrEn}
        add3 (1|M0)              r21.3<1>:d    r9.0<0;0>:d       r2.7<0;0>:d       r4.3<0>:d        {I@4}
        mov (1|M0)               r12.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        add3 (1|M0)              r95.5<1>:d    r11.0<0;0>:d      r21.3<0;0>:d      0:w               {I@2}
        shl (1|M0)               r1.4<1>:d     r12.0<0;1,0>:d    2:w               {Compacted,I@2}
        shl (1|M0)               r1.2<1>:ud    r21.6<0;1,0>:ud   0x2:uw             
        addc (1|M0)              r5.0<1>:ud    r3.0<0;1,0>:ud    r1.4<0;1,0>:ud   {AccWrEn,I@2}
        mov (1|M0)               r95.4<1>:d    r10.0<0;1,0>:d                  
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r8.0<1>:ud    r3.2<0;1,0>:ud    r1.4<0;1,0>:ud   {AccWrEn}
        shr (1|M0)               r1.3<1>:ud    r12.0<0;1,0>:ud   0x1E:uw             
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r6.0<0;1,0>:ud    0xC0:uw             
        mach (1|M0)              r11.0<1>:ud   r6.0<0;1,0>:ud    0xC0:ud              {AccWrEn}
        and (1|M0)               r21.4<1>:ud   r1.2<0;1,0>:ud    0x70:uw              {I@7}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r1.2<1>:d     r13.0<0;1,0>:d    2:w               {Compacted}
        shl (1|M0)               r4.2<1>:d     r10.0<0;1,0>:d    2:w               {Compacted,I@2}
        mov (1|M0)               r94.0<1>:f    r5.0<0;1,0>:f                    {Compacted}
        or (1|M0)                r1.5<1>:d     r1.2<0;1,0>:d     r1.3<0;1,0>:d    {I@2}
        addc (1|M0)              r5.0<1>:ud    r3.4<0;1,0>:ud    r4.2<0;1,0>:ud   {AccWrEn,A@1}
        mov (1|M0)               r95.0<1>:f    r8.0<0;1,0>:f                    {Compacted}
        shl (1|M0)               r2.3<1>:d     r11.0<0;1,0>:d    2:w               {Compacted}
        shr (1|M0)               r2.4<1>:ud    r10.0<0;1,0>:ud   0x1E:uw             
        cmp (1|M0)    (lt)f0.0   null<1>:d     r21.0<0;1,0>:d    1:w              
        add3 (1|M0)              r94.1<1>:d    r7.0<0;0>:d       r3.1<0;0>:d       r1.5<0>:d        {I@5}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r8.0<1>:ud    r3.6<0;1,0>:ud    r4.2<0;1,0>:ud   {AccWrEn,F@1}
(W)     and (1|M0)               f0.0<1>:uw    f0.0<0;1,0>:uw    0x1:uw             
        add3 (1|M0)              r95.1<1>:d    r9.0<0;0>:d       r3.3<0;0>:d       r1.5<0>:d       
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@7}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shr (1|M0)               r94.6<1>:ud   r21.6<0;1,0>:ud   0x5:uw             
        mov (1|M0)               r95.2<1>:d    r5.0<0;1,0>:d                   
        mov (1|M0)               r94.2<1>:d    r8.0<0;1,0>:d                    {I@7}
        add3 (1|M0)              r95.3<1>:d    r7.0<0;0>:d       r3.5<0;0>:d       r4.3<0>:d        {I@5}
        add3 (1|M0)              r94.3<1>:d    r9.0<0;0>:d       r3.7<0;0>:d       r4.3<0>:d        {I@5}
(f0.0.any16h) goto (16|M0)                   L9608                  L9608                
L1080:
        cmp (1|M0)    (lt)f1.1   null<1>:ud    r21.6<0;1,0>:ud   0x280:uw             
        shl (1|M0)               r94.7<1>:ud   r21.6<0;1,0>:ud   0x4:uw             
(W)     and (1|M0)               f1.1<1>:uw    f1.1<0;1,0>:uw    0x1:uw             
(~f1.1.any16h) goto (16|M0)                  L3904                  L3904                
L1144:
        shl (1|M0)               r1.3<1>:d     r94.7<0;1,0>:d    2:w               {I@3}
        cmp (1|M0)    (lt)f1.0   null<1>:ud    r21.0<0;1,0>:ud   0x3:uw             
        addc (1|M0)              r5.0<1>:ud    r95.0<0;1,0>:ud   r1.3<0;1,0>:ud   {AccWrEn,I@2}
        shr (1|M0)               r1.2<1>:ud    r94.7<0;1,0>:ud   0x1E:uw             
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r94.7<0;1,0>:ud   0x3:uw             
        mach (1|M0)              r8.0<1>:ud    r94.7<0;1,0>:ud   0x3:ud              {AccWrEn}
(W)     and (1|M0)               f1.0<1>:uw    f1.0<0;1,0>:uw    0x1:uw             
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r8.0<0;1,0>:d     2:w               {Compacted,I@3}
        shl (1|M0)               r4.2<1>:d     r7.0<0;1,0>:d     2:w               {Compacted,I@2}
        shr (1|M0)               r2.4<1>:ud    r7.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r9.0<1>:ud    r95.4<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r1.4<1>:d     r1.2<0;1,0>:d     0:w              
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@3}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r94.4<1>:d    r21.0<0;1,0>:d                  
        mov (1|M0)               r94.5<1>:d    0:w                              
        mov (1|M0)               r21.6<1>:d    r5.0<0;1,0>:d                   
        mov (1|M0)               r95.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@7}
        add3 (1|M0)              r21.7<1>:d    r6.0<0;0>:d       r95.1<0;0>:d      r1.4<0>:d        {I@6}
        add3 (1|M0)              r95.1<1>:d    r10.0<0;0>:d      r95.5<0;0>:d      r4.3<0>:d        {I@5}
(f1.0.any16h) goto (16|M0)                   L1648                  L1648                
L1464:
        add (1|M0)               r1.2<1>:ud    r21.0<0;1,0>:ud   0xFFFFFFFD:ud             
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x2800:uw              {I@1}
        mach (1|M0)              r6.0<1>:ud    r1.2<0;1,0>:ud    0x2800:ud              {AccWrEn}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r6.0<0;1,0>:d     2:w               {Compacted,I@2}
        shl (1|M0)               r4.2<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@2}
        shr (1|M0)               r2.4<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r7.0<1>:ud    r21.6<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r9.0<1>:f     r7.0<0;1,0>:f                    {Compacted,I@3}
        add3 (1|M0)              r9.1<1>:d     r8.0<0;0>:d       r21.7<0;0>:d      r4.3<0>:d        {I@1}
        send.ugm (1|M0)          r96      r9      null:0  0x0            0x0220D580           {A@1,$3} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        goto (16|M0)                         L1648                  L2736                
L1648:
        join (16|M0)                         L2736                                
L1664:
        mov (4|M0)               r4.8<1>:w     0x32103210:v                              
        shl (1|M0)               r1.4<1>:d     r94.4<0;1,0>:d    2:w              
        mul (4|M0)               r7.0<1>:d     r4.8<1;1,0>:w     12:w               {I@2}
        mul (4|M0)               r8.0<2>:d     r4.8<1;1,0>:w     12:w              
        addc (1|M0)              r5.0<1>:ud    r95.0<0;1,0>:ud   r1.4<0;1,0>:ud   {AccWrEn,I@3}
        asr (4|M0)               r8.1<2>:d     r7.0<1;1,0>:d     31:w               {I@3}
        shl (1|M0)               r1.2<1>:d     r94.5<0;1,0>:d    2:w              
        shr (1|M0)               r1.3<1>:ud    r94.4<0;1,0>:ud   0x1E:uw             
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        sync.nop                             null                             {Compacted,$3.src}
        addc (4|M0)              r9.0<1>:ud    r8.0<2;1,0>:ud    0x30:ud              {AccWrEn,I@4}
        mov (1|M0)               r2.4<1>:d     r5.0<0;1,0>:d                   
        or (1|M0)                r1.5<1>:d     r1.2<0;1,0>:d     r1.3<0;1,0>:d    {I@4}
        mov (4|M0)               r10.0<1>:ud   acc0.0<1;1,0>:ud                
        addc (4|M0)              r5.0<1>:ud    r8.0<2;1,0>:ud    0x60:ud              {AccWrEn}
        add3 (1|M0)              r2.5<1>:d     r6.0<0;0>:d       r95.1<0;0>:d      r1.5<0>:d        {I@3}
        mov (4|M0)               r6.0<1>:ud    acc0.0<1;1,0>:ud                
        addc (4|M0)              r11.0<1>:ud   r8.0<2;1,0>:ud    0x90:ud              {AccWrEn}
        mov (4|M0)               r100.0<2>:ud  r8.0<2;1,0>:ud                  
        mov (4|M0)               r101.0<2>:d   r9.0<1;1,0>:d                   
        mov (8|M0)               r12.0<2>:ud   r2.4<0;1,0>:ud                  
        mov (4|M0)               r100.1<2>:ud  r8.1<2;1,0>:ud                  
        mov (4|M0)               r7.0<1>:ud    acc0.0<1;1,0>:ud                
        add3 (4|M0)              r101.1<2>:d   r10.0<1;0>:d      r8.1<4;2>:d       0:w               {I@7}
        mov (8|M0)               r12.1<2>:ud   r2.5<0;1,0>:ud                   {I@7}
        mov (4|M0)               r98.0<2>:d    r5.0<1;1,0>:d                   
        mov (4|M0)               r99.0<2>:d    r11.0<1;1,0>:d                   {I@7}
        add3 (4|M0)              r98.1<2>:d    r6.0<1;0>:d       r8.1<4;2>:d       0:w              
        addc (8|M0)              r9.0<1>:ud    r12.0<2;1,0>:ud   r100.0<2;1,0>:ud {AccWrEn,Compacted,I@4}
        add3 (4|M0)              r99.1<2>:d    r7.0<1;0>:d       r8.1<4;2>:d       0:w              
        mov (8|M0)               r10.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        addc (8|M0)              r6.0<1>:ud    r12.0<2;1,0>:ud   r98.0<2;1,0>:ud  {AccWrEn,Compacted,I@2}
        mov (8|M0)               r18.0<1>:d    r100.1<2;1,0>:d                 
        mov (8|M0)               r7.0<1>:d     r98.1<2;1,0>:d                  
        mov (8|M0)               r19.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        add3 (8|M0)              r5.0<1>:d     r10.0<1;0>:d      r12.1<4;2>:d      r18.0<1>:d       {I@3}
        add3 (8|M0)              r8.0<1>:d     r19.0<1;0>:d      r12.1<4;2>:d      r7.0<1>:d        {I@2}
        mov (8|M0)               r14.0<2>:d    r9.0<1;1,0>:d                   
        mov (8|M0)               r16.0<2>:d    r6.0<1;1,0>:d                   
        mov (8|M0)               r14.1<2>:d    r5.0<1;1,0>:d                    {I@4}
        mov (8|M0)               r16.1<2>:d    r8.0<1;1,0>:d                    {I@4}
        cmp (1|M0)    (eq)f0.1   null<1>:d     r21.0<0;1,0>:d    2:w              
        send.dc1 (16|M0)         r96      r14     null:0  0x0            0x082411FF           {A@2,$3} // wr:4+0, rd:2; a64 dword gathering read
        addc (1|M0)              r11.0<1>:ud   r94.4<0;1,0>:ud   0x1:ud              {AccWrEn}
(W)     and (1|M0)               f0.1<1>:uw    f0.1<0;1,0>:uw    0x1:uw             
        mov (1|M0)               r20.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r95.4<1>:d    r11.0<0;1,0>:d                   {I@3}
        add3 (1|M0)              r95.5<1>:d    r20.0<0;0>:d      r94.5<0;0>:d      0:w               {I@2}
(f0.1.any16h) goto (16|M0)                   L2736                  L2736                
L2392:
        shl (1|M0)               r1.4<1>:d     r95.4<0;1,0>:d    2:w               {I@3}
        shl (1|M0)               r1.2<1>:d     r95.5<0;1,0>:d    2:w               {I@3}
        shr (1|M0)               r1.3<1>:ud    r95.4<0;1,0>:ud   0x1E:uw             
        addc (1|M0)              r5.0<1>:ud    r95.0<0;1,0>:ud   r1.4<0;1,0>:ud   {AccWrEn,I@3}
        or (1|M0)                r1.5<1>:d     r1.2<0;1,0>:d     r1.3<0;1,0>:d    {I@2}
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r2.4<1>:d     r5.0<0;1,0>:d                    {I@3}
        add3 (1|M0)              r2.5<1>:d     r6.0<0;0>:d       r95.1<0;0>:d      r1.5<0>:d        {I@2}
        mov (8|M0)               r7.0<2>:ud    r2.4<0;1,0>:ud                   {I@2}
        mov (8|M0)               r7.1<2>:ud    r2.5<0;1,0>:ud                   {I@2}
        mov (8|M0)               r15.0<1>:d    r100.1<2;1,0>:d                  {$3.src}
        addc (8|M0)              r9.0<1>:ud    r7.0<2;1,0>:ud    r100.0<2;1,0>:ud {AccWrEn,Compacted,I@2}
        mov (8|M0)               r5.0<1>:d     r98.1<2;1,0>:d                  
        mov (8|M0)               r10.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        addc (8|M0)              r17.0<1>:ud   r7.0<2;1,0>:ud    r98.0<2;1,0>:ud  {AccWrEn,Compacted}
        add3 (8|M0)              r16.0<1>:d    r10.0<1;0>:d      r7.1<4;2>:d       r15.0<1>:d       {I@2}
        mov (8|M0)               r18.0<1>:ud   acc0.0<1;1,0>:ud                 {Compacted}
        mov (8|M0)               r11.0<2>:d    r9.0<1;1,0>:d                   
        add3 (8|M0)              r6.0<1>:d     r18.0<1;0>:d      r7.1<4;2>:d       r5.0<1>:d        {I@2}
        mov (8|M0)               r13.0<2>:d    r17.0<1;1,0>:d                  
        mov (8|M0)               r11.1<2>:d    r16.0<1;1,0>:d                  
        mov (8|M0)               r13.1<2>:d    r6.0<1;1,0>:d                    {I@3}
        send.dc1 (16|M0)         r102     r11     null:0  0x0            0x082411FF           {A@1,$4} // wr:4+0, rd:2; a64 dword gathering read
        goto (16|M0)                         L2736                  L2920                
L2736:
        join (16|M0)                         L2920                                
L2752:
        add (1|M0)               r1.2<1>:d     r21.0<0;1,0>:d    -2:w               {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x2800:uw              {I@1}
        mach (1|M0)              r6.0<1>:ud    r1.2<0;1,0>:ud    0x2800:ud              {AccWrEn}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r6.0<0;1,0>:d     2:w               {Compacted,I@2}
        shl (1|M0)               r4.2<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@2}
        shr (1|M0)               r2.4<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r7.0<1>:ud    r21.6<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        sync.nop                             null                             {Compacted,$3.src}
        mov (1|M0)               r9.0<1>:f     r7.0<0;1,0>:f                    {Compacted,I@3}
        add3 (1|M0)              r9.1<1>:d     r8.0<0;0>:d       r21.7<0;0>:d      r4.3<0>:d        {I@1}
        send.ugm (1|M0)          r102     r9      null:0  0x0            0x0220D580           {A@1,$4} // wr:1+0, rd:2; load.ugm.d32x16t.a64
L2920:
        join (16|M0)                         L3904                                
L2936:
        add (1|M0)               r1.2<1>:d     r21.0<0;1,0>:d    -1:w               {Compacted}
        sync.nop                             null                             {Compacted,$4.src}
        mov (1|M0)               r12.0<1>:ud   r96.0<0;1,0>:ud                  {$3.dst}
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x2800:uw              {I@2}
        mach (1|M0)              r6.0<1>:ud    r1.2<0;1,0>:ud    0x2800:ud              {AccWrEn}
        mov (1|M0)               r12.3<1>:f    r96.1<0;1,0>:f                  
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r2.3<1>:d     r6.0<0;1,0>:d     2:w               {Compacted,I@2}
        shl (1|M0)               r4.2<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@2}
        shr (1|M0)               r2.4<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        addc (1|M0)              r7.0<1>:ud    r21.6<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r9.0<1>:d     r7.0<0;1,0>:d                    {I@3}
        add3 (1|M0)              r9.1<1>:d     r8.0<0;0>:d       r21.7<0;0>:d      r4.3<0>:d        {I@2}
        addc (1|M0)              r5.0<1>:ud    r95.0<0;1,0>:ud   0x80:ud              {AccWrEn}
        send.ugm (1|M0)          r10      r9      null:0  0x0            0x0220D580           {A@2,$5} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r12.6<1>:f    r96.2<0;1,0>:f                  
        mov (1|M0)               r13.1<1>:f    r96.3<0;1,0>:f                  
        mov (1|M0)               r13.4<1>:ud   r96.4<0;1,0>:ud                 
        mov (1|M0)               r13.7<1>:f    r96.5<0;1,0>:f                  
        mov (1|M0)               r14.2<1>:f    r96.6<0;1,0>:f                  
        mov (1|M0)               r14.5<1>:f    r96.7<0;1,0>:f                  
        mov (1|M0)               r15.0<1>:ud   r97.0<0;1,0>:ud                 
        mov (1|M0)               r15.3<1>:f    r97.1<0;1,0>:f                  
        mov (1|M0)               r15.6<1>:f    r97.2<0;1,0>:f                  
        mov (1|M0)               r12.1<1>:f    r102.0<0;1,0>:f                  {Compacted,$4.dst}
        mov (1|M0)               r12.4<1>:f    r102.1<0;1,0>:f                 
        mov (1|M0)               r12.7<1>:f    r102.2<0;1,0>:f                 
        mov (1|M0)               r13.2<1>:f    r102.3<0;1,0>:f                 
        mov (1|M0)               r13.5<1>:f    r102.4<0;1,0>:f                 
        mov (1|M0)               r14.0<1>:f    r102.5<0;1,0>:f                  {Compacted}
        mov (1|M0)               r14.3<1>:f    r102.6<0;1,0>:f                 
        mov (1|M0)               r14.6<1>:f    r102.7<0;1,0>:f                 
        mov (1|M0)               r15.1<1>:f    r103.0<0;1,0>:f                  {Compacted}
        mov (1|M0)               r15.4<1>:f    r103.1<0;1,0>:f                 
        mov (1|M0)               r15.7<1>:f    r103.2<0;1,0>:f                 
        mov (1|M0)               r96.1<1>:f    r97.3<0;1,0>:f                  
        mov (1|M0)               r7.0<1>:d     r5.0<0;1,0>:d                    {I@4}
        mov (1|M0)               r96.2<1>:f    r103.3<0;1,0>:f                 
        add3 (1|M0)              r7.1<1>:d     r6.0<0;0>:d       r95.1<0;0>:d      0:w               {I@4}
        mov (1|M0)               r96.4<1>:ud   r97.4<0;1,0>:ud                 
        mov (1|M0)               r96.5<1>:f    r103.4<0;1,0>:f                 
        mov (1|M0)               r96.7<1>:f    r97.5<0;1,0>:f                  
        mov (1|M0)               r97.0<1>:f    r103.5<0;1,0>:f                  {Compacted,I@4}
        mov (1|M0)               r97.2<1>:f    r97.6<0;1,0>:f                  
        mov (1|M0)               r97.3<1>:f    r103.6<0;1,0>:f                 
        mov (1|M0)               r97.5<1>:f    r97.7<0;1,0>:f                  
        mov (1|M0)               r97.6<1>:f    r103.7<0;1,0>:f                 
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
        mov (1|M0)               r96.0<1>:f    r11.2<0;1,0>:f                   {Compacted}
        mov (1|M0)               r96.3<1>:ud   r11.3<0;1,0>:ud                 
        mov (1|M0)               r96.6<1>:f    r11.4<0;1,0>:f                  
        mov (1|M0)               r97.1<1>:f    r11.5<0;1,0>:f                  
        mov (1|M0)               r97.4<1>:f    r11.6<0;1,0>:f                   {I@4}
        mov (1|M0)               r97.7<1>:ud   r11.7<0;1,0>:ud                 
        send.ugm (1|M0)          null     r95     r12:4   0x0            0x0200D784           {A@3,$6} // wr:1+4, rd:0; store.ugm.d64x16t.a64
        send.ugm (1|M0)          null     r7      r96:2   0x0            0x0200C784           {A@1,$7} // wr:1+2, rd:0; store.ugm.d64x8t.a64
L3904:
        join (16|M0)                         L9608                                
L3920:
        shl (1|M0)               r1.3<1>:d     r94.6<0;1,0>:d    7:w              
        shr (1|M0)               r1.2<1>:ud    r94.6<0;1,0>:ud   0x19:uw             
        shl (1|M0)               r4.2<1>:d     r1.3<0;1,0>:d     2:w               {I@2}
        shl (1|M0)               r3.2<1>:d     r21.4<0;1,0>:d    2:w              
        addc (1|M0)              r5.0<1>:ud    r21.2<0;1,0>:ud   r4.2<0;1,0>:ud   {AccWrEn,I@2}
        or (1|M0)                r1.4<1>:d     r1.2<0;1,0>:d     0:w              
        mov (1|M0)               r6.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shl (1|M0)               r1.2<1>:d     r21.1<0;1,0>:d    5:w              
        sync.nop                             null                             {Compacted,$7.src}
        addc (1|M0)              r7.0<1>:ud    r5.0<0;1,0>:ud    r3.2<0;1,0>:ud   {AccWrEn,I@4}
        shr (1|M0)               r2.4<1>:ud    r1.3<0;1,0>:ud    0x1E:uw             
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mul (1|M0)               acc0.0<1>:ud  r1.2<0;1,0>:ud    0x1800:uw              {I@4}
        mach (1|M0)              r10.0<1>:ud   r1.2<0;1,0>:ud    0x1800:ud              {AccWrEn}
        shl (1|M0)               r2.3<1>:d     r1.4<0;1,0>:d     2:w              
        sync.nop                             null                             {Compacted,$4.src}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted,$3.src}
        or (1|M0)                r4.3<1>:d     r2.3<0;1,0>:d     r2.4<0;1,0>:d    {I@2}
        shl (1|M0)               r2.4<1>:d     r9.0<0;1,0>:d     2:w               {Compacted,I@2}
        add3 (1|M0)              r5.2<1>:d     r6.0<0;0>:d       r21.3<0;0>:d      r4.3<0>:d        {I@2}
        addc (1|M0)              r6.0<1>:ud    r7.0<0;1,0>:ud    r2.4<0;1,0>:ud   {AccWrEn,I@2}
        shl (1|M0)               r5.3<1>:d     r21.5<0;1,0>:d    2:w              
        shr (1|M0)               r5.4<1>:ud    r21.4<0;1,0>:ud   0x1E:uw             
        mov (1|M0)               r125.0<1>:f   r6.0<0;1,0>:f                    {Compacted,I@3}
        mov (1|M0)               r11.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        or (1|M0)                r3.3<1>:d     r5.3<0;1,0>:d     r5.4<0;1,0>:d    {I@2}
        addc (1|M0)              r5.0<1>:ud    r125.0<0;1,0>:ud  0x6000:ud              {AccWrEn,F@1}
        shl (1|M0)               r1.7<1>:d     r10.0<0;1,0>:d    2:w               {Compacted}
        shr (1|M0)               r2.3<1>:ud    r9.0<0;1,0>:ud    0x1E:uw             
        add3 (1|M0)              r1.6<1>:d     r8.0<0;0>:d       r5.2<0;0>:d       r3.3<0>:d        {I@4}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r125.0<0;1,0>:ud  0xC000:ud              {AccWrEn}
        or (1|M0)                r2.5<1>:d     r1.7<0;1,0>:d     r2.3<0;1,0>:d    {I@4}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r125.0<0;1,0>:ud  0x12000:ud              {AccWrEn}
        mov (1|M0)               r124.0<1>:f   r5.0<0;1,0>:f                    {Compacted}
        add3 (1|M0)              r125.1<1>:d   r11.0<0;0>:d      r1.6<0;0>:d       r2.5<0>:d        {I@3}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r125.0<0;1,0>:ud  0x18000:ud              {AccWrEn,F@1}
        mov (1|M0)               r123.0<1>:f   r9.0<0;1,0>:f                    {Compacted}
        add3 (1|M0)              r124.1<1>:d   r8.0<0;0>:d       r125.1<0;0>:d     0:w               {I@3}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r125.0<0;1,0>:ud  0x1E000:ud              {AccWrEn,F@1}
        mov (1|M0)               r122.0<1>:f   r6.0<0;1,0>:f                    {Compacted}
        add3 (1|M0)              r123.1<1>:d   r10.0<0;0>:d      r125.1<0;0>:d     0:w              
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r125.0<0;1,0>:ud  0x24000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r122.1<1>:d   r7.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r121.0<1>:f   r5.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r125.0<0;1,0>:ud  0x2A000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r121.1<1>:d   r8.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r120.0<1>:f   r9.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r125.0<0;1,0>:ud  0x30000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r120.1<1>:d   r10.0<0;0>:d      r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r119.0<1>:f   r6.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r125.0<0;1,0>:ud  0x36000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r119.1<1>:d   r7.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r118.0<1>:f   r5.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r125.0<0;1,0>:ud  0x3C000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r118.1<1>:d   r8.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r117.0<1>:f   r9.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r125.0<0;1,0>:ud  0x42000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r117.1<1>:d   r10.0<0;0>:d      r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r116.0<1>:f   r6.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r125.0<0;1,0>:ud  0x48000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r116.1<1>:d   r7.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r115.0<1>:f   r5.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r125.0<0;1,0>:ud  0x4E000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r115.1<1>:d   r8.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r114.0<1>:f   r9.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r125.0<0;1,0>:ud  0x54000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r114.1<1>:d   r10.0<0;0>:d      r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r113.0<1>:f   r6.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r125.0<0;1,0>:ud  0x5A000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r113.1<1>:d   r7.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r112.0<1>:f   r5.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r125.0<0;1,0>:ud  0x60000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r112.1<1>:d   r8.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r111.0<1>:f   r9.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r125.0<0;1,0>:ud  0x66000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r111.1<1>:d   r10.0<0;0>:d      r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r110.0<1>:f   r6.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r125.0<0;1,0>:ud  0x6C000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r110.1<1>:d   r7.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r109.0<1>:f   r5.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r125.0<0;1,0>:ud  0x72000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r109.1<1>:d   r8.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r108.0<1>:f   r9.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r125.0<0;1,0>:ud  0x78000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r108.1<1>:d   r10.0<0;0>:d      r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r107.0<1>:f   r6.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r125.0<0;1,0>:ud  0x7E000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r107.1<1>:d   r7.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r106.0<1>:f   r5.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r125.0<0;1,0>:ud  0x84000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r106.1<1>:d   r8.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r105.0<1>:f   r9.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r125.0<0;1,0>:ud  0x8A000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r105.1<1>:d   r10.0<0;0>:d      r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r104.0<1>:f   r6.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r125.0<0;1,0>:ud  0x90000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r104.1<1>:d   r7.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        sync.nop                             null                             {Compacted,$4.dst}
        mov (1|M0)               r103.0<1>:f   r5.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r125.0<0;1,0>:ud  0x96000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r103.1<1>:d   r8.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r102.0<1>:f   r9.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r125.0<0;1,0>:ud  0x9C000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r102.1<1>:d   r10.0<0;0>:d      r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r101.0<1>:f   r6.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r125.0<0;1,0>:ud  0xA2000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r101.1<1>:d   r7.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r100.0<1>:f   r5.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r125.0<0;1,0>:ud  0xA8000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r100.1<1>:d   r8.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r99.0<1>:f    r9.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r125.0<0;1,0>:ud  0xAE000:ud              {AccWrEn,F@1}
        add3 (1|M0)              r99.1<1>:d    r10.0<0;0>:d      r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r98.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r6.0<1>:ud    r125.0<0;1,0>:ud  0xB4000:ud              {AccWrEn,F@1}
        shl (1|M0)               r95.6<1>:d    r94.6<0;1,0>:d    2:w               {$6.src}
        add3 (1|M0)              r98.1<1>:d    r7.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        sync.nop                             null                             {Compacted,$3.dst}
        mov (1|M0)               r97.0<1>:f    r5.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r5.0<1>:ud    r125.0<0;1,0>:ud  0xBA000:ud              {AccWrEn,F@1}
        and (1|M0)               r2.3<1>:d     r1.3<0;1,0>:d     1920:w               {Compacted}
        add3 (1|M0)              r97.1<1>:d    r8.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        mov (1|M0)               r95.0<1>:f    r6.0<0;1,0>:f                    {Compacted,I@7}
        mov (1|M0)               r3.0<1>:f     r5.0<0;1,0>:f                    {Compacted,I@3}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        shr (1|M0)               r5.0<1>:ud    r94.6<0;1,0>:ud   0x1E:uw              {F@1}
        addc (1|M0)              r6.0<1>:ud    r95.2<0;1,0>:ud   r95.6<0;1,0>:ud  {AccWrEn}
        shr (1|M0)               r4.2<1>:ud    r1.2<0;1,0>:ud    0x1E:uw             
        mov (1|M0)               r96.0<1>:f    r9.0<0;1,0>:f                    {Compacted}
        add3 (1|M0)              r96.1<1>:d    r10.0<0;0>:d      r125.1<0;0>:d     0:w              
        add3 (1|M0)              r95.1<1>:d    r7.0<0;0>:d       r125.1<0;0>:d     0:w              
        shr (1|M0)               r2.5<1>:ud    r2.3<0;1,0>:ud    0x1E:uw              {I@7}
        add3 (1|M0)              r3.1<1>:d     r8.0<0;0>:d       r125.1<0;0>:d     0:w               {I@7}
        or (1|M0)                r95.7<1>:d    r5.0<0;1,0>:d     0:w               {Compacted,I@7}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        send.ugm (1|M0)          r30      r125    null:0  0x0            0x0220D580           {$8} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r32      r124    null:0  0x0            0x0220D580           {$9} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r34      r123    null:0  0x0            0x0220D580           {$10} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r36      r122    null:0  0x0            0x0220D580           {$11} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r38      r121    null:0  0x0            0x0220D580           {$12} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r40      r120    null:0  0x0            0x0220D580           {$13} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r42      r119    null:0  0x0            0x0220D580           {$14} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r44      r118    null:0  0x0            0x0220D580           {$15} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r46      r117    null:0  0x0            0x0220D580           {$0} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r48      r116    null:0  0x0            0x0220D580           {$1} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r50      r115    null:0  0x0            0x0220D580           {$2} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r52      r114    null:0  0x0            0x0220D580           {$4} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r54      r113    null:0  0x0            0x0220D580           {$5} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r56      r112    null:0  0x0            0x0220D580           {$6} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r58      r111    null:0  0x0            0x0220D580           {$7} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r60      r110    null:0  0x0            0x0220D580           {$3} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r62      r109    null:0  0x0            0x0220D580           {$8} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r64      r108    null:0  0x0            0x0220D580           {$9} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r66      r107    null:0  0x0            0x0220D580           {$10} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r68      r106    null:0  0x0            0x0220D580           {$11} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r70      r105    null:0  0x0            0x0220D580           {$12} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r72      r104    null:0  0x0            0x0220D580           {$13} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r74      r103    null:0  0x0            0x0220D580           {$14} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r76      r102    null:0  0x0            0x0220D580           {$15} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r78      r101    null:0  0x0            0x0220D580           {$0} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r80      r100    null:0  0x0            0x0220D580           {$1} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r82      r99     null:0  0x0            0x0220D580           {$2} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r84      r98     null:0  0x0            0x0220D580           {$4} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r86      r97     null:0  0x0            0x0220D580           {$5} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r88      r96     null:0  0x0            0x0220D580           {A@1,$6} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r90      r95     null:0  0x0            0x0220D580           {A@2,$7} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        send.ugm (1|M0)          r92      r3      null:0  0x0            0x0220D580           {$3} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        shl (1|M0)               r94.4<1>:d    r94.6<0;1,0>:d    7:w              
        mov (1|M0)               r94.5<1>:d    0:w                              
        sel (1|M0)    (lt)f0.0   r3.4<1>:ud    r21.0<0;1,0>:ud   0x4:uw              {$3.src}
        mov (1|M0)               r3.5<1>:d     0:w                              
        mov (1|M0)               r96.4<1>:f    0.0:f                               {Compacted,$6.src}
        mov (1|M0)               r96.2<2>:f    0.0:f                              
        mov (1|M0)               r96.3<2>:f    0.0:f                              
        shl (1|M0)               r3.6<1>:d     r1.2<0;1,0>:d     2:w              
        shl (1|M0)               r95.4<1>:d    r2.3<0;1,0>:d     2:w               {$7.src}
        or (1|M0)                r3.7<1>:d     r4.2<0;1,0>:d     0:w              
        or (1|M0)                r95.5<1>:d    r2.5<0;1,0>:d     0:w              
        add3 (1|M0)              r94.7<1>:d    r7.0<0;0>:d       r95.3<0;0>:d      r95.7<0>:d       {I@7}
        mov (1|M0)               r94.6<1>:d    r6.0<0;1,0>:d                   
L6664:
        mul (1|M0)               acc0.0<1>:ud  r96.2<0;1,0>:ud   0x2800:uw              {F@2}
        mach (1|M0)              r6.0<1>:ud    r96.2<0;1,0>:ud   0x2800:ud              {AccWrEn}
        mul (1|M0)               r1.2<1>:d     r96.3<0;1,0>:d    10240:w               {F@1}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        add (1|M0)               r2.4<1>:d     r1.2<0;1,0>:d     r6.0<0;1,0>:d    {I@2}
        shl (1|M0)               r4.4<1>:d     r5.0<0;1,0>:d     2:w               {Compacted,I@2}
        shr (1|M0)               r4.3<1>:ud    r5.0<0;1,0>:ud    0x1E:uw             
        shl (1|M0)               r4.2<1>:d     r2.4<0;1,0>:d     2:w               {I@3}
        addc (1|M0)              r7.0<1>:ud    r94.0<0;1,0>:ud   r4.4<0;1,0>:ud   {AccWrEn,I@3}
        or (1|M0)                r4.5<1>:d     r4.2<0;1,0>:d     r4.3<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r9.0<1>:ud    r7.0<0;1,0>:ud    0x2000:ud              {AccWrEn,I@3}
        add3 (1|M0)              r7.2<1>:d     r8.0<0;0>:d       r94.1<0;0>:d      r4.5<0>:d        {I@2}
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r11.0<1>:ud   r9.0<0;1,0>:ud    r95.4<0;1,0>:ud  {AccWrEn,I@3}
        add3 (1|M0)              r7.4<1>:d     r10.0<0;0>:d      r7.2<0;0>:d       0:w               {I@2}
        mov (1|M0)               r12.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r13.0<1>:ud   r11.0<0;1,0>:ud   r3.6<0;1,0>:ud   {AccWrEn,I@3}
        add3 (1|M0)              r1.3<1>:d     r12.0<0;0>:d      r7.4<0;0>:d       r95.5<0>:d       {I@2}
        mov (1|M0)               r5.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r6.0<1>:d     r13.0<0;1,0>:d                   {I@3}
        add3 (1|M0)              r6.1<1>:d     r5.0<0;0>:d       r1.3<0;0>:d       r3.7<0>:d        {I@2}
        mul (1|M0)               acc0.0<1>:ud  r96.2<0;1,0>:ud   0x30:uw             
        sync.nop                             null                             {Compacted,$3.src}
        send.ugm (1|M0)          r14      r6      null:0  0x0            0x0240E580           {A@2,$8} // wr:1+0, rd:4; load.ugm.d32x32t.a64
        mach (1|M0)              r19.0<1>:ud   r96.2<0;1,0>:ud   0x30:ud              {AccWrEn}
        mul (1|M0)               r2.3<1>:d     r96.3<0;1,0>:d    48:w               {Compacted}
        mov (1|M0)               r18.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        add (1|M0)               r4.3<1>:d     r2.3<0;1,0>:d     r19.0<0;1,0>:d   {Compacted,I@2}
        shl (1|M0)               r7.5<1>:d     r18.0<0;1,0>:d    2:w               {Compacted,I@2}
        shr (1|M0)               r7.4<1>:ud    r18.0<0;1,0>:ud   0x1E:uw             
        shl (1|M0)               r7.3<1>:d     r4.3<0;1,0>:d     2:w               {Compacted,I@3}
        addc (1|M0)              r5.0<1>:ud    r94.6<0;1,0>:ud   r7.5<0;1,0>:ud   {AccWrEn,I@3}
        or (1|M0)                r7.6<1>:d     r7.3<0;1,0>:d     r7.4<0;1,0>:d    {I@2}
        mov (1|M0)               r8.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        mov (1|M0)               r10.0<1>:d    r5.0<0;1,0>:d                    {I@3}
        add3 (1|M0)              r10.1<1>:d    r8.0<0;0>:d       r94.7<0;0>:d      r7.6<0>:d        {I@2}
        shl (1|M0)               r1.2<1>:d     r96.2<0;1,0>:d    8:w              
        send.ugm (1|M0)          r6       r10     null:0  0x0            0x04100580           {A@2,$9} // wr:2+0, rd:1; load.ugm.d32.a64
        and (1|M0)               r5.0<1>:d     r1.2<0;1,0>:d     256:w               {Compacted,I@1}
        mad (1|M0)               r8.0<1>:d     r5.0<0;0>:d       r1.0<0;0>:uw      64:w               {I@1}
        mad (16|M0)              acc0.0<1>:f   r96.4<0;0>:f      r30.0<1;0>:f      r14.0<0>:f       {Compacted,$8.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r32.0<1;0>:f      r14.1<0>:f       {Compacted,$9.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r34.0<1;0>:f      r14.2<0>:f       {Compacted,$10.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r36.0<1;0>:f      r14.3<0>:f       {Compacted,$11.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r38.0<1;0>:f      r14.4<0>:f       {Compacted,$12.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r40.0<1;0>:f      r14.5<0>:f       {Compacted,$13.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r42.0<1;0>:f      r14.6<0>:f       {Compacted,$14.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r44.0<1;0>:f      r14.7<0>:f       {Compacted,$15.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r46.0<1;0>:f      r15.0<0>:f       {Compacted,$0.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r48.0<1;0>:f      r15.1<0>:f       {Compacted,$1.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r50.0<1;0>:f      r15.2<0>:f       {Compacted,$2.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r52.0<1;0>:f      r15.3<0>:f       {Compacted,$4.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r54.0<1;0>:f      r15.4<0>:f       {Compacted,$5.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r56.0<1;0>:f      r15.5<0>:f       {Compacted,$6.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r58.0<1;0>:f      r15.6<0>:f       {Compacted,$7.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r60.0<1;0>:f      r15.7<0>:f       {Compacted,$3.dst}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r62.0<1;0>:f      r16.0<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r64.0<1;0>:f      r16.1<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r66.0<1;0>:f      r16.2<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r68.0<1;0>:f      r16.3<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r70.0<1;0>:f      r16.4<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r72.0<1;0>:f      r16.5<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r74.0<1;0>:f      r16.6<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r76.0<1;0>:f      r16.7<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r78.0<1;0>:f      r17.0<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r80.0<1;0>:f      r17.1<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r82.0<1;0>:f      r17.2<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r84.0<1;0>:f      r17.3<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r86.0<1;0>:f      r17.4<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r88.0<1;0>:f      r17.5<0>:f       {Compacted}
        mad (16|M0)              acc0.0<1>:f   acc0.0<1;0>:f     r90.0<1;0>:f      r17.6<0>:f       {Compacted}
        mad (16|M0)              r11.0<1>:f    acc0.0<1;0>:f     r92.0<1;0>:f      r17.7<0>:f       {Compacted,$9.src}
        send.slm (1|M0)          null     r8      r11:2   0x0            0x0200C704           {A@1,$10} // wr:1+2, rd:0; store.slm.d64x8t.a32
        mul (1|M0)               r6.0<1>:f     r6.0<0;1,0>:f     1.442695e+00:f               {$9.dst}
        math.exp (1|M0)          r6.0<1>:f     r6.0<0;1,0>:f                    {@1,$11}
(W)     send.slm (1|M0)          r9       r0      null:0  0x0            0x0210001F           {$12} // wr:1+0, rd:1; fence.slm.none.group
(W)     mov (1|M0)               r10.2<1>:ud   0x0:ud                             
(W)     mov (2|M0)               r10.10<1>:ub  r0.11<0;1,0>:ub                 
(W)     mov (8|M0)               null<1>:ud    r9.0<1;1,0>:ud                   {$12.dst}
(W)     send.gtwy (1|M0)         null     r10     null:0  0x0            0x02000004           {A@2,$13} // wr:1+0, rd:0; signal barrier
(W)     sync.bar                             null                            
        addc (1|M0)              r13.0<1>:ud   r7.0<0;1,0>:ud    0x4000:ud              {AccWrEn}
        shl (1|M0)               r1.6<1>:d     r94.4<0;1,0>:d    2:w              
        mov (1|M0)               r26.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        sync.nop                             null                             {Compacted,$10.src}
        addc (1|M0)              r8.0<1>:ud    r13.0<0;1,0>:ud   r1.6<0;1,0>:ud   {AccWrEn,I@2}
        shl (1|M0)               r1.4<1>:d     r94.5<0;1,0>:d    2:w              
        shr (1|M0)               r1.5<1>:ud    r94.4<0;1,0>:ud   0x1E:uw             
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        sync.nop                             null                             {Compacted,$13.src}
        addc (1|M0)              r10.0<1>:ud   r8.0<0;1,0>:ud    r3.2<0;1,0>:ud   {AccWrEn,I@4}
        add3 (1|M0)              r1.3<1>:d     r26.0<0;0>:d      r7.2<0;0>:d       0:w              
        or (1|M0)                r1.7<1>:d     r1.4<0;1,0>:d     r1.5<0;1,0>:d    {I@4}
        mov (1|M0)               r11.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r7.0<1>:ud    r94.2<0;1,0>:ud   r95.6<0;1,0>:ud  {AccWrEn}
        add3 (1|M0)              r2.4<1>:d     r9.0<0;0>:d       r1.3<0;0>:d       r1.7<0>:d        {I@3}
        mov (1|M0)               r9.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted}
        addc (1|M0)              r8.0<1>:ud    r7.0<0;1,0>:ud    r7.5<0;1,0>:ud   {AccWrEn,I@3}
        send.slm (1|M0)          r18      r5      null:0  0x0            0x0280E700           {$3} // wr:1+0, rd:8; load.slm.d64x32t.a32
        sync.nop                             null                             {Compacted,$3.src}
        add3 (1|M0)              r5.1<1>:d     r11.0<0;0>:d      r2.4<0;0>:d       r3.3<0>:d        {I@3}
        add3 (1|M0)              r1.3<1>:d     r9.0<0;0>:d       r94.3<0;0>:d      r95.7<0>:d       {I@3}
        mov (1|M0)               r5.0<1>:d     r10.0<0;1,0>:d                  
        mov (1|M0)               r10.0<1>:ud   acc0.0<0;1,0>:ud                 {Compacted}
        send.ugm (1|M0)          r28      r5      null:0  0x0            0x0220D580           {A@2,$14} // wr:1+0, rd:2; load.ugm.d32x16t.a64
        mov (1|M0)               r12.0<1>:d    r8.0<0;1,0>:d                   
        add3 (1|M0)              r12.1<1>:d    r10.0<0;0>:d      r1.3<0;0>:d       r7.6<0>:d        {I@2}
        add (16|M0)              acc0.0<1>:f   r18.0<1;1,0>:f    r20.0<1;1,0>:f   {Compacted,$3.dst}
        send.ugm (1|M0)          r11      r12     null:0  0x0            0x04100580           {A@1,$15} // wr:2+0, rd:1; load.ugm.d32.a64
        add (16|M0)              acc0.0<1>:f   acc0.0<1;1,0>:f   r22.0<1;1,0>:f   {Compacted}
        add (16|M0)              r26.0<1>:f    acc0.0<1;1,0>:f   r24.0<1;1,0>:f   {Compacted}
        sync.nop                             null                             {Compacted,$14.src}
        addc (1|M0)              r5.0<1>:ud    r96.2<0;1,0>:ud   0x1:ud              {AccWrEn,A@1}
        mov (1|M0)               r1.2<1>:d     r5.0<0;1,0>:d                    {I@1}
        sync.nop                             null                             {Compacted,$14.dst}
        mad (16|M0)              acc2.0<1>:f   r28.0<1;0>:f      -r26.0<1;0>:f     r6.0<0>:f        {Compacted,$11.dst}
        mul (16|M0)              acc2.0<1>:f   acc2.0<1;1,0>:f   r11.0<0;1,0>:f   {Compacted,$15.dst}
        mul (16|M0)              r7.0<1>:f     r14.4<0;1,0>:f    acc2.0<1;1,0>:f 
        sync.nop                             null                             {Compacted,$2.src}
        mad (16|M0)              r38.0<1>:f    r7.0<1;0>:f       r38.0<1;0>:f      r6.0<0>:f        {Compacted,F@1}
        mul (16|M0)              r7.0<1>:f     r15.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r52.0<1>:f    r7.0<1;0>:f       r52.0<1;0>:f      r6.0<0>:f        {Compacted,F@1}
        mul (16|M0)              r7.0<1>:f     r16.2<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r66.0<1>:f    r7.0<1;0>:f       r66.0<1;0>:f      r6.0<0>:f        {Compacted,F@1}
        mul (16|M0)              r7.0<1>:f     r17.1<0;1,0>:f    acc2.0<1;1,0>:f 
        sync.nop                             null                             {Compacted,$4.src}
        mad (16|M0)              r80.0<1>:f    r7.0<1;0>:f       r80.0<1;0>:f      r6.0<0>:f        {Compacted,F@1}
        mov (1|M0)               r7.0<1>:ud    acc0.0<0;1,0>:ud                 {Compacted,F@1}
        mul (16|M0)              r18.0<1>:f    r14.0<0;1,0>:f    acc2.0<1;1,0>:f 
        add3 (1|M0)              r1.3<1>:d     r7.0<0;0>:d       r96.3<0;0>:d      0:w               {I@1}
        mul (16|M0)              r20.0<1>:f    r14.1<0;1,0>:f    acc2.0<1;1,0>:f 
        cmp (1|M0)    (eq)f0.1   null<1>:d     r1.3<0;1,0>:d     r3.5<0;1,0>:d    {I@1}
        mul (16|M0)              acc0.0<1>:f   r14.2<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r24.0<1>:f    r14.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r9.0<1>:f     r14.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r11.0<1>:f    r14.6<0;1,0>:f    acc2.0<1;1,0>:f 
        sync.nop                             null                             {Compacted,$7.src}
        mad (16|M0)              r30.0<1>:f    r18.0<1;0>:f      r30.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        sync.nop                             null                             {Compacted,$6.src}
        mad (16|M0)              r32.0<1>:f    r20.0<1;0>:f      r32.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r34.0<1>:f    acc0.0<1;0>:f     r34.0<1;0>:f      r6.0<0>:f        {Compacted,$5.src}
(f0.1)  cmp (1|M0)    (eq)f0.1   null<1>:d     r1.2<0;1,0>:d     r3.4<0;1,0>:d   
        mul (16|M0)              acc0.0<1>:f   r14.7<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r15.0<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r22.0<1>:f    r15.1<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r36.0<1>:f    r24.0<1;0>:f      r36.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        sync.nop                             null                             {Compacted,$1.src}
        mad (16|M0)              r40.0<1>:f    r9.0<1;0>:f       r40.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        sync.nop                             null                             {Compacted,$0.src}
        mad (16|M0)              r42.0<1>:f    r11.0<1;0>:f      r42.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r44.0<1>:f    acc0.0<1;0>:f     r44.0<1;0>:f      r6.0<0>:f        {Compacted}
        mad (16|M0)              r46.0<1>:f    r20.0<1;0>:f      r46.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r48.0<1>:f    r22.0<1;0>:f      r48.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mul (16|M0)              acc0.0<1>:f   r15.2<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r9.0<1>:f     r15.4<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r11.0<1>:f    r15.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r15.6<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r15.7<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r22.0<1>:f    r16.0<0;1,0>:f    acc2.0<1;1,0>:f 
(W)     and (1|M0)               f0.0<1>:uw    f0.1<0;1,0>:uw    0x1:uw             
        mad (16|M0)              r50.0<1>:f    acc0.0<1;0>:f     r50.0<1;0>:f      r6.0<0>:f        {Compacted}
        sync.nop                             null                             {Compacted,$8.src}
        mad (16|M0)              r54.0<1>:f    r9.0<1;0>:f       r54.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r56.0<1>:f    r11.0<1;0>:f      r56.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r58.0<1>:f    r18.0<1;0>:f      r58.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r60.0<1>:f    r20.0<1;0>:f      r60.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r62.0<1>:f    r22.0<1;0>:f      r62.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mul (16|M0)              acc0.0<1>:f   r16.1<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r9.0<1>:f     r16.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r11.0<1>:f    r16.4<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r16.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r16.6<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r22.0<1>:f    r16.7<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r64.0<1>:f    acc0.0<1;0>:f     r64.0<1;0>:f      r6.0<0>:f        {Compacted}
        mad (16|M0)              r68.0<1>:f    r9.0<1;0>:f       r68.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r70.0<1>:f    r11.0<1;0>:f      r70.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r72.0<1>:f    r18.0<1;0>:f      r72.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r74.0<1>:f    r20.0<1;0>:f      r74.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mad (16|M0)              r76.0<1>:f    r22.0<1;0>:f      r76.0<1;0>:f      r6.0<0>:f        {Compacted,F@6}
        mul (16|M0)              acc0.0<1>:f   r17.0<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r9.0<1>:f     r17.2<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r11.0<1>:f    r17.3<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r18.0<1>:f    r17.4<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r20.0<1>:f    r17.5<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              r22.0<1>:f    r17.6<0;1,0>:f    acc2.0<1;1,0>:f 
        mul (16|M0)              acc2.0<1>:f   r17.7<0;1,0>:f    acc2.0<1;1,0>:f 
        mad (16|M0)              r78.0<1>:f    acc0.0<1;0>:f     r78.0<1;0>:f      r6.0<0>:f        {Compacted}
        mad (16|M0)              r82.0<1>:f    r9.0<1;0>:f       r82.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r84.0<1>:f    r11.0<1;0>:f      r84.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r86.0<1>:f    r18.0<1;0>:f      r86.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r88.0<1>:f    r20.0<1;0>:f      r88.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r90.0<1>:f    r22.0<1;0>:f      r90.0<1;0>:f      r6.0<0>:f        {Compacted,F@7}
        mad (16|M0)              r92.0<1>:f    acc2.0<1;0>:f     r92.0<1;0>:f      r6.0<0>:f        {Compacted}
(~f0.0.any16h) goto (16|M0)                  L9560                  L9560                
L8976:
        send.ugm (1|M0)          null     r125    r30:2   0x0            0x0200C784           {$7} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r124    r32:2   0x0            0x0200C784           {$6} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r123    r34:2   0x0            0x0200C784           {$5} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r122    r36:2   0x0            0x0200C784           {$4} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r121    r38:2   0x0            0x0200C784           {$2} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r120    r40:2   0x0            0x0200C784           {$1} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r119    r42:2   0x0            0x0200C784           {$0} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r118    r44:2   0x0            0x0200C784           {$15} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r117    r46:2   0x0            0x0200C784           {$14} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r116    r48:2   0x0            0x0200C784           {$13} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r115    r50:2   0x0            0x0200C784           {$6} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r114    r52:2   0x0            0x0200C784           {$3} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r113    r54:2   0x0            0x0200C784           {$8} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r112    r56:2   0x0            0x0200C784           {$9} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r111    r58:2   0x0            0x0200C784           {$10} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r110    r60:2   0x0            0x0200C784           {$7} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r109    r62:2   0x0            0x0200C784           {$11} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r108    r64:2   0x0            0x0200C784           {$12} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r107    r66:2   0x0            0x0200C784           {$13} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r106    r68:2   0x0            0x0200C784           {$6} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r105    r70:2   0x0            0x0200C784           {$14} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r104    r72:2   0x0            0x0200C784           {$15} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r103    r74:2   0x0            0x0200C784           {$0} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r102    r76:2   0x0            0x0200C784           {$1} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@7}
        send.ugm (1|M0)          null     r101    r78:2   0x0            0x0200C784           {$2} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        send.ugm (1|M0)          null     r100    r80:2   0x0            0x0200C784           {$4} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@6}
        send.ugm (1|M0)          null     r99     r82:2   0x0            0x0200C784           {$5} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@5}
        send.ugm (1|M0)          null     r98     r84:2   0x0            0x0200C784           {$8} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@4}
        send.ugm (1|M0)          null     r97     r86:2   0x0            0x0200C784           {$6} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@3}
        send.ugm (1|M0)          null     r96     r88:2   0x0            0x0200C784           {$9} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@2}
        send.ugm (1|M0)          null     r95     r90:2   0x0            0x0200C784           {$7} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        sync.nop                             null                             {Compacted,F@1}
        send.ugm (1|M0)          null     r3      r92:2   0x0            0x0200C784           {$3} // wr:1+2, rd:0; store.ugm.d64x8t.a64
        goto (16|M0)                         L9560                  L9608                
L9560:
        join (16|M0)                         L9608                                
L9576:
(W)     mov (2|M0)               r96.2<1>:ud   r1.2<1;1,0>:ud                   {$9.src}
        goto.b (16|M0)                       L9608                  L6664                
L9608:
        join (16|M0)                         L9624                                
L9624:
(W)     mov (8|M0)               r127.0<1>:f   r0.0<1;1,0>:f                    {Compacted}
(W)     send.ugm (1|M0)          r5       r0      null:0  0x0            0x0210641F           {$5} // wr:1+0, rd:1; fence invalid flush type scoped to tile
(W)     mov (8|M0)               null<1>:ud    r5.0<1;1,0>:ud                   {$5.dst}
(W)     send.gtwy (1|M0)         null     r127    null:0  0x0            0x02000010           {EOT,A@1} // wr:1+0, rd:0; end of thread
L9680:
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
