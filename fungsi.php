<?php


function multivitamin(int $age, bool $man): int {
    $mask = 4;
    $copy = $mask;
    if ($man) {
        $age += $mask;
    }
    return $age + 90;
}

/* function fibonnaci(int $nth): int { */
/*     if ($n < 0) { */
/*         return -1; */
/*     } */
/*     if ($n === 0) { */
/*         return 0; */
/*     } */
/*     if ($n === 1) { */
/*         return 1; */
/*     } */
/*     return fibonacci($n - 1) + fibonacci($n - 2); */
/* } */

//hello();
