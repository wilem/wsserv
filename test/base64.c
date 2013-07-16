#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

/*
  0 A            17 R            34 i            51 z
  1 B            18 S            35 j            52 0
  2 C            19 T            36 k            53 1
  3 D            20 U            37 l            54 2
  4 E            21 V            38 m            55 3
  5 F            22 W            39 n            56 4
  6 G            23 X            40 o            57 5
  7 H            24 Y            41 p            58 6
  8 I            25 Z            42 q            59 7
  9 J            26 a            43 r            60 8
  10 K            27 b            44 s            61 9
  11 L            28 c            45 t            62 +
  12 M            29 d            46 u            63 /
  13 N            30 e            47 v
  14 O            31 f            48 w         (pad) =
  15 P            32 g            49 x
  16 Q            33 h            50 y
 */

static char tab[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q',
    'R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f','g','h',
    'i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y',
    'z','0','1','2','3','4','5','6','7','8','9','+','/'
};

static char pad = '=';

char *hello = "hello\n";

// 3 bytes/4 chars
struct bit24group {
    u_char ch0    : 6;
    u_char ch1_01 : 2;
    u_char ch1_25 : 4;
    u_char ch2_03 : 4;
    u_char ch2_45 : 2;
    u_char ch3    : 6;
}__attribute__ ((__packed__));

char *
convert2base64(void *mem, size_t len, size_t *olen)
{
  size_t quo = len / 3;
  size_t rem = len % 3;
  size_t siz = quo;
  if (rem > 0)
      siz++;
  struct bit24group *grp = calloc(siz, sizeof(struct bit24group));
  memcpy(grp, mem, siz * 3);
  *olen = siz * 4;
  char *out = (char *)calloc(1, siz * 4 + 1); // one for '\0'
  int i, j = 0;
  u_char ch0, ch1, ch2, ch3;
  for (i = 0; i < siz; i++) {
      ch0 = grp[i].ch0;
      out[j++] = tab[ch0];
      printf("o[%d]: %c idx: %d\n", j, out[j - 1], ch0);

      ch1 = grp[i].ch1_25 * 4 + grp[i].ch1_01; //<< 2
      out[j++] = tab[ch1];
      printf("o[%d]: %c idx: %d\n", j, out[j - 1], ch1);

      ch2 = grp[i].ch2_45 * 16 + grp[i].ch2_03; //<< 4
      out[j++] = tab[ch2];
      printf("o[%d]: %c idx: %d\n", j, out[j - 1], ch2);

      ch3 = grp[i].ch3;
      out[j++] = tab[ch3];
      printf("o[%d]: %c idx: %d\n", j, out[j - 1], ch3);
  }
  
  return out;
}

int
main(int c, char **v)
{
    size_t ol;
    char *o = convert2base64(hello, 6, &ol);

    printf("size of grp: %d\n", sizeof(struct bit24group));
    printf("str[%u]: \"%s\"\n", ol, o);
    return 0;
}
