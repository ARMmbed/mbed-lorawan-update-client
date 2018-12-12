/*
 * PackageLicenseDeclared: Apache-2.0
 * Copyright (c) 2018 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "crypto/FragmentationEcdsaVerify.h"

#include "mbed_trace.h"
#define TRACE_GROUP "FECD"

FragmentationEcdsaVerify::FragmentationEcdsaVerify(const char* aPubKey, size_t aPubKeySize) :
    pubKey((const unsigned char*)aPubKey), pubKeySize(aPubKeySize)
{
}

bool FragmentationEcdsaVerify::verify(const unsigned char* hash, unsigned char* signature, size_t signature_size) {
    int ret;

    mbedtls_pk_init(&pk);

    ret = mbedtls_pk_parse_public_key(&pk, pubKey, pubKeySize);
    if (ret != 0) {
        tr_warn("ECDSA failed to parse public key (-0x%04x)", ret);
        return false;
    }

    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 0, signature, signature_size);
    if (ret != 0) {
        tr_debug("ECDSA failed to verify message (-0x%04x)", ret);
        return false;
    }

    mbedtls_pk_free(&pk);

    return ret == 0;
}
