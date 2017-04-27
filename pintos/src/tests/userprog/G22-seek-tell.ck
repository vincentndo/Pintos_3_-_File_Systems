# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(G22-seek-tell) begin
(G22-seek-tell) create G22_file.txt
(G22-seek-tell) open G22_file.txt
(G22-seek-tell) write G22_file.txt
(G22-seek-tell) end of file
(G22-seek-tell) file pointer at position 12
(G22-seek-tell) rewind file pointer
(G22-seek-tell) file pointer at position 0
(G22-seek-tell) remove G22_file.txt
(G22-seek-tell) end
G22-seek-tell: exit(0)
EOF
pass;