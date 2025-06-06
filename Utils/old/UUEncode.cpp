﻿

#include "UUEncode.h"


/*
一、编码规则
    Base64编码的思想是是采用64个基本的ASCII码字符对数据进行重新编码。
    它将需要编码的数据拆分成字节数组。以3个字节为一组。按顺序排列24位数据，
    再把这24位数据分成4组，即每组6位。再在每组的的最高位前补两个0凑足一个字
    节。这样就把一个3 字节为一组的数据重新编码成了4个字节。当所要编码的数据
    的字节数不是3的整倍数，也就是说在分组时最后一组不够3 个字节。这时在最后
    一组填充1到2个0字节。并在最后编码完成后在结尾添加1到2个“=”。
    例：将对ABC进行BASE64编码
    首先取ABC对应的ASCII码值。A（65）B（66）C（67）。
    再取二进制值A（01000001）B（01000010）C（01000011），然后把这三个字节的
    二进制码接起来（010000010100001001000011），再以6位为单位分成4个数据块
    并在最高位填充两个0后形成4个字节的编码后的值（00010000）（00010100）
    （00001001）（00000011）。蓝色部分为真实数据。再把这四个字节数据转化成10
    进制数得（16）（20）（19）（3）。最后根据BASE64给出的64个基本字符表，查出
    对应的ASCII码字符（Q）（U）（J）（D）。这里的值实际就是数据在字符表中的索引。
    注BASE64字符表：ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/
二、解码规则
    解码过程就是把4个字节再还原成3个字节再根据不同的数据形式把字节数组重新整理成数据。
*/

/* The two currently defined translation tables.  The first is the
   standard uuencoding, the second is base64 encoding.  */
static const char uu_std[64] = {
  '`', '!', '"', '#', '$', '%', '&', '\'',
  '(', ')', '*', '+', ',', '-', '.', '/',
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', ':', ';', '<', '=', '>', '?',
  '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
  'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
  'X', 'Y', 'Z', '[', '\\', ']', '^', '_'
};

static const char uu_base64[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', '+', '/'
};

const char *getBase64Set() {
    return uu_base64;
}

/* Pointer to the translation table we currently use.  */
const char *trans_ptr = uu_base64;

/* ENC is the basic 1 character encoding function to make a char printing.  */
#define ENC(Char)           (trans_ptr[(Char) & 077])

void encodeuu(const char *strIn, int nLen, string &str, bool bBase64, bool bIncNewLine) {
    int n;
    const char *p, *p2;

    if (bBase64) {
        trans_ptr = uu_base64;
    } else {
        trans_ptr = uu_std;
    }

    assert(nLen >= 0);

    str.clear();

    for (p2 = strIn; nLen >= 0; nLen-= 45, p2 += 45) {
        if (nLen > 45) {
            n = 45;
        } else {
            n = nLen;
        }
        if (trans_ptr == uu_std) {
            str += ENC (n);
        }
        // putchar (ENC (n));

        for (p = p2; n > 2; n -= 3, p += 3) {
            str += ENC (*p >> 2);
            str += ENC (((*p << 4) & 060) | ((p[1] >> 4) & 017));
            str += ENC (((p[1] << 2) & 074) | ((p[2] >> 6) & 03));
            str += ENC (p[2] & 077);
            //            try_putchar (ENC (*p >> 2));
            //            try_putchar (ENC (((*p << 4) & 060) | ((p[1] >> 4) & 017)));
            //            try_putchar (ENC (((p[1] << 2) & 074) | ((p[2] >> 6) & 03)));
            //            try_putchar (ENC (p[2] & 077));
        }

        if (nLen >= 45 && bIncNewLine) {
            str += '\n';
        }
    }

    if (n > 0)  /* encode the last one or two chars */ {
        char tail = trans_ptr == uu_std ? ENC ('\0') : '=';
        char p1;

        if (n == 1) {
            p1 = '\0';
        } else {
            p1 = p[1];
        }

        str += ENC (*p >> 2);
        str += ENC (((*p << 4) & 060) | ((p1 >> 4) & 017));
        str += n == 1 ? tail : ENC ((p1 << 2) & 074);
        str += tail;
        //            try_putchar (ENC (*p >> 2));
        //            try_putchar (ENC (((*p << 4) & 060) | ((p[1] >> 4) & 017)));
        //            try_putchar (n == 1 ? tail : ENC ((p[1] << 2) & 074));
        //            try_putchar (tail);
        //        str += '\n';
        // try_putchar ('\n');
    }

    if (trans_ptr == uu_std) {
        str += ENC ('\0');
        //        str += '\n';
        //        try_putchar (ENC ('\0'));
        //        try_putchar ('\n');
    }
}

bool decodebase64(cstr_t szIn, int nLen, string &strOut) {
    static const char b64_tab[256] = {
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*000-007*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*010-017*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*020-027*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*030-037*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*040-047*/
        '\177', '\177', '\177', '\76',  '\177', '\177', '\177', '\77',  /*050-057*/
        '\64',  '\65',  '\66',  '\67',  '\70',  '\71',  '\72',  '\73',  /*060-067*/
        '\74',  '\75',  '\177', '\177', '\177', '\100', '\177', '\177', /*070-077*/
        '\177', '\0',   '\1',   '\2',   '\3',   '\4',   '\5',   '\6',   /*100-107*/
        '\7',   '\10',  '\11',  '\12',  '\13',  '\14',  '\15',  '\16',  /*110-117*/
        '\17',  '\20',  '\21',  '\22',  '\23',  '\24',  '\25',  '\26',  /*120-127*/
        '\27',  '\30',  '\31',  '\177', '\177', '\177', '\177', '\177', /*130-137*/
        '\177', '\32',  '\33',  '\34',  '\35',  '\36',  '\37',  '\40',  /*140-147*/
        '\41',  '\42',  '\43',  '\44',  '\45',  '\46',  '\47',  '\50',  /*150-157*/
        '\51',  '\52',  '\53',  '\54',  '\55',  '\56',  '\57',  '\60',  /*160-167*/
        '\61',  '\62',  '\63',  '\177', '\177', '\177', '\177', '\177', /*170-177*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*200-207*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*210-217*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*220-227*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*230-237*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*240-247*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*250-257*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*260-267*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*270-277*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*300-307*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*310-317*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*320-327*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*330-337*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*340-347*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*350-357*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*360-367*/
        '\177', '\177', '\177', '\177', '\177', '\177', '\177', '\177', /*370-377*/
    };

    if (nLen < 0) {
        nLen = strlen(szIn);
    }

    strOut.clear();

#ifdef UNICODE
    WCHAR *p, *pEnd;
    p = (WCHAR *)szIn;
#else
    unsigned char *p, *pEnd;
    p = (unsigned char *)szIn;
#endif
    pEnd = p + nLen;

    unsigned char b1, b2, b3, b4;

    while (p < pEnd) {
        while (p < pEnd && b64_tab[*p] == '\177') {
            p++;
        }
        if (p > pEnd) {
            break;
        }
        b1 = b64_tab[*p];
        p++;

        while (p < pEnd && b64_tab[*p] == '\177') {
            p++;
        }
        if (p > pEnd) {
            break;
        }
        b2 = b64_tab[*p];
        p++;

        while (p < pEnd && b64_tab[*p] == '\177') {
            p++;
        }
        if (p > pEnd) {
            break;
        }
        b3 = b64_tab[*p];
        p++;

        while (p < pEnd && b64_tab[*p] == '\177') {
            p++;
        }
        if (p > pEnd) {
            break;
        }
        b4 = b64_tab[*p];
        p++;

        strOut += b1 << 2 | b2 >> 4;
        if (b3 != '\100') {
            strOut += b2 << 4 | b3 >> 2;
            if (b4 != '\100') {
                strOut += b3 << 6 | b4;
            }
        }
    }

    return true;
}

#define    DEC(Char)        (((Char) - ' ') & 077)

bool decodestduu(cstr_t szIn, int nLen, string &strOut) {
    // while (1)
    strOut.clear();

    {
        int n;
        cstr_t p = szIn;
        cstr_t pEnd = szIn + nLen;
        for (p = szIn; p < pEnd;) {
            /* N is used to avoid writing out all the characters at the end of
            the file.  */

            if (*p == '\n') {
                p++;
            }
            n = DEC (*p);
            if (n <= 0) {
                return true;
            }

            for (++p; n >= 3; p += 4, n -= 3) {
                strOut += DEC(p[0]) << 2 | DEC (p[1]) >> 4;
                strOut += DEC (p[1]) << 4 | DEC (p[2]) >> 2;
                strOut += DEC (p[2]) << 6 | DEC (p[3]);
                //            TRY_PUTCHAR (DEC (p[0]) << 2 | DEC (p[1]) >> 4);
                //            TRY_PUTCHAR (DEC (p[1]) << 4 | DEC (p[2]) >> 2);
                //            TRY_PUTCHAR (DEC (p[2]) << 6 | DEC (p[3]));
            }
            if (n > 0) {
                strOut += DEC (p[0]) << 2 | DEC (p[1]) >> 4;
                // TRY_PUTCHAR (DEC (p[0]) << 2 | DEC (p[1]) >> 4);
                if (n >= 2) {
                    strOut += DEC (p[1]) << 4 | DEC (p[2]) >> 2;
                }
                // TRY_PUTCHAR (DEC (p[1]) << 4 | DEC (p[2]) >> 2);
            }
        }
    }

    //    if (fgets (buf, sizeof(buf), stdin) == nullptr
    //        || strcmp (buf, "end\n"))
    //    {
    //        error (0, 0, _("%s: No `end' line"), inname);
    //        return 1;
    //    }

    return true;
}
