#ifndef CRYPTO_H_
#define CRYPTO_H_
#pragma once

#include <string>
#include <fstream>

// Depricated
#include <hex.h>         
#include <base32.h>
#include <base64.h>
// Depricated //

#include <sodium.h>

#include "errorcodes.h"
#include "credentials.h"
#include "utils.h"
#include "fileroller.h"

extern "C"
{
    #include "crypto_scrypt.h"
    int crypto_scrypt( const uint8_t *,
                       size_t,
                       const uint8_t *,
                       size_t,
                       uint64_t,
                       uint32_t,
                       uint32_t,
                       uint8_t *,
                       size_t);
}

namespace attic { namespace crypto {

#include <sys/mman.h>
static void m_lock(void* p, size_t len) {
    mlock(p, len);
}

// new
static bool ScryptEncode(const std::string &input, 
                         const std::string &salt,
                         const unsigned int size,
                         std::string &out);

static bool GenerateHash(const std::string& source, std::string& hash_out);
static void GenerateKey(std::string& out);
static void GenerateIv(std::string& out);
static void GenerateNonce(std::string& out);
static bool Encrypt(const std::string& in, const Credentials& cred, std::string& out);
static bool Decrypt(const std::string& in, const Credentials& cred, std::string& out);
static int EnterPassphrase(const std::string& pass, const std::string& iv, Credentials& out);
static int GenerateKeyFromPassphrase(const std::string& pass, Credentials& out);
static Credentials GenerateCredentials();
static void GenerateCredentials(Credentials& cred);
static bool GeneratePlaintextHashForFile(const std::string& filepath, std::string& hash_out);
static void GeneratePublicAndPrivateKey(std::string& public_out, std::string& private_out);

// Old
static void GenerateFileHash(const std::string& filepath, std::string& hash_out);
static void GenerateRandomString(std::string& out, const unsigned int size);

static void HexEncodeString(const std::string& input, std::string& output) {
    CryptoPP::StringSource(input,
                           true,
                           new CryptoPP::HexEncoder(
                             new CryptoPP::StringSink(output),
                             false // Caps
                           ) // HexEncoder
                          ); // StringSource
}

static void HexDecodeString(const std::string& input, std::string& output) {
    CryptoPP::StringSource(input,
                           true,
                           new CryptoPP::HexDecoder(
                               new CryptoPP::StringSink(output)
                           ) // HexDecoder
                          ); // StringSource
}

static void Base64EncodeString(const std::string& input, std::string& output) {
    CryptoPP::StringSource(input, 
                           true, 
                           new CryptoPP::Base64Encoder(new CryptoPP::StringSink(output), 
                                                       false) // Insert line breaks false
                          );
}

static void Base64DecodeString(const std::string& input, std::string& output) {
    CryptoPP::StringSource( input,
                            true,
                            new CryptoPP::Base64Decoder(new CryptoPP::StringSink(output)));
}

static void Base32EncodeString(const std::string& input, std::string& output) {
    CryptoPP::StringSource(input,
                           true,
                           new CryptoPP::Base32Encoder(new CryptoPP::StringSink(output))
                          );
}

static void Base32DecodeString(const std::string& input, std::string& output) {
    CryptoPP::StringSource(input,
                           true,
                           new CryptoPP::Base32Decoder(new CryptoPP::StringSink(output)));
}

// New NACL STUFF
//
//
//
//
static bool GenerateHmacSha256(const std::string& source, std::string& hash_out) {
    unsigned char out[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(out, 
                       reinterpret_cast<const unsigned char*>(source.c_str()),
                       source.size());
    std::string ver;
    ver.append(reinterpret_cast<const char*>(out), crypto_hash_sha256_BYTES);
    Base64EncodeString(ver, hash_out);
    return true;

}

static bool GenerateHash(const std::string& source, std::string& hash_out) {
    unsigned char out[crypto_hash_sha512_BYTES];
    crypto_hash_sha512(out, 
                       reinterpret_cast<const unsigned char*>(source.c_str()),
                       source.size());
    std::string ver;
    ver.append(reinterpret_cast<const char*>(out), crypto_hash_sha512_BYTES);
    Base64EncodeString(ver, hash_out);
    return true;
}

static bool GenerateHexEncodedHmac(const std::string& source, std::string& hash_out) {
    unsigned char out[crypto_hash_sha512_BYTES];
    crypto_hash_sha512(out, 
                       reinterpret_cast<const unsigned char*>(source.c_str()),
                       source.size());
    std::string ver;
    ver.append(reinterpret_cast<const char*>(out), crypto_hash_sha512_BYTES);
    HexEncodeString(ver, hash_out);
    return true;
}

static Credentials GenerateCredentials() {
    Credentials cred;
    GenerateCredentials(cred);
    return cred;
}

static bool GeneratePlaintextHashForFile(const std::string& filepath, std::string& hash_out) {
    bool ret = false;
    FileRoller fr(filepath);
    std::string hash;
    ret = fr.Digest(hash);
    if(ret)
        Base64EncodeString(hash, hash_out);
    return ret;
}

static void GenerateCredentials(Credentials& cred) {
    std::string key, iv;
    GenerateIv(iv);
    GenerateKey(key);
    cred.set_key(key);
    cred.set_iv(iv);
}

static void GeneratePublicAndPrivateKey(std::string& public_out, std::string& private_out) {
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];

    crypto_box_keypair(public_key, secret_key);

    public_out.append(reinterpret_cast<const char*>(public_key), crypto_box_PUBLICKEYBYTES);
    private_out.append(reinterpret_cast<const char*>(secret_key), crypto_box_SECRETKEYBYTES);
}

static bool EncryptStringWithPublicKey(const std::string& data_in,
                                       const std::string& recipient_public_key,
                                       const std::string& sender_private_key,
                                       std::string& nonce_out,
                                       std::string& out) {
    bool ret = false;

    std::string buffer;
    char zerb[32]; // Add 32 byte zero'd to buffer
    memset(zerb, 0, 32);
    buffer.append(zerb, 32);
    // avoid cow
    buffer.append(data_in.c_str(), data_in.size());
    unsigned char* c = new unsigned char[buffer.size()]; // ciphertext

    unsigned char nonce[crypto_box_NONCEBYTES];
    randombytes(nonce, crypto_box_NONCEBYTES);

    if(crypto_box(c,
                  reinterpret_cast<const unsigned char*>(buffer.c_str()),
                  buffer.size(),
                  nonce,
                  reinterpret_cast<const unsigned char*>(recipient_public_key.c_str()),
                  reinterpret_cast<const unsigned char*>(sender_private_key.c_str())) == 0) {
        out.append(reinterpret_cast<const char*>(c), buffer.size());
        nonce_out.append(reinterpret_cast<const char*>(nonce), crypto_box_NONCEBYTES);
        ret = true;
    }

    if(c) {
        delete[] c;
        c = NULL;
    }

    return ret;
}

static bool DecryptStringWithPrivateKey(const std::string& cipher_text,
                                        const std::string& sender_public_key,
                                        const std::string& recipient_private_key,
                                        const std::string& nonce,
                                        std::string& out) {
    bool ret = false;

    unsigned char* m = new unsigned char[cipher_text.size()];

    if(crypto_box_open(m,
                       reinterpret_cast<const unsigned char*>(cipher_text.c_str()),
                       cipher_text.size(),
                       reinterpret_cast<const unsigned char*>(nonce.c_str()),
                       reinterpret_cast<const unsigned char*>(sender_public_key.c_str()),
                       reinterpret_cast<const unsigned char*>(recipient_private_key.c_str())) == 0) {
        out.append(reinterpret_cast<const char*>(m), cipher_text.size());
        ret = true;
    }

    if(m) {
        delete[] m;
        m = NULL;
    }

    return ret;
}

static bool Encrypt(const std::string& in, const Credentials& cred, std::string& out) {
    bool ret = false;
    std::string buffer;
    // append buffer, (required by nacl)
    buffer.append(32,0);
    // append data
    buffer.append(in.c_str(), in.size());
    // ciphertext
    unsigned char* c = new unsigned char[buffer.size()];
    if(crypto_secretbox(c,  
                        reinterpret_cast<const unsigned char*>(buffer.c_str()),
                        buffer.size(),
                        cred.byte_iv(),
                        cred.byte_key()) == 0) {
        out.append(reinterpret_cast<const char*>(c), buffer.size());
        ret = true;
    }

    if(c) {
        delete c;
        c = NULL;
    }
    return ret;
}

static bool Decrypt(const std::string& in, const Credentials& cred, std::string& out) {
    bool ret = false;
    if(in.size()) {
        unsigned char* m = new unsigned char[in.size()];
        if(crypto_secretbox_open(m, 
                                 reinterpret_cast<const unsigned char*>(in.c_str()),
                                 in.size(),
                                 cred.byte_iv(),
                                 cred.byte_key()) == 0) {
            out.append(reinterpret_cast<const char*>(m), in.size());
            // remove padding
            out.erase(0, 32);
            ret = true;
        }

        if(m) {
            delete m;
            m = NULL;
        }
    }
    return ret;
}

static void GenerateKey(std::string& out) {
    unsigned char key[crypto_secretbox_KEYBYTES];
    randombytes(key, crypto_secretbox_KEYBYTES);
    out.append(reinterpret_cast<const char*>(key), crypto_secretbox_KEYBYTES);
}

static void GenerateNonce(std::string& out) {
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes(nonce, crypto_secretbox_NONCEBYTES);
    out.append(reinterpret_cast<const char*>(nonce), crypto_secretbox_NONCEBYTES);
}

static void GenerateIv(std::string& out) {
    GenerateNonce(out);
}

static int EnterPassphrase(const std::string& pass, const std::string& iv, Credentials& out) {
    int status = ret::A_OK;
    std::string key;
    ScryptEncode(pass, iv, out.GetKeySize(), key);
    // Set key
    status = out.set_key(key);
    if(status == ret::A_OK) {
        // Set iv
        status = out.set_iv(iv);
    }
    return status;
}

static int GenerateKeyFromPassphrase(const std::string& pass, Credentials& out) {
    int status = ret::A_OK;
    std::string key, salt;
    GenerateNonce(salt);
    status = EnterPassphrase(pass, salt, out);
    return status;
}

static bool ScryptEncode(const std::string &input, 
                         const std::string &salt,
                         const unsigned int size,
                         std::string &out) {
    // Note* pass in 16 bit salt
    //uint8_t salt[32]; // 16 <- do 16, 64 or 128

    uint8_t* password;
    size_t plen;

    uint64_t N = 16384;
    uint32_t r = 8;
    uint32_t p = 1;

    //uint8_t dk[64]; // Derived key
    uint8_t dk[size]; // Derived key

    byte* pInput = new byte[input.size()];
    memcpy(pInput, reinterpret_cast<const unsigned char*>(input.c_str()), input.size());

    byte* pSalt = new byte[salt.size()];
    memcpy(pSalt, reinterpret_cast<const unsigned char*>(salt.c_str()), salt.size());

    crypto_scrypt( pInput,
                   input.size(),
                   pSalt,
                   salt.size(),
                   N,
                   r,
                   p,
                   dk,
                   size);
    out.append(reinterpret_cast<char*>(dk), sizeof(uint8_t)*size);

    if(pInput) {
        delete[] pInput;
        pInput = NULL;
    }

    if(pSalt) {
        delete[] pSalt;
        pSalt = NULL;
    }

    return true;
}

static void SecureZeroMemory(void * const p, const size_t len) {
    sodium_memzero(p, len);
}

}} //namespace
#endif

