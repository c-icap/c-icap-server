{
    n=match($0, /^[[:digit:]]+\.[[:digit:]]+\.[[:digit:]]+$/);
    if (n==0) {
        t=gsub(/[^[:digit:]]/, "", $0);
        printf("0xF%0.11X", $0);
    } else {
        split($0, a, ".");
        printf("0x%0.4X%0.4X%0.4X", a[1], a[2], a[3]);
    }
}
