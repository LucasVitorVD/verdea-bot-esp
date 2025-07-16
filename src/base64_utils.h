#ifndef BASE64_UTILS_H
#define BASE64_UTILS_H

int base64_encode(char *output, char *input, int inputLen);
int base64_decode(char *output, char *input, int inputLen);
int base64_enc_len(int plainLen);
int base64_dec_len(char * input, int inputLen);


#endif