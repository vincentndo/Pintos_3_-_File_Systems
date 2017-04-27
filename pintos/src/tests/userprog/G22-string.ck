# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(G22-string) begin
G22-string: exit(-1)
EOF
pass;