#ifndef OPENSSL_CH
#define OPENSSL_CH

/* AesEncrypt() and AesDecrypt() modes; the key length (16, 24 or 32 bytes) selects AES-128/192/256 */
#define AES_MODE_CBC               1  /* PKCS#7 padding, 16 byte IV (default) */
#define AES_MODE_CTR               2  /* no padding, 16 byte IV */
#define AES_MODE_GCM               3  /* no padding, 12 byte IV, 16 byte tag after the data */
#define AES_MODE_ECB               4  /* PKCS#7 padding, no IV */

/* RsaEncrypt() and RsaDecrypt() paddings */
#define RSA_PADDING_OAEP_SHA256    1  /* OAEP with SHA-256 and MGF1 SHA-256 (default) */
#define RSA_PADDING_OAEP_SHA1      2  /* OAEP with SHA-1 */
#define RSA_PADDING_PKCS1          3  /* PKCS#1 v1.5 */

/* SignData() and VerifyData() paddings for RSA keys */
#define RSA_PADDING_PSS            4  /* PSS, salt as long as the digest */

#endif
