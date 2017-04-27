# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(G22-buffer) begin
G22-buffer: exit(-1)
EOF
pass;