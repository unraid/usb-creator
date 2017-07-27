/*
 * Copyright (C) Eric Schultz
 * Copyright (C) Lime Technology, Inc.
 */


#ifndef SHA256CRYPT_H_
#define SHA256CRYPT_H_

#ifdef __cplusplus
extern "C" {
#endif

char *
sha256_crypt (const char *key, const char *salt);

#ifdef __cplusplus
}
#endif

#endif /* SHA256CRYPT_H_ */
