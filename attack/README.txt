This file contains materials for one instance of the attacklab. Read the attacklab.pdf for more instructions

Files:

    ctarget

Linux binary with code-injection vulnerability.  To be used for phases
1-3 of the assignment.

    rtarget

Linux binary with return-oriented programming vulnerability.  To be
used for phases 4-5 of the assignment.

     cookie.txt

Text file containing 4-byte signature required for this lab instance.

     farm.c

Source code for gadget farm present in this instance of rtarget.  You
can compile (use flag -Og) and disassemble it to look for gadgets.

     hex2raw

Utility program to generate byte sequences.  See documentation in lab
handout.


## Solutions
The solutions are created in 5 txt files for the 5 stages of attack.
ctarget_lx.txt represents the code-injection attack for the first 3 stages.
rtarget_lx.txt represents the return-oriented programming attack for the last 2 stages.
