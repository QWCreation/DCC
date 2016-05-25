/* MD5.c : Defines the entry point for the console application.*/
/*
 *  After becoming frustrated with the lack of a standalone, portable,
 *  decent random number generator, I decided to make one based on a
 *  cryptographic one-way hash function.  I chose MD5 since it is fast
 *  and free source was readily available.  More cryptographically
 *  secure hash functions are available (e.g. SHA-1), but for the
 *  purposes of a rand/random/erand48 replacement, MD5 should be more
 *  than sufficient.
 *
 *  MD5 takes an arbitrary amount of input and yields a 16 byte hash.
 *  This RNG continually MD5's a 16 byte digest, and uses the bottom N
 *  bits as the random number yielded, where N is just large enough to
 *  include the largest random number desired.
 *
 *      To yield a random number between 0 and r:
 *
 *              create mask which has enough bits to include all of r
 *                      (for example, if r is 100, mask would be 0x7F)
 *
 *              do {
 *                      digest = MD5(digest)
 *                      number = digest & mask
 *              } while (number > r)
 *
 *  The digest should be loaded and saved to a disk file between
 *  invocations of a program using the RNG.
 *
 *  Random functions appear after the included MD5 code.
 *
 *  Send comments to:  skrenta@pbm.com (Rich Skrenta)
 */


/*****************************************************************/

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */


#include <stdio.h>
#include <stdlib.h>
#include "md5.h"

/*
 * Shuffle the bytes into little-endian order within words, as per the
 * MD5 spec.  Note: this code works regardless of the byte order.
 */


void byteSwap(word32 *buf, unsigned words)
{
	byte *p = (byte *)buf;

	do {
		*buf++ = (word32)((unsigned)p[3] << 8 | p[2]) << 16 |
			((unsigned)p[1] << 8 | p[0]);
		p += 4;
	} while (--words);
}


void bcopy(byte *src, byte *des, int len)
{
	int j;
	for(j=0;j<len;j++)	des[j]=src[j]; 
}

void bzero(byte *p,int len)
{
	int j;
	for(j=0;j<len;j++)	p[j]=0; 
}


/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void xMD5Init(struct xMD5Context *ctx)
{
	ctx->buf[0] = 0x67452301;
	ctx->buf[1] = 0xefcdab89;
	ctx->buf[2] = 0x98badcfe;
	ctx->buf[3] = 0x10325476;

	ctx->bytes[0] = 0;
	ctx->bytes[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void xMD5Update(struct xMD5Context *ctx, byte const *buf, int len)
{
	word32 t;

	/* Update byte count */

	t = ctx->bytes[0];
	if ((ctx->bytes[0] = t + len) < t)
		ctx->bytes[1]++;	/* Carry from low to high */

	t = 64 - (t & 0x3f);	/* Space available in ctx->in (at least 1) */
	if ((unsigned)t >(unsigned)len) {
		bcopy((byte *)buf, (byte *)ctx->in + 64 - (unsigned)t, len);
		return;
	}
	/* First chunk is an odd size */
	bcopy((byte *)buf,(byte *)ctx->in + 64 - (unsigned)t, (unsigned)t);
	byteSwap(ctx->in, 16);
	xMD5Transform(ctx->buf, ctx->in);
	buf += (unsigned)t;
	len -= (unsigned)t;

	/* Process data in 64-byte chunks */
	while (len >= 64) {
		bcopy((byte *)buf, (byte *)ctx->in, 64);
		byteSwap(ctx->in, 16);
		xMD5Transform(ctx->buf, ctx->in);
		buf += 64;
		len -= 64;
	}

	/* Handle any remaining bytes of data. */
	bcopy((byte *)buf, (byte *)ctx->in, len);
}


/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
void xMD5Final(byte digest[16], struct xMD5Context *ctx)
{
	int count = (int)(ctx->bytes[0] & 0x3f); /* Bytes in ctx->in */
	byte *p = (byte *)ctx->in + count;	/* First unused byte */

	/* Set the first char of padding to 0x80.  There is always room. */
	*p++ = 0x80;

	/* Bytes of padding needed to make 56 bytes (-8..55) */
	count = 56 - 1 - count;

	if (count < 0) {	/* Padding forces an extra block */
		bzero(p, count+8);
		byteSwap(ctx->in, 16);
		xMD5Transform(ctx->buf, ctx->in);
		p = (byte *)ctx->in;
		count = 56;
	}
	bzero(p, count+8);
	byteSwap(ctx->in, 14);

	/* Append length in bits and transform */
	ctx->in[14] = ctx->bytes[0] << 3;
	ctx->in[15] = ctx->bytes[1] << 3 | ctx->bytes[0] >> 29;
	xMD5Transform(ctx->buf, ctx->in);

	byteSwap(ctx->buf, 4);
	bcopy((byte *)ctx->buf, (byte *)digest, 16);
	bzero((byte *)ctx,sizeof(ctx));
}




/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
void xMD5Transform(word32 buf[4], word32 const in[16])
{
	register word32 a, b, c, d;

	a = buf[0];
	b = buf[1];
	c = buf[2];
	d = buf[3];

	MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
	MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
	MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
	MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
	MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
	MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
	MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
	MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
	MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
	MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
	MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
	MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
	MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
	MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
	MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
	MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

	MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
	MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
	MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
	MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
	MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
	MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
	MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
	MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
	MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
	MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
	MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
	MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
	MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
	MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
	MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
	MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

	MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
	MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
	MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
	MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
	MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
	MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
	MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
	MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
	MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
	MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
	MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
	MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
	MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
	MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
	MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
	MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

	MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
	MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
	MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
	MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
	MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
	MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
	MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
	MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
	MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
	MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
	MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
	MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
	MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
	MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
	MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
	MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}


void MD5(void *dest, void *orig, int len)
{
	struct xMD5Context context;

	xMD5Init(&context);
	xMD5Update(&context,(byte*)orig, len);
	xMD5Final((byte*)dest, &context);
}
#if 0
#define MD5_BUF_LEN 4096

static unsigned char md5_buf[MD5_BUF_LEN];

int main(int argc, char **argv)
{
	size_t len;
	int ret;
	int i;
	FILE *src_file = NULL;
	byte dest[16];
	struct xMD5Context context;

	if (argc < 2) {
		fprintf(stderr,
			"Usage: %s file\n",
			argv[0]);
		goto EXIT;
	}

	if ((src_file = fopen(argv[1], "r")) == NULL) {
		perror("open");
		goto EXIT;
	}
	
	xMD5Init(&context);
	while ((len = fread(md5_buf, 1, MD5_BUF_LEN, src_file))) {
		xMD5Update(&context,(byte*)md5_buf, len);
	}
	
	xMD5Final((byte*)dest, &context);
	printf("MD5 sum:\n");
	for (i = 0; i < 16; i ++)
		printf("%02x", dest[i]);
	printf("\n");
	ret = 0;
EXIT:
	if (src_file)
		fclose(src_file);

	return ret;
}
#endif

