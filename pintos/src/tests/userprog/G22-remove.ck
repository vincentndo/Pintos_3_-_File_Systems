# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(G22-remove) begin
(G22-remove) create G22_file_1.txt
(G22-remove) remove G22_file_1.txt
(G22-remove) create G22_file_2.txt
(G22-remove) open G22_file_2.txt
(G22-remove) remove G22_file_2.txt
(G22-remove) still write successfully after remove
(G22-remove) end
G22-remove: exit(0)
EOF
pass;