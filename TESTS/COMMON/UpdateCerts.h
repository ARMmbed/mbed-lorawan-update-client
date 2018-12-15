#ifndef _UPDATE_CERTS_H
#define _UPDATE_CERTS_H

const char * UPDATE_CERT_PUBKEY = "-----BEGIN PUBLIC KEY-----\nMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEL7s1CjGyLrjakn8ZlPJ16RKJvN1A\nWTbVl0mDVwouv8k8k4RPjRChDGFyiF79RPHl3nOdJZoPFaxvESB55QKESQ==\n-----END PUBLIC KEY-----\n";
const size_t UPDATE_CERT_LENGTH = 179;

const uint8_t UPDATE_CERT_MANUFACTURER_UUID[16] = { 0x35, 0xa4, 0x66, 0xb8, 0x8b, 0x16, 0x50, 0x77, 0xaf, 0x86, 0x47, 0x7a, 0x5c, 0x23, 0xe8, 0xca };
const uint8_t UPDATE_CERT_DEVICE_CLASS_UUID[16] = { 0xa3, 0xa5, 0xdf, 0xb0, 0x5e, 0x97, 0x5a, 0x58, 0xb1, 0xe3, 0xb4, 0x1d, 0x21, 0x12, 0xe5, 0x85 };

#endif // _UPDATE_CERTS_H_