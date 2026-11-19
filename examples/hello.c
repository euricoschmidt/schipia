#include <schipbase.h>
#include <stdio.h>
int main(void) { Tbuf *b = Tbuf_new(0); if (!b) return 1; Tbuf_close(b); puts("Schip: SDK pronto"); return 0; }
